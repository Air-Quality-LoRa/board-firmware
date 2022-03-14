#define ENABLE_DEBUG  (1)
#include <debug.h>

#include <net/loramac.h>
#include <semtech_loramac.h>
#include <sx126x.h>
#include <sx126x_netdev.h>
#include <sx126x_params.h>

#include <ztimer.h>

#include "configuration.h"
#include "communication.h"
#include "error.h"

/* The fist datarate used to join the network */
#define INIT_DATARATE 6

semtech_loramac_t loramac;
sx126x_t sx126x;

static char loraDownlinkThreadStack[THREAD_STACKSIZE_DEFAULT];

static uint8_t configurationIsValid = 0;

static void *loraDownlinkThread(void *arg)
{
    kernel_pid_t mainThreadPid = arg;

    DEBUG("[communication] started downink thread");
    
    int ret;
    while (1) {
        /* blocks until some data is received */
        ret = semtech_loramac_recv(&loramac);
        if(ret == SEMTECH_LORAMAC_RX_DATA){
            DEBUG("[communication] received data");
            if(loramac.rx_data.payload_len > 6){
                setDynamicConfig(loramac.rx_data.payload[0]);
                if(configurationIsValid == 0){
                    configurationIsValid = 1;
                    msg_t msg;
                    msg_send(&msg, mainThreadPid);
                }
            }
        } else {         
            DEBUG("[communication] recieved ack or link status");
        }
        //TODO: when datarate changed, reset timers 
        DEBUG("[communication] Did the dr change? it is %i now, the recv returned %i", semtech_loramac_get_dr(&loramac), ret);

    }
    return NULL;
}

uint8_t loraGetDatarate(void){
    return semtech_loramac_get_dr(&loramac);
}

void loraJoin(void){

    uint8_t initDataRate = INIT_DATARATE;
    uint8_t waitBetweenJoinsSecs = 30;

    sx126x_setup(&sx126x, &sx126x_params[0], 0);
    loramac.netdev = &sx126x.netdev;
    loramac.netdev->driver = &sx126x_driver;
    
    if(semtech_loramac_init(&loramac)!=0){
        handleError("Could not init loramac");
    }

    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);
    
    semtech_loramac_set_adr(&loramac, true); //enable adaptative data rate
    semtech_loramac_set_tx_mode(&loramac, 1); //set unconfirmed uplink

    semtech_loramac_set_dr(&loramac, initDataRate);
    DEBUG("[communication] Starting join procedure: dr=%d\n", initDataRate);

    uint8_t joinRes;
    while ((joinRes = semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA)) != SEMTECH_LORAMAC_JOIN_SUCCEEDED)
    {
        DEBUG("[communication] Join procedure failed: code=%d \n", joinRes);

        if (initDataRate > 0)
        {
            initDataRate--;
            semtech_loramac_set_dr(&loramac, initDataRate);
        } else {
            initDataRate = INIT_DATARATE-2; //Retry on higher SF
        }

        DEBUG("[communication] Retry join procedure in %i sec. at dr=%d\n", waitBetweenJoinsSecs, initDataRate);
        ztimer_sleep(ZTIMER_SEC, waitBetweenJoinsSecs);
    }

    thread_create(loraDownlinkThreadStack, sizeof(loraDownlinkThreadStack),
              THREAD_PRIORITY_MAIN - 1, 0, loraDownlinkThread, NULL, "recv thread");

}

void loraGetConfiguration(void){
    configurationIsValid = 0;

    msg_t sendConfirmed = {0};
    int messageReceived = 0;
    do{
        semtech_loramac_set_tx_port(&loramac, 1);
        semtech_loramac_send(&loramac, NULL, 0);

        messageReceived = ztimer_msg_receive_timeout(ZTIMER_SEC, &sendConfirmed, 20);
        ztimer_sleep(ZTIMER_SEC, 60); //TODO: optimise for low datarates, 60 seconds will exeed x on datarate < 2?
    }while(messageReceived!=0);

}

void loraSendData(uint8_t data[], uint8_t len){

    enum dataToSend _dataToSend;
    getDynamicConfiguration(loraGetDatarate(), NULL, NULL, &_dataToSend);
    
    semtech_loramac_set_tx_port(&loramac, _dataToSend+1); //dataToSend + 1 gives the port on witch data have to be sent
    semtech_loramac_send(&loramac, data, len);
}