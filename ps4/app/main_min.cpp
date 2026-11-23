// Minimal variant: no filesystem calls, only printf + a 10 s wait, like the
// OpenOrbis hello_world sample. Used to bisect launch failures.
#include <cstdio>
#include <orbis/libkernel.h>

int main() {
  std::printf("MockPort minimal variant started\n");
  sceKernelUsleep(10 * 1000 * 1000);
  return 0;
}
