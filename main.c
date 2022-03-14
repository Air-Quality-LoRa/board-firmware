#define ENABLE_DEBUG  (1)
#include <debug.h>

#include <stdio.h>
#include <string.h>
#include <board.h>

#include <shell.h>
#include <ztimer.h>

#include <net/loramac.h>
#include <semtech_loramac.h>
#include <sx126x.h>
#include <sx126x_netdev.h>
#include <sx126x_params.h>

#include <bmx280_params.h>
#include <bmx280.h>

#include "pms7003_driver.h"
#include "configuration.h"
#include "error.h"
#include "communication.h"

#define TIME_BEFORE_PROGRAM_START 5

// devices
i2c_t i2c_dev = I2C_DEV(0);
bmx280_t bme_dev;

static int _init_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    sx126x_setup(&sx126x, &sx126x_params[0], 0);
    loramac.netdev = &sx126x.netdev;
    loramac.netdev->driver = &sx126x_driver;
    
    if(semtech_loramac_init(&loramac)!=0){
        handleError("Could not init loramac");
    }

    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    semtech_loramac_set_dr(&loramac, 5);
    return 0;
}

static int _setadr_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    semtech_loramac_set_adr(&loramac, true);
    return 0;

}

static int _join_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    loraJoin();
    return 0;
}

static int _info_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("dr is %i", semtech_loramac_get_dr(&loramac));
    return 0;

    // printf("size : %i\n", FLASHPAGE_SIZE); 

    // printf("flashpage_size : %i\n", flashpage_size(0));
    // printf("flashpage_addr : %p\n", flashpage_addr(0));
    // printf("flashpage_frist_free : %i", flashpage_first_free());
    // printf("flashpage_last_free : %i", flashpage_last_free());

    // int ret = bmx280_init(&bme_dev, &bmx280_params[0]);
    // if(ret == BMX280_OK){
    //     printf("BMX280_OK\n");
    // }
    // if(ret == BMX280_ERR_BUS){
    //     printf("BMX280_ERR_BUS\n");
    // }
    // if(ret == BMX280_ERR_NODEV){
    //     printf("BMX280_ERR_NODEV\n");
    // }

    // //printf("humidity   %i pour mille\n", bme280_read_humidity(&bme_dev));
    // printf("tempeature %i centi degré C\n", bmx280_read_temperature(&bme_dev));
    // printf("pressure   %li Pa\n", bmx280_read_pressure(&bme_dev));

    // return 0;


}

static int _send_confirmed_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    semtech_loramac_set_tx_mode(&loramac, 0);
    uint8_t a[2] = {'c','c'};
    semtech_loramac_send(&loramac, a, 2);
    //int return_code = semtech_loramac_recv(&loramac);

    // printf("return code is %i and is rx confirmed: %i\n", return_code, return_code==SEMTECH_LORAMAC_RX_CONFIRMED);
    // printf("\ndata received: len=%i, port=%i\n",loramac.rx_data.payload_len, loramac.rx_data.port);
    
    return 0;
}

static int _send_handler(int argc, char **argv) {
    (void)argc;
    (void)argv;
    semtech_loramac_set_tx_mode(&loramac, 1);
    uint8_t a[2] = {'c','c'};
    semtech_loramac_send(&loramac, a , 2);
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
    { "send", "test", _send_handler },
    { "send_confirm", "test", _send_confirmed_handler },
    { "join", "test", _join_handler },
    { "setadr", "test", _setadr_handler },
    { "init", "test", _init_handler },
    { "pms", "play with pms7003 sensor", _pms_handler },
    { NULL, NULL, NULL }
};

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
    
    printf("The program will start automatically in %i seconds. Press a key to configure...\n", TIME_BEFORE_PROGRAM_START);

    ztimer_sleep(ZTIMER_SEC, TIME_BEFORE_PROGRAM_START);

    while(userInterracted){
        interactiveConfig();
        saveConfig();
        printf("The configuration was saved!\n");

        printConfig();
        printf("The program will start automatically in %i seconds. Press a key to configure...\n", TIME_BEFORE_PROGRAM_START);

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

uint8_t getTemperatureByte(int16_t temperature)
{
    if(temperature == INT16_MIN){
        handleError("Could not read temperature.");
    }
    if(temperature <= -2000){
        return 0;
    }
    if(temperature > 8000){
        return 200;
    }

    temperature+=2000; //temperature is now between 0 and 10 000
    return temperature/50; //0 = -20°C or less, +1 every 0.5°C, 200 = 80°C or more
}

uint8_t getHumidityByte(uint16_t humidity){
    return humidity/100;
}


void clearFrameEcc(uint8_t *frameEcc){
    for (uint8_t i = 0; i < 21; i++){
        frameEcc[i]= 0;
    } 
}

#define MSG_TYPE_MESURE 0x0
#define MSG_TYPE_ECC 0x1

int main(void)
{ 

    configure();
    // char line_buf[SHELL_DEFAULT_BUFSIZE];
    // shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    loraJoin();
    loraGetConfiguration();

    uint8_t packetNumber = 0;
    uint8_t lastEccSent = 0;
    msg_t msgWakeUpMesure = {0};
    msg_t msgRcv = {0};
    ztimer_t timer = {0};

    struct pms7003Data pmsData;

    uint8_t frame[21];
    uint8_t frameEcc[21];
    clearFrameEcc(&frameEcc);

    //TODO: change timing
    msgWakeUpMesure.type = MSG_TYPE_MESURE;
    ztimer_set_msg(ZTIMER_SEC, &timer, 1, &msgWakeUpMesure, getpid());

    while(1){
        msg_receive(&msgRcv);   
        if(msgRcv.type == MSG_TYPE_MESURE){

            enum dataToSend dataToSend;
            uint8_t sendIntervalMinutes;
            uint8_t eccSendInterval;
            getDynamicConfiguration(loraGetDatarate(), &sendIntervalMinutes, &eccSendInterval, &dataToSend);

            pms7003_measure(&pmsData);
            
            frame[0] = packetNumber++; //at 255 it will overflow and return to 0
            frame[1] = getTemperatureByte(bmx280_read_temperature(&bme_dev));
            frame[2] = getHumidityByte(bme280_read_humidity(&bme_dev)); //allways get humity after calling bmx280_read_temperature  

            int frameLength;

            switch (dataToSend)
            {
            case pms_concentration:
                memcpy(&frame[3], pmsUseAtmoshphericMesure?&pmsData.pm1_0Atmospheric:&pmsData.pm1_0Standard, 6);
                frameLength = 9;
                break;
            case pms_particle_units:
                memcpy(&frame[3], &pmsData.particuleGT0_3, 12);
                frameLength = 15;
                break;
            case all:
                memcpy(&frame[3], pmsUseAtmoshphericMesure?&pmsData.pm1_0Atmospheric:&pmsData.pm1_0Standard, 6); //TODO atmospheric or standard???
                memcpy(&frame[9], &pmsData.particuleGT0_3, 12);
                frameLength = 21;
                break;
            default:
                break;
            }
            loraSendData(frame, frameLength);

            updateEccFrame(frameEcc, frame);
            lastEccSent++;
            if(lastEccSent == eccSendInterval){
                //Shedule an ecc packet
                lastEccSent = 0;
            }
            //ztimer send message mesure to self
        } else if (msg_rcv.type == MSG_TYPE_ECC) {

            loraSendData(frameEcc, frameLength);
            lastEccSent = 0;
        }
    }

    // char line_buf[SHELL_DEFAULT_BUFSIZE];
    // shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
