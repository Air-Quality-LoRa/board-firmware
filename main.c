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
#include "messages.h"
#include "util.h"

#define PROGRAM_START_AFTER_SEC 5

// devices
i2c_t i2c_dev = I2C_DEV(0);
bmx280_t bme_dev;

static char waitUserInterractionStack[THREAD_STACKSIZE_DEFAULT];

static void* waitUserInterractionThread(void *arg){
    fflush(stdin);
    char c = 0;
    while(c!='z'){
       c = getchar() ;
    }
    *(uint8_t*)arg=1;
    return 0;
}

void configure(void){
    //Wait to give a chance to the user to see the message
    ztimer_sleep(ZTIMER_SEC, PROGRAM_START_AFTER_SEC);

    loadConfig();

    #ifdef ENABLE_DEBUG
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    #endif
    printConfig();

    printf("The program will start automatically in %i seconds. Press z to change configuration...\n", PROGRAM_START_AFTER_SEC);
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

    ztimer_sleep(ZTIMER_SEC, PROGRAM_START_AFTER_SEC);

    while(userInterracted){
        interactiveConfig();
        saveConfig();
        printf("The configuration was saved!\n");

        printConfig();
        printf("The program will start automatically in %i seconds. Press z to configure...\n", PROGRAM_START_AFTER_SEC);

        userInterracted=0;
        thread_create(
            waitUserInterractionStack,
            sizeof(waitUserInterractionStack),
            THREAD_PRIORITY_MAIN - 1,
            THREAD_CREATE_STACKTEST,
            waitUserInterractionThread,
            &userInterracted,
            "get user interraction thread");

        ztimer_sleep(ZTIMER_SEC, PROGRAM_START_AFTER_SEC);
    }

    printf("Starting!\n");

    if (pms7003_init(pmsUsePowersaveMode)){
        printf("WARNING : PMS7003 is not reachable, check if it is correctly connected!\n");
    }
    if(bmx280_init(&bme_dev, &bmx280_params[0]) != BMX280_OK){
        printf("WARNING : BME280 is not reachable, check if it is correctly connected!\n");
    }
    //TODO : stop main uart?
}

uint8_t getTemperatureByte(int16_t temperature)
{   //TODO secure this
    // if(temperature == INT16_MIN){
    //     handleError("Could not read temperature.");
    // }
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

void updateEccFrame(uint8_t *frameEcc, uint8_t *sentFrame){
    for (uint8_t i = 0; i < 21; i++)
    {
        frameEcc[i] ^=sentFrame[i];
    }
    
}

int main(void)
{ 
    configure();

    loraJoin();
    ztimer_sleep(ZTIMER_SEC, 10);
    loraGetConfiguration();

    uint8_t packetNumber = 0;
    uint8_t lastEccSent = 0;
    msg_t wakeUpMsgMesure = {0};
    msg_t wakeUpMsgEcc = {0};
    msg_t msgRcv = {0};
    ztimer_t timerMesure = {0};
    ztimer_t timerEcc = {0};

    struct pms7003Data pmsData;

    uint8_t frame[21];
    uint8_t frameEcc[21];
    clearFrameEcc(frameEcc);

    uint8_t currentDatarate = loraGetDatarate();

    uint8_t sendIntervalMinutes, eccSendInterval;
    enum dataToSend dataToSend;
    getDynamicConfiguration(currentDatarate, &sendIntervalMinutes, &eccSendInterval, &dataToSend);

    //TODO : first mesure can be send 4 minutes after wihout problem
    wakeUpMsgMesure.type = MSG_TYPE_MESURE;
    ztimer_set_msg(ZTIMER_SEC, &timerMesure, 30/*4*60*/, &wakeUpMsgMesure, getpid());

    while(1){
        msg_receive(&msgRcv);   
        if(msgRcv.type == MSG_TYPE_MESURE){

            pms7003_measure(&pmsData);

            #if ENABLE_DEBUG
            pms7003_print(&pmsData);
            printf("\n");
            #endif

            frame[0] = packetNumber++; //at 255 it will overflow and return to 0
            frame[1] = getTemperatureByte(bmx280_read_temperature(&bme_dev));
            frame[2] = getHumidityByte(bme280_read_humidity(&bme_dev)); //allways get humity after calling bmx280_read_temperature  

            switch (dataToSend)
            {
            case pms_concentration:
                memcpy(&frame[3], pmsUseAtmoshphericMesure?&pmsData.pm1_0Atmospheric:&pmsData.pm1_0Standard, 6);
                break;
            case pms_particle_units:
                memcpy(&frame[3], &pmsData.particuleGT0_3, 12);
                break;
            case all:
                memcpy(&frame[3], pmsUseAtmoshphericMesure?&pmsData.pm1_0Atmospheric:&pmsData.pm1_0Standard, 6); //TODO atmospheric or standard???
                memcpy(&frame[9], &pmsData.particuleGT0_3, 12);
                break;
            default:
                break;
            }
            loraSendData(frame, 0);

            //updating the datarate if necessary
            uint8_t newDatarate = loraGetDatarate();
            if(currentDatarate != newDatarate){
                currentDatarate = newDatarate;
                lastEccSent = 0; //reset the ecc count
                clearFrameEcc(frameEcc);
            }

            getDynamicConfiguration(currentDatarate, &sendIntervalMinutes, &eccSendInterval, &dataToSend);
            updateEccFrame(frameEcc, frame);

            lastEccSent++;
            if(lastEccSent == eccSendInterval){
                wakeUpMsgEcc.type = MSG_TYPE_ECC;
                ztimer_set_msg(ZTIMER_SEC, &timerEcc, 60/*(sendIntervalMinutes/2)*60*/, &wakeUpMsgEcc, getpid());
                lastEccSent = 0;
                DEBUG("[main] Next message will be ecc.\n");
            } else if(lastEccSent>eccSendInterval){
                DEBUG("[main] Should have sent ecc before... Resetting ecc count\n");
                lastEccSent = 0;
                clearFrameEcc(frameEcc);
            } else {
                DEBUG("[main] %i messages to send before ecc.\n", eccSendInterval-lastEccSent);
            }
            
            wakeUpMsgMesure.type = MSG_TYPE_MESURE;
            ztimer_set_msg(ZTIMER_SEC, &timerMesure, 60/*(sendIntervalMinutes*60)-(pmsUsePowersaveMode?30:0)*/, &wakeUpMsgMesure, getpid());
        
        } else if (msgRcv.type == MSG_TYPE_ECC) {
            frameEcc[0] = packetNumber++;
            loraSendData(frameEcc, 1);
            clearFrameEcc(frameEcc); //if ecc was planned, stop it since it will be only 0
            DEBUG("[main] Sent ECC frame\n");

        } else if (msgRcv.type == MSG_TYPE_CONFIG_CHANGED){
            clearFrameEcc(frameEcc);
            lastEccSent = 0;
            ztimer_remove(ZTIMER_SEC, &timerEcc);
            DEBUG("[main] Configuration has changed!!\n");
        } else {
            DEBUG("[main] Unknown message received!!!\n");

        }
    }

    return 0;
}
