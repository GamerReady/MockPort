// Minimal variant: no filesystem calls, only printf and an endless wait, like
// the OpenOrbis hello_world sample (main() never returns). Used to bisect
// launch failures.
#include <cstdio>
#include <orbis/libkernel.h>

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("MockPort minimal variant started\n");
  for (;;) {
    sceKernelUsleep(1000 * 1000);
  }
}
