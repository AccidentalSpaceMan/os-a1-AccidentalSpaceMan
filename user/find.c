#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// The user stack is a single 4 KB page and a process may only have
// NOFILE open files, so deep trees need care: the path lives in one
// global buffer, the walk uses an explicit stack instead of recursion,
// and each directory is closed before we descend into a child (then
// reopened and fast-forwarded when we come back).
static char path[MAXPATH];
static char *pattern;
static char type;          // 0 = anything, 'f' = files only, 'd' = dirs only

// Scratch space used between recursive calls, kept off the stack.
static struct dirent de;
static struct stat st;
static char name[DIRSIZ + 1];

// Does name match pat? '*' matches any sequence (including empty);
// every other character must match exactly.
int
match(char *pat, char *s)
{
  if(*pat == '\0')
    return *s == '\0';
  if(*pat == '*')
    return match(pat + 1, s) || (*s != '\0' && match(pat, s + 1));
  return *s == *pat && match(pat + 1, s + 1);
}

static int
wanted(void)
{
  if(type == 'd')
    return st.type == T_DIR;
  if(type == 'f')
    return st.type == T_FILE || st.type == T_DEVICE;
  return 1;
}

// Explicit stack for the walk, one slot per directory level. Recursion
// overflows the one-page user stack in deep trees. Every level adds at
// least two characters ("/x"), so MAXPATH/2 levels is enough.
static int lens[MAXPATH / 2 + 1];   // strlen(path) at this level
static int dones[MAXPATH / 2 + 1];  // directory entries already handled

// Walk the directory in path, printing every matching entry below it.
void
find(void)
{
  int fd, i, len, nlen, sub, depth;

  depth = 0;
  lens[0] = strlen(path);
  dones[0] = 0;
  while(depth >= 0){
    len = lens[depth];
    path[len] = '\0';
    if((fd = open(path, O_RDONLY)) < 0){
      fprintf(2, "find: cannot open %s\n", path);
      depth--;
      continue;
    }
    // Skip the entries handled before we descended into a child.
    for(i = 0; i < dones[depth]; i++)
      if(read(fd, &de, sizeof(de)) != sizeof(de))
        break;

    sub = 0;
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      dones[depth]++;
      if(de.inum == 0)
        continue;
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = '\0';
      if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        continue;

      // Build path/name, without doubling a trailing '/'.
      nlen = len;
      if(path[nlen - 1] != '/')
        nlen++;
      if(nlen + strlen(name) >= MAXPATH){
        fprintf(2, "find: path too long: %s/%s\n", path, name);
        continue;
      }
      path[len] = '/';
      strcpy(path + nlen, name);
      nlen += strlen(name);

      if(stat(path, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        path[len] = '\0';
        continue;
      }
      if(match(pattern, name) && wanted())
        printf("%s\n", path);
      if(st.type == T_DIR){
        sub = 1;
        break;
      }
      path[len] = '\0';
    }
    // Close before descending, so only one directory is ever open.
    close(fd);

    if(sub){
      depth++;
      lens[depth] = nlen;
      dones[depth] = 0;
    } else {
      depth--;
    }
  }
}

static void
usage(void)
{
  fprintf(2, "usage: find <dir> <pattern> [-type f|d]\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  int fd;

  if(argc == 5){
    if(strcmp(argv[3], "-type") != 0)
      usage();
    if(strcmp(argv[4], "f") == 0)
      type = 'f';
    else if(strcmp(argv[4], "d") == 0)
      type = 'd';
    else
      usage();
  } else if(argc != 3){
    usage();
  }
  pattern = argv[2];

  if(strlen(argv[1]) == 0 || strlen(argv[1]) >= MAXPATH ||
     (fd = open(argv[1], O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", argv[1]);
    exit(1);
  }
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", argv[1]);
    close(fd);
    exit(1);
  }
  close(fd);

  // <dir> itself is never printed; only search inside directories.
  if(st.type == T_DIR){
    strcpy(path, argv[1]);
    find();
  }
  exit(0);
}
