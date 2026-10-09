#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

static char *args[MAXARG + 1];  // argv for exec, NULL-terminated
static int nbase;               // cmd + fixed args at the front of args
static int nargs;               // entries in args right now
static int n;                   // -n value, or 0 for one run per line
static int failed;              // did any run exit non-zero?

static char words[1024];        // storage for the words in args
static int wpos;

static void
usage(void)
{
  fprintf(2, "usage: xargs [-n N] cmd [args...]\n");
  exit(1);
}

// Run the command with the words collected so far, then start a new batch.
static void
run(void)
{
  int pid, status;

  if(nargs == nbase)    // no words (e.g. an empty line): nothing to run
    return;
  args[nargs] = 0;

  pid = fork();
  if(pid < 0){
    fprintf(2, "xargs: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    exec(args[0], args);
    fprintf(2, "xargs: cannot exec %s\n", args[0]);
    exit(1);
  }
  if(wait(&status) < 0 || status != 0)
    failed = 1;

  nargs = nbase;
  wpos = 0;
}

static void
addword(char *w, int len)
{
  // Start a new batch if this word would not fit. The kernel's sys_exec
  // keeps argv in char *argv[MAXARG] and needs room for the final NULL,
  // so at most MAXARG-1 real arguments (cmd included) can be passed.
  if(nargs == MAXARG - 1 || wpos + len + 1 > sizeof(words))
    run();
  if(len + 1 > sizeof(words)){
    fprintf(2, "xargs: word too long\n");
    exit(1);
  }
  memmove(words + wpos, w, len);
  words[wpos + len] = '\0';
  args[nargs++] = words + wpos;
  wpos += len + 1;

  if(n > 0 && nargs - nbase == n)
    run();
}

int
main(int argc, char *argv[])
{
  char buf[512], cur[512];
  char *s, c;
  int i, k, m, curlen;

  i = 1;
  if(argc > 1 && strcmp(argv[1], "-n") == 0){
    if(argc < 3 || argv[2][0] == '\0')
      usage();
    for(s = argv[2]; *s; s++)
      if(*s < '0' || *s > '9')
        usage();
    n = atoi(argv[2]);
    if(n < 1)
      usage();
    i = 3;
  }
  if(i >= argc)
    usage();      // no command given
  if(argc - i > MAXARG - 2){    // leave room for at least one word
    fprintf(2, "xargs: too many arguments\n");
    exit(1);
  }
  for(; i < argc; i++)
    args[nbase++] = argv[i];
  nargs = nbase;

  // Split stdin into words; a newline also ends a run when there is no -n.
  curlen = 0;
  while((m = read(0, buf, sizeof(buf))) > 0){
    for(k = 0; k < m; k++){
      c = buf[k];
      if(c == ' ' || c == '\t' || c == '\n'){
        if(curlen > 0){
          addword(cur, curlen);
          curlen = 0;
        }
        if(c == '\n' && n == 0)
          run();
      } else {
        if(curlen == sizeof(cur)){
          fprintf(2, "xargs: word too long\n");
          exit(1);
        }
        cur[curlen++] = c;
      }
    }
  }
  if(curlen > 0)
    addword(cur, curlen);
  run();    // last line without '\n', or leftover words with -n

  exit(failed ? 1 : 0);
}
