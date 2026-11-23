// MockPort PS4 shell: minimal OpenOrbis homebrew used to prove that the
// toolchain -> eboot.bin -> .pkg pipeline works. It does not start the
// Mocktail runtime; it only records that it launched.
//
// Like the OpenOrbis hello_world sample, main() never returns: an Orbis app
// that exits from main() is treated as a crash by the system. Close it with
// the PS button.
#include <cstdio>
#include <sys/stat.h>
#include <orbis/libkernel.h>

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  mkdir("/data/mockport", 0777);  // ignore errors: it may already exist
  FILE* log = fopen("/data/mockport/boot.log", "a");
  if (log) {
    fputs("MockPort PS4 shell started\n", log);
    fclose(log);
  }
  std::printf("MockPort PS4 shell started\n");
  for (;;) {
    sceKernelUsleep(1000 * 1000);
  }
}
