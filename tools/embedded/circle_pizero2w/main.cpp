#include "kernel.h"
#include <circle/startup.h>

int main(void) {
    CKernel kernel;
    if (!kernel.Initialize()) {
        halt();
        return EXIT_HALT;
    }

    TShutdownMode mode = kernel.Run();
    if (mode == ShutdownReboot) {
        reboot();
        return EXIT_REBOOT;
    }

    halt();
    return EXIT_HALT;
}
