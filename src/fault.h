#ifndef STRACE_FAIL_H
#define STRACE_FAIL_H

int inject_fault(int pid, int nth);
int fault_injected(int pid);

#endif
