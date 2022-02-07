#define ENABLE_DEBUG  (1)
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "net/loramac.h"
#include "semtech_loramac.h"
#include "shell.h"
#include "periph_conf.h"

#if IS_USED(MODULE_SX127X)
#include "sx127x.h"
#include "sx127x_netdev.h"
#include "sx127x_params.h"
#endif

#if IS_USED(MODULE_SX126X)
#include "sx126x.h"
#include "sx126x_netdev.h"
#include "sx126x_params.h"
#endif

semtech_loramac_t loramac;

#if IS_USED(MODULE_SX127X)
static sx127x_t sx127x;
#endif
#if IS_USED(MODULE_SX126X)
static sx126x_t sx126x;
#endif

static const uint8_t deveui[LORAMAC_DEVEUI_LEN] = {0x70,0xB3,0xD5,0x7E,0xD0,0x04,0xC3,0x69};
static const uint8_t appeui[LORAMAC_APPEUI_LEN] = {0x99,0x99,0x00,0x00,0x00,0x00,0x77,0x77};
static const uint8_t appkey[LORAMAC_APPKEY_LEN] = {0x7B,0xE3,0x2C,0x90,0x46,0x86,0x73,0x25,0xC5,0xA0,0x71,0xBD,0xB1,0xC1,0x24,0x66};


static int _init_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    semtech_loramac_init(&loramac);
    return 0;
}

static int _keys_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    semtech_loramac_set_deveui(&loramac,deveui);
    semtech_loramac_set_appeui(&loramac,appeui);
    semtech_loramac_set_appkey(&loramac,appkey);

    semtech_loramac_set_dr(&loramac,12);
    return 0;
}

static int _join_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if(semtech_loramac_join(&loramac,LORAMAC_JOIN_OTAA) != SEMTECH_LORAMAC_JOIN_SUCCEEDED) {
        printf("Join procedure failed");
    } else {
        printf("Join procedure succeeded");
    }
    return 0;
}

static int _send_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char *message = "Hi";
    if(semtech_loramac_send(&loramac,(uint8_t*)message,strlen(message)) != SEMTECH_LORAMAC_TX_DONE) {
        printf("Can't send message");
    } else {
        printf("Message sent");
    }
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "init", "init loramac", _init_handler },
    { "keys", "init keys", _keys_handler },
    { "join", "joins ttn", _join_handler },
    { "send", "sends Hi on ttn", _send_handler },
    { NULL, NULL, NULL }
};

int main(void) {
    #if IS_USED(MODULE_SX127X)
        sx127x_setup(&sx127x, &sx127x_params[0], 0);
        loramac.netdev = &sx127x.netdev;
        loramac.netdev->driver = &sx127x_driver;
    #endif

    #if IS_USED(MODULE_SX126X)
        sx126x_setup(&sx126x, &sx126x_params[0], 0);
        loramac.netdev = &sx126x.netdev;
        loramac.netdev->driver = &sx126x_driver;
    #endif

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;


}
