#define ENABLE_DEBUG (1)
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

static void *waitUserInterractionThread(void *arg)
{
    fflush(stdin);
    char c = 0;
    while (c != 'z')
    {
        c = getchar();
    }
    *(uint8_t *)arg = 1;
    return 0;
}

void configure(void)
{
    // Wait to give a chance to the user to see the message
    ztimer_sleep(ZTIMER_SEC, PROGRAM_START_AFTER_SEC);

    loadConfig();

#ifdef ENABLE_DEBUG
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
#endif
    printConfig();

    printf("The program will start automatically in %i seconds. Press z to change configuration...\n", PROGRAM_START_AFTER_SEC);
    // Create a thread to perform the blocking getchar()
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

    while (userInterracted)
    {
        interactiveConfig();
        saveConfig();
        printf("The configuration was saved!\n");

        printConfig();
        printf("The program will start automatically in %i seconds. Press z to configure...\n", PROGRAM_START_AFTER_SEC);

        userInterracted = 0;
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

    if (pms7003_init(pmsUsePowersaveMode))
    {
        printf("WARNING : PMS7003 is not reachable, check if it is correctly connected!\n");
    }
    if (bmx280_init(&bme_dev, &bmx280_params[0]) != BMX280_OK)
    {
        printf("WARNING : BME280 is not reachable, check if it is correctly connected!\n");
    }
    // TODO : stop main uart
}

uint8_t getTemperatureByte(int16_t temperature)
{ // TODO secure this
    // if(temperature == INT16_MIN){
    //     handleError("Could not read temperature.");
    // }
    if (temperature <= -2000)
    {
        return 0;
    }
    if (temperature > 8000)
    {
        return 200;
    }

    temperature += 2000;     // temperature is now between 0 and 10 000
    return temperature / 50; // 0 = -20°C or less, +1 every 0.5°C, 200 = 80°C or more
}

uint8_t getHumidityByte(uint16_t humidity)
{
    return humidity / 100;
}

void clearPacketEcc(uint8_t *packetEcc)
{
    for (uint8_t i = 0; i < 21; i++)
    {
        packetEcc[i] = 0;
    }
}

void updateEccFrame(uint8_t *packetEcc, uint8_t *sentFrame)
{
    for (uint8_t i = 0; i < 21; i++)
    {
        packetEcc[i] ^= sentFrame[i];
    }
}

#define RCV_QUEUE_SIZE 8
static msg_t rcv_queue[RCV_QUEUE_SIZE];

int main(void)
{
    // Messages and timers
    msg_t wakeUpMsgMeasure = {0};
    msg_t wakeUpMsgEcc = {0};
    msg_t msgRcv = {0};
    ztimer_t wakeUpTimerMeasure = {0};
    ztimer_t wakeUpTimerEcc = {0};

    // packet numbers and data
    uint8_t packetNumber = 0;
    uint8_t lastEccSent = 0;
    uint8_t packet[21];
    uint8_t packetEcc[21];
    clearPacketEcc(packetEcc);

    struct pms7003Data pmsData;

    msg_init_queue(rcv_queue, RCV_QUEUE_SIZE);

    configure(); // interractive

    loraJoin();
    loraGetConfigurationFromNetwork();

    uint8_t currentDatarate = loraGetDatarate();

    uint8_t sendIntervalMinutes;
    uint8_t eccSendInterval;
    enum dataToSend dataToSend;
    getDynamicConfiguration(currentDatarate, &sendIntervalMinutes, &eccSendInterval, &dataToSend);

    // TODO : send first measure asap
    wakeUpMsgMeasure.type = MSG_TYPE_MEASURE;
    ztimer_set_msg(ZTIMER_SEC, &wakeUpTimerMeasure, 4 /*4*60*/, &wakeUpMsgMeasure, getpid());

    // main loop
    while (1)
    {
        msg_receive(&msgRcv);

        if (msgRcv.type == MSG_TYPE_MEASURE)
        {

            pms7003_measure(&pmsData);

            // #if ENABLE_DEBUG
            //             pms7003_print(&pmsData);
            //             printf("\n");
            // #endif

            packet[0] = packetNumber++; // at 255 it will overflow and return to 0
            packet[1] = getTemperatureByte(bmx280_read_temperature(&bme_dev));
            packet[2] = getHumidityByte(bme280_read_humidity(&bme_dev)); // allways get humity after calling bmx280_read_temperature

            switch (dataToSend)
            {
            case pms_concentration:
                memcpy(&packet[3], pmsUseAtmoshphericMesure ? &pmsData.pm1_0Atmospheric : &pmsData.pm1_0Standard, 6);
                break;
            case pms_particle_units:
                memcpy(&packet[3], &pmsData.particuleGT0_3, 12);
                break;
            case all:
                memcpy(&packet[3], pmsUseAtmoshphericMesure ? &pmsData.pm1_0Atmospheric : &pmsData.pm1_0Standard, 6); // TODO atmospheric or standard???
                memcpy(&packet[9], &pmsData.particuleGT0_3, 12);
                break;
            default:
                break;
            }

            loraSendData(packet, 0);
            updateEccFrame(packetEcc, packet);
            lastEccSent++;

            // updating the datarate if necessary
            uint8_t newDatarate = loraGetDatarate();
            if (currentDatarate != newDatarate)
            {
                currentDatarate = newDatarate;
                lastEccSent = 0; // reset the ecc count
                clearPacketEcc(packetEcc);
                getDynamicConfiguration(currentDatarate, &sendIntervalMinutes, &eccSendInterval, &dataToSend); // TODO : change eccSendInterval resets the ecc packet
            }

            if (lastEccSent == eccSendInterval)
            {
                wakeUpMsgEcc.type = MSG_TYPE_ECC;
                ztimer_set_msg(ZTIMER_SEC, &wakeUpTimerEcc, 2 / 2 /*(sendIntervalMinutes/2)*60*/, &wakeUpMsgEcc, getpid()); // send an ecc message between two measures
                lastEccSent = 0;
                DEBUG("[main] The next packet will be ecc.\n");
            }
            else if (lastEccSent > eccSendInterval)
            {
                DEBUG("[main] The ecc packet should have been sent before, this should NEVER HAPPEN!!! Resetting ecc count.\n");
                lastEccSent = 0;
                clearPacketEcc(packetEcc);
            }
            else
            {
                DEBUG("[main] The next ecc packet will be sent in %i messages.\n", eccSendInterval - lastEccSent);
            }

            wakeUpMsgMeasure.type = MSG_TYPE_MEASURE;
            ztimer_set_msg(ZTIMER_SEC, &wakeUpTimerMeasure, 4 / 2 /*((sendIntervalMinutes*60)-(pmsUsePowersaveMode?30:0))/2*/, &wakeUpMsgMeasure, getpid());
        }
        else if (msgRcv.type == MSG_TYPE_ECC)
        {

            packetEcc[0] = packetNumber++;
            loraSendData(packetEcc, 1);
            clearPacketEcc(packetEcc);
            DEBUG("[main] The ecc packet was sent.\n");
        }
        else if (msgRcv.type == MSG_TYPE_CONFIG_CHANGED)
        {
            // stop timers and reset ecc count
            clearPacketEcc(packetEcc);
            lastEccSent = 0;
            ztimer_remove(ZTIMER_SEC, &wakeUpTimerEcc);
            ztimer_remove(ZTIMER_SEC, &wakeUpTimerMeasure);

            // get new configuration and start timer
            currentDatarate = loraGetDatarate();
            getDynamicConfiguration(currentDatarate, &sendIntervalMinutes, &eccSendInterval, &dataToSend);
            wakeUpMsgMeasure.type = MSG_TYPE_MEASURE;
            ztimer_set_msg(ZTIMER_SEC, &wakeUpTimerMeasure, 4 / 2 /*((sendIntervalMinutes*60)-(pmsUsePowersaveMode?30:0))/2*/, &wakeUpMsgMeasure, getpid());

            DEBUG("[main] Configuration has changed! Timers were reset with new configuration.\n");
        }
        else
        {
            DEBUG("[main] Unknown message received! THIS SOULD NEVER HAPPEN.\n");
        }
    }

    return 0;
}
