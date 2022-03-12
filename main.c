#define ENABLE_DEBUG  (1)
#include <debug.h>

#include <stdio.h>
#include <string.h>
#include <board.h>

#include <shell.h>
#include <ztimer.h>

#include <net/loramac.h>
#include <semtech_loramac.h>

#include <bmx280_params.h>
#include <bmx280.h>

#include <sx126x.h>
#include <sx126x_netdev.h>
#include <sx126x_params.h>

#include "pms7003_driver.h"
#include "configuration.h"

#define TIME_BEFORE_PROGRAM_START 5

// devices
static sx126x_t sx126x;
semtech_loramac_t loramac;

i2c_t i2c_dev = I2C_DEV(0);
bmx280_t bme_dev;


static int _info_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // printf("size : %i\n", FLASHPAGE_SIZE); 

    // printf("flashpage_size : %i\n", flashpage_size(0));
    // printf("flashpage_addr : %p\n", flashpage_addr(0));
    // printf("flashpage_frist_free : %i", flashpage_first_free());
    // printf("flashpage_last_free : %i", flashpage_last_free());

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

    printf("humidity   %i pour mille\n", bme280_read_humidity(&bme_dev));
    printf("tempeature %i centi degré C\n", bmx280_read_temperature(&bme_dev));
    printf("pressure   %li Pa\n", bmx280_read_pressure(&bme_dev));

    return 0;
}

static int _pms_handler(int argc, char **argv){
    (void)argc;
    (void)argv;

    if(argc <= 1){
        printf("Usage : pms <init|print>\n");
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
        if(argc > 2){
            if(!strcmp(argv[2],"csv")){
            
            } else {
                printf("Usage : pms print [csv]\n");
                return 1;
            }
        } else {
            struct pms7003Data data;
            if(pms7003_measure(&data)==1){
                return 1;
            }
            pms7003_print(&data);
        }
    } else {
        printf("Usage : pms <init|print>\n");
        return 1;
    }

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "info", "test", _info_handler },
    { "pms", "play with pms7003 sensor", _pms_handler },
    { NULL, NULL, NULL }
};



void joinLora(void){

    uint8_t initDataRate = 0;
    uint8_t waitBetweenJoinsSecs = 240;

    sx126x_setup(&sx126x, &sx126x_params[0], 0);
    loramac.netdev = &sx126x.netdev;
    loramac.netdev->driver = &sx126x_driver;
    
    //Init
    semtech_loramac_init(&loramac);
    
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    DEBUG("[otaa] Starting join procedure: dr=%d\n", initDataRate);

    semtech_loramac_set_dr(&loramac, initDataRate);

    uint8_t joinRes;
    while ((joinRes = semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA)) != SEMTECH_LORAMAC_JOIN_SUCCEEDED)
    {
        DEBUG("[otaa] Join procedure failed: code=%d \n", joinRes);

        if (initDataRate > 0)
        {
            initDataRate--;
            semtech_loramac_set_dr(&loramac, initDataRate);
        }

        DEBUG("[otaa] Retry join procedure in %i sec. at dr=%d\n", waitBetweenJoinsSecs, initDataRate);

        ztimer_sleep(ZTIMER_MSEC, waitBetweenJoinsSecs*1000);
    }
}

static char waitUserInterractionStack[THREAD_STACKSIZE_DEFAULT];

static void* waitUserInterractionThread(void *arg){
    fflush(stdin);
    getchar();
    *(uint8_t*)arg=1;
    return 0;
}

void configure(void){
    //Wait to give a chance to the user to see the message
    ztimer_sleep(ZTIMER_SEC, TIME_BEFORE_PROGRAM_START);

    //Create a thread to perform the blocking getchar()

    uint8_t userInterracted = 0;
    thread_create(
        waitUserInterractionStack,
        sizeof(waitUserInterractionStack),
        THREAD_PRIORITY_MAIN - 1,
        THREAD_CREATE_STACKTEST,
        waitUserInterractionThread,
        &userInterracted,
        "get user interraction thread");

    loadConfig();

    printConfig();
    
    printf("The programm will start automatically in %i seconds. Press a key to configure...\n", TIME_BEFORE_PROGRAM_START);

    ztimer_sleep(ZTIMER_SEC, TIME_BEFORE_PROGRAM_START);

    while(userInterracted){
        interactiveConfig();
        saveConfig();
        printf("The configuration was saved!\n");

        printConfig();
        printf("The programm will start automatically in %i seconds. Press a key to configure...\n", TIME_BEFORE_PROGRAM_START);

        userInterracted=0;
        thread_create(
            waitUserInterractionStack,
            sizeof(waitUserInterractionStack),
            THREAD_PRIORITY_MAIN - 1,
            THREAD_CREATE_STACKTEST,
            waitUserInterractionThread,
            &userInterracted,
            "get user interraction thread");

        ztimer_sleep(ZTIMER_SEC, TIME_BEFORE_PROGRAM_START);
    }

    printf("Starting!\n");
}

static char loraDownlinkThreadStack[THREAD_STACKSIZE_DEFAULT];

static void *loraDownlinkThread(void *arg)
{
    (void)arg;
    while (1) {
        /* blocks until some data is received */
        semtech_loramac_recv(&loramac);

        if(loramac.rx_data.payload_len > 0){
            uplinkInterval = (loramac.rx_data.payload[0]&0xE0)>>5;
            dataToSend = (loramac.rx_data.payload[0]&0x18)>>3;
            eccFrameFrequency = loramac.rx_data.payload[0]&0x7;
        }
    }
    return NULL;
}

int main(void)
{ 
    configure();

    joinLora();

    thread_create(loraDownlinkThreadStack, sizeof(loraDownlinkThreadStack),
              THREAD_PRIORITY_MAIN - 1, 0, loraDownlinkThread, NULL, "recv thread");

    

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
