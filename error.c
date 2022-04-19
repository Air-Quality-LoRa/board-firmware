#include "error.h"
#include "periph/pm.h"
#include "periph/wdt.h"

void handleError(char *message)
{
    printf("[Fatal error] %s\n", message);
    printf("              The card will restart now\n\n");
    pm_reboot();
}