#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/uproc.h"
#include "user/user.h"

// Indexed by enum procstate (UNUSED, USED, SLEEPING, RUNNABLE, RUNNING,
// ZOMBIE). getprocs never returns UNUSED entries.
static char *states[] = {
  "unused", "used", "sleeping", "runnable", "running", "zombie"
};

// Global, not on the one-page user stack.
static struct uproc procs[NPROC];

int
main(void)
{
  int i, n;
  char *state;

  n = getprocs(procs, NPROC);
  if(n < 0){
    fprintf(2, "ps: getprocs failed\n");
    exit(1);
  }

  printf("PID\tPPID\tSTATE\tSIZE\tNAME\n");
  for(i = 0; i < n; i++){
    if(procs[i].state >= 0 && procs[i].state < sizeof(states) / sizeof(states[0]))
      state = states[procs[i].state];
    else
      state = "???";
    printf("%d\t%d\t%s\t%lu\t%s\n", procs[i].pid, procs[i].ppid, state,
           procs[i].sz, procs[i].name);
  }
  exit(0);
}
