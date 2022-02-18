#define ENABLE_DEBUG  (1)
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "net/loramac.h"
#include "semtech_loramac.h"
#include "shell.h"
#include "periph_conf.h"
#include "thread.h"
#include "fmt.h"
#include "periph/i2c.h"

#include "bmx280_params.h"
#include <bmx280.h>

#include "pms7003_driver.h"


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
#define RECV_MSG_QUEUE                   (4U)

semtech_loramac_t loramac;
i2c_t dev = I2C_DEV(0);
bmx280_t bme_dev;

static msg_t _recv_queue[RECV_MSG_QUEUE];
 
static char _recv_stack[THREAD_STACKSIZE_DEFAULT];

#if IS_USED(MODULE_SX127X)
static sx127x_t sx127x;
#endif
#if IS_USED(MODULE_SX126X)
static sx126x_t sx126x;
#endif

static const uint8_t deveui[LORAMAC_DEVEUI_LEN] = {0x70,0xB3,0xD5,0x7E,0xD0,0x04,0xC3,0x69};
static const uint8_t appeui[LORAMAC_APPEUI_LEN] = {0x99,0x99,0x00,0x00,0x00,0x00,0x77,0x77};
static const uint8_t appkey[LORAMAC_APPKEY_LEN] = {0x7B,0xE3,0x2C,0x90,0x46,0x86,0x73,0x25,0xC5,0xA0,0x71,0xBD,0xB1,0xC1,0x24,0x66};

static void *_recv(void *arg)
{
    msg_init_queue(_recv_queue, RECV_MSG_QUEUE);
 
    (void)arg;
    int i = 0;
    while (1) {
        i = i +1;
        /* blocks until some data is received */
        semtech_loramac_recv(&loramac);
        loramac.rx_data.payload[loramac.rx_data.payload_len] = 0;
        printf("%s\n",thread_getname(thread_getpid()));
        printf("%i,Data receivvvvved: %s, port: %d\n", i,
               (char *)loramac.rx_data.payload, loramac.rx_data.port);
    }
    return NULL;
}

static int _startrec_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    thread_create(_recv_stack, sizeof(_recv_stack),
              THREAD_PRIORITY_MAIN - 1, 0, _recv, NULL, "recvvv thread");
    return 0;
}

static int _pms_handler(int argc, char **argv){
    (void)argc;
    (void)argv;

    if(argc <= 1){
        printf("Usage : pms <init|measure>\n");
        return 1;
    }

    if(!strcmp(argv[1],"init")){
        if(argc <= 2){
            printf("Usage : pms init <powersave|nopowersave>\n");
            return 1;
        }
        if(!strcmp(argv[2], "powersave")){
            pms7003_init(1);
        } else if(!strcmp(argv[2], "nopowersave")){
            pms7003_init(0);
        } else {
            printf("Usage : pms init <powersave|nopowersave>\n");
            return 1;
        }
    } else if (!strcmp(argv[1],"print")){
        struct pms7003Data data;
        if(pms7003_measure(&data)==1){
            return 1;
        }
        pms7003_print(&data);
    } else {
        printf("Usage : pms <init|print>\n");
        return 1;
    }

    return 0;
}

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

    semtech_loramac_set_dr(&loramac,9);
    semtech_loramac_set_tx_mode(&loramac,1);
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

static int _temp_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int ret = bmx280_init(&bme_dev, &bmx280_params[0]);
    if(ret == BMX280_OK){
        printf("BMX280_OK\n");
    }
    if(ret == BMX280_ERR_BUS){
        printf("BMX280_ERR_BUS\n");
    }
    if(ret == BMX280_ERR_NODEV){
        printf("BMX280_ERR_NODEV\n");
    }


    double humidity = (double)bme280_read_humidity(&bme_dev);
    double temperature = (double)bmx280_read_temperature(&bme_dev);

    printf("humidity   %f %%\n", humidity);
    printf("tempeature %f °C\n", temperature/(double)100);
    printf("pressure   %li Pa\n", bmx280_read_pressure(&bme_dev));


    // i2c_init(dev);
    // i2c_acquire(dev);
    // char i2c[2]= {0,0};
    // i2c_read_regs(dev, 0x76, 0x05, &i2c, 2, 0);

    // printf("i2c : %i | %i", (int) i2c[0], (int) i2c[1]);

    // i2c_release(dev);
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "init", "init loramac", _init_handler },
    { "keys", "init keys", _keys_handler },
    { "join", "joins ttn", _join_handler },
    { "send", "sends Hi on ttn", _send_handler },
    { "i2c", "try i2c read", _temp_handler },
    { "pms", "play with pms7003 sensor", _pms_handler },
    { "startrec", "start rec thread", _startrec_handler},
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
