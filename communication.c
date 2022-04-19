#define ENABLE_DEBUG  (1)
#include <debug.h>

#include <net/loramac.h>
#include <semtech_loramac.h>
#include <sx126x.h>
#include <sx126x_netdev.h>
#include <sx126x_params.h>

#include <ztimer.h>
#include <periph/wdt.h>

#include "configuration.h"
#include "communication.h"
#include "error.h"
#include "util.h"
#include "messages.h"

/* The fist datarate used to join the network */
#define INIT_DATARATE 5

semtech_loramac_t loramac;
sx126x_t sx126x;

static uint8_t configurationIsValid = 0;

static char watchdogThreadStack[THREAD_STACKSIZE_DEFAULT];

static void *watchdogThread(void *arg){
    (void)arg;
    msg_t msg;
    msg_receive(&msg);
    handleError("watchdog restart");
    return NULL;
}

// ztimer_t timerwatchdog = {0};
// msg_t msgWatchdog = {0};
kernel_pid_t pid_watchdog;
// inline static void setLoraWatchdog(void){
//     ztimer_set_msg(ZTIMER_SEC, &timerwatchdog, 30, &msgWatchdog, pid_watchdog);
// }  

// inline static void stopLoraWatchdog(void){
//     ztimer_remove(ZTIMER_SEC, &timerwatchdog);
// }



static char loraDownlinkThreadStack[THREAD_STACKSIZE_DEFAULT];

static void *loraDownlinkThread(void *arg)
{
    msg_t msg;
    kernel_pid_t mainPid = *(kernel_pid_t*)arg;

    DEBUG("[communication] started downink thread\n");
    
    int ret;
    while (1) {
        /* blocks until some data is received */
        ret = semtech_loramac_recv(&loramac);
        if(ret == SEMTECH_LORAMAC_RX_DATA){
            DEBUG("[communication] downlink : received data\n");
            if(loramac.rx_data.payload_len >= 6){
                setDynamicConfig(loramac.rx_data.payload);
                msg.type = MSG_TYPE_CONFIG_CHANGED;
                msg_send(&msg, mainPid);
            }
        } else {         
            DEBUG("[communication] downlink : recieved ack or link status\n");
        }
        DEBUG("[communication] Datarate is now %i\n", semtech_loramac_get_dr(&loramac));
        DEBUG("[communication] semtech_loramac_recv returned %i\n", ret);
    }
    return NULL;
}

uint8_t loraGetDatarate(void){
    return semtech_loramac_get_dr(&loramac);
}


void loraJoin(void){

    //temporary watchdog
    pid_watchdog = thread_create(watchdogThreadStack, sizeof(watchdogThreadStack),
              THREAD_PRIORITY_MAIN - 1, 0, watchdogThread, NULL, "watchdog thread");

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
    //setLoraWatchdog();

    while ((joinRes = semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA)) != SEMTECH_LORAMAC_JOIN_SUCCEEDED)
    {
        //stopLoraWatchdog();

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
        //setLoraWatchdog();

    }
    //stopLoraWatchdog();


    kernel_pid_t pid = getpid();
    thread_create(loraDownlinkThreadStack, sizeof(loraDownlinkThreadStack),
              THREAD_PRIORITY_MAIN - 1, 0, loraDownlinkThread, &pid, "recv thread");
            
    DEBUG("[communication] Network joined!!\n");
    //TODO remove this? downgrading datarate to be sure the first messages will be sent.
    semtech_loramac_set_dr(&loramac, semtech_loramac_get_dr(&loramac)-1);
    semtech_loramac_set_adr(&loramac, true); //enable adaptative data rate (idk if the adr enable before join worked?)
}

void loraGetConfigurationFromNetwork(void){
    configurationIsValid = 0;

    msg_t sendConfirmed = {0};
    int messageReceived = 0;

    ztimer_sleep(ZTIMER_SEC, 10);

    do{
        semtech_loramac_set_tx_port(&loramac, 1);
        //setLoraWatchdog();
        semtech_loramac_send(&loramac, NULL, 0);
        //stopLoraWatchdog();

        DEBUG("[communication] Asked the server for a configuration\n");

        messageReceived = ztimer_msg_receive_timeout(ZTIMER_SEC, &sendConfirmed, 20);
        if(messageReceived>=0 && sendConfirmed.type != MSG_TYPE_CONFIG_CHANGED){
            handleError("Received an unexpected message in loraGetConfiguration. Expected MSG_TYPE_CONFIG_CHANGED");
        }
        ztimer_sleep(ZTIMER_SEC, 20); //TODO: optimise for low datarates, 60 seconds will exeed x on datarate < 2?
    }while(messageReceived<=0);
    DEBUG("[communication] The configuration was received\n");

}

void loraSendData(uint8_t data[], uint8_t type){

    uint8_t len = 0;

    enum dataToSend _dataToSend;
    getDynamicConfiguration(loraGetDatarate(), NULL, NULL, &_dataToSend);

    switch (_dataToSend)
    {
    case pms_concentration:
        len = 9;
        break;
    case pms_particle_units:
        len = 15;
        break;
    case all:
        len = 21;
        break;
    default:
        len = 3;
    }
    #ifdef ENABLE_DEBUG
    DEBUG("[communication] The message that will be sent is : ");
    printHexArray(data, len);
    DEBUG("\n");
    #endif

    uint8_t port = _dataToSend+1+(type?3:0);
    semtech_loramac_set_tx_port(&loramac, port); //dataToSend + 1 gives the port on witch data have to be sent
    DEBUG("[communication] The message will be sent on port : %d\n", port);
    
    //setLoraWatchdog();
    // semtech_loramac_send(&loramac, data, len);
    //stopLoraWatchdog();
}