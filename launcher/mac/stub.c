// Точка входа Bastion Online.app: запускает Resources/bastion.sh.
// Зажатый Option при запуске открывает служебное меню (BASTION_MENU=1).
#include <ApplicationServices/ApplicationServices.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char exe[PATH_MAX], real[PATH_MAX], script[PATH_MAX];
    uint32_t size = sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) != 0 || !realpath(exe, real)) return 1;
    snprintf(script, sizeof(script), "%s/../Resources/bastion.sh", dirname(real));

    CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
    if (flags & kCGEventFlagMaskAlternate) setenv("BASTION_MENU", "1", 1);

    execl("/bin/bash", "bash", script, (char *)NULL);
    perror("exec bastion.sh");
    return 1;
}
