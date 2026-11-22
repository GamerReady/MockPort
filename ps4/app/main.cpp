// MockPort PS4 shell: minimal OpenOrbis homebrew used to prove that the
// toolchain -> eboot.bin -> .pkg pipeline works. It does not start the
// Mocktail runtime; it only records that it launched.
#include <cstdio>
#include <sys/stat.h>
#include <orbis/libkernel.h>

int main() {
  mkdir("/data/mockport", 0777);  // ignore errors: it may already exist
  FILE* log = fopen("/data/mockport/boot.log", "a");
  if (log) {
    fputs("MockPort PS4 shell started\n", log);
    fclose(log);
  }
  std::printf("MockPort PS4 shell started\n");
  sceKernelUsleep(3 * 1000 * 1000);
  return 0;
}
