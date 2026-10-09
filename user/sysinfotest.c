// Tests for the sysinfo system call.
#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "kernel/fcntl.h"
#include "user/user.h"

struct sysinfo a, b;
int failures;

void
get(struct sysinfo *s)
{
  if(sysinfo(s) < 0){
    printf("FAIL: sysinfo returned -1\n");
    exit(1);
  }
}

void
check(char *what, int ok)
{
  printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
  if(!ok)
    failures++;
}

int
main(void)
{
  int fd, p[2], pid, st;

  get(&a);
  printf("freemem=%lu nproc=%lu nopenfile=%lu\n", a.freemem, a.nproc,
         a.nopenfile);

  // freemem: allocating memory should use up free pages.
  sbrk(10 * 4096);
  get(&b);
  check("freemem drops by >= 10 pages after sbrk",
        a.freemem - b.freemem >= 10 * 4096);
  sbrk(-(10 * 4096));
  get(&b);
  check("freemem recovers after sbrk(-n)", b.freemem == a.freemem);

  // nopenfile: open/pipe add file-table entries, dup shares one.
  get(&a);
  fd = open("README", O_RDONLY);
  get(&b);
  check("open: nopenfile +1", b.nopenfile == a.nopenfile + 1);
  dup(fd);
  get(&b);
  check("dup: nopenfile unchanged", b.nopenfile == a.nopenfile + 1);
  pipe(p);
  get(&b);
  check("pipe: nopenfile +2 more", b.nopenfile == a.nopenfile + 3);
  close(p[0]);
  close(p[1]);
  get(&b);
  check("close pipe: back to +1", b.nopenfile == a.nopenfile + 1);
  close(fd);
  close(fd + 1);
  get(&b);
  check("close all: back to start", b.nopenfile == a.nopenfile);

  // nproc: fork adds a process, wait frees it.
  get(&a);
  pid = fork();
  if(pid == 0){
    pause(10);
    exit(0);
  }
  get(&b);
  check("fork: nproc +1", b.nproc == a.nproc + 1);
  wait(&st);
  get(&b);
  check("wait: nproc back", b.nproc == a.nproc);

  // Bad pointers must return -1, not crash.
  check("bad pointer 0xffffffffff returns -1",
        sysinfo((struct sysinfo *)0xffffffffffULL) == -1);
  check("pointer past sz returns -1",
        sysinfo((struct sysinfo *)(sbrk(0) + 8192)) == -1);
  check("read-only text pointer returns -1",
        sysinfo((struct sysinfo *)main) == -1);

  if(failures == 0)
    printf("sysinfotest: OK\n");
  else
    printf("sysinfotest: %d FAILED\n", failures);
  exit(failures ? 1 : 0);
}
