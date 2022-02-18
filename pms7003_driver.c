#include <stdio.h>
#include <string.h>
#include "board.h"
#include "periph/uart.h"
#include "pms7003_driver.h"
#include "thread.h"
#include "ztimer.h"

#define ENABLE_DEBUG 1
#include "debug.h"

#define USED_UART UART_DEV(1)

#define RCV_QUEUE_SIZE  8
char pms7003_thread_stack[THREAD_STACKSIZE_MAIN];
static msg_t rcv_queue[RCV_QUEUE_SIZE];
kernel_pid_t pms7003_pid = 0;

static struct pms7003Data lastMesure;

static enum state currentState = uninitialized;
static uint8_t useTheSleepMode = 0;

static const uint8_t readFrame[] =          {0x42, 0x4d, 0xe2, 0x00, 0x00, 0x01, 0x71};
static const uint8_t passiveModeFrame[] =   {0x42, 0x4d, 0xe1, 0x00, 0x00, 0x01, 0x70};
//static const uint8_t activeModeFrame[] =    {0x42, 0x4d, 0xe1, 0x00, 0x01, 0x01, 0x71};
static const uint8_t sleepFrame[] =         {0x42, 0x4d, 0xe4, 0x00, 0x00, 0x01, 0x73};
static const uint8_t wakeupFrame[] =        {0x42, 0x4d, 0xe4, 0x00, 0x01, 0x01, 0x74};

#define MAX_USER_READ_QUEUE_SIZE 10
static uint8_t pidDataRead = 0;
static uint8_t pidDataWrite = 0;
static kernel_pid_t pidData[MAX_USER_READ_QUEUE_SIZE];

uint8_t queue_pop_pid(kernel_pid_t *pid) {
    if (pidDataRead == pidDataWrite) {
        return 1;
    }
    *pid = pidData[pidDataRead];
    pidDataRead = (pidDataRead+1)%MAX_USER_READ_QUEUE_SIZE;
    return 0;
}

uint8_t queue_push_pid(kernel_pid_t pid) {
    uint8_t nextWrite = (pidDataWrite+1)%MAX_USER_READ_QUEUE_SIZE;
    if(nextWrite == pidDataRead){
        return 1;
    }
    pidData[pidDataWrite] = pid;
    pidDataWrite = nextWrite;
    return 0;
}

uint8_t queue_empty_pid(void) {
    return pidDataRead==pidDataWrite;
}

#define RX_FRAME_SIZE 32
#define DATA_FRAME_LENGTH RX_FRAME_SIZE
#define SERVICE_FRAME_LENGTH 8

#define VALID_DATA_AFTER_WAKEUP_SEC 5
#define TIME_BEFORE_GOING_BACK_TO_SLEEP_SEC 5

uint8_t _verify_checksum(uint8_t* frame, uint8_t lenght){
    uint16_t checksum = 0;
    for(uint8_t i = 0; i<lenght-2; i++){
        checksum+=frame[i];
    }
    return (checksum==((frame[lenght-2]<<8)|frame[lenght-1]));
}

static inline uint8_t _check_data_frame_valid(uint8_t* frame){
    return _verify_checksum(frame, DATA_FRAME_LENGTH);
}

static inline uint8_t _check_service_frame_valid(uint8_t* frame){
    return _verify_checksum(frame, SERVICE_FRAME_LENGTH);
}

uint8_t _decode_data_frame(struct pms7003Data *data ,uint8_t* frame){
    if (_check_data_frame_valid(frame) == 0){
        return 1;
    }

    data->pm1_0Standard = (frame[4]<<8) | frame[5];
    data->pm2_5Standard = (frame[6]<<8) | frame[7];
    data->pm10Standard = (frame[8]<<8) | frame[9];

    data->pm1_0Atmospheric = (frame[10]<<8) | frame[11];
    data->pm2_5Atmospheric = (frame[12]<<8) | frame[13];
    data->pm10Atmospheric = (frame[14]<<8) | frame[15];

    data->particuleGT0_3 = (frame[16]<<8) | frame[17];
    data->particuleGT0_5 = (frame[18]<<8) | frame[19];
    data->particuleGT1_0 = (frame[20]<<8) | frame[21];
    data->particuleGT2_5 = (frame[22]<<8) | frame[23];
    data->particuleGT5_0 = (frame[24]<<8) | frame[25];
    data->particuleGT10 = (frame[26]<<8) | frame[27];

    return 0;
}

uint8_t _decode_service_frame(enum serviceFrameType *frameType, uint8_t* frame){
    if (_check_service_frame_valid(frame) == 0){
        return 1;
    }

    uint8_t activeConfirmationFrame[] =     {0x00,0x04,0xe1,0x01};
    uint8_t passiveConfirmationFrame[] =    {0x00,0x04,0xe1,0x00};
    uint8_t sleepConfirmationFrame[] =      {0x00,0x04,0xe4,0x00};

    if(!memcmp(&frame[2],activeConfirmationFrame,4)){
        *frameType = activeConfirm;
    } else if(!memcmp(&frame[2],passiveConfirmationFrame,4)){
        *frameType = passiveConfirm;
    } else if (!memcmp(&frame[2],sleepConfirmationFrame,4)){
        *frameType = sleepConfirm;
    } else {
        return 1;
    }
    return 0;    
}

/**
 * The rx handler, decode recived frames, and send messages to the event loop when something happened.
 */
static void _pms7003_rx_handler(void *arg, uint8_t data){
    (void) arg;
    static uint8_t currentFrame[RX_FRAME_SIZE] = {};
    static uint8_t framePointer = 0;

    currentFrame[framePointer++] = data;
    if(framePointer == 8){
        enum serviceFrameType type;
        if(!_decode_service_frame(&type, currentFrame)){
            msg_t msg;

            if (type == passiveConfirm){
                msg.type = MSG_TYPE_PMS_RECEIVED_PASSIVE_CONFIRM;
            } else if (type == sleepConfirm){
                msg.type = MSG_TYPE_PMS_RECEIVED_SLEEP_CONFIRM;
            } else if (type == activeConfirm){
                msg.type = MSG_TYPE_PMS_RECEIVED_ACTIVE_CONFIRM;
            } else {
                msg.type = MSG_TYPE_PMS_RECIEVED_ERROR;
            } 
            msg_send(&msg, pms7003_pid);
            framePointer=0;
        }
    }

    if(framePointer >= RX_FRAME_SIZE){     
        //TODO: put mutex on lastMesure
        if(!_decode_data_frame(&lastMesure, currentFrame)){
            msg_t msg;
            msg.type = MSG_TYPE_PMS_RECEIVED_DATA;
            //msg.content.value = ;
            msg_send(&msg, pms7003_pid);
        } else {
            msg_t msg;
            msg.type = MSG_TYPE_PMS_RECIEVED_ERROR;
            msg_send(&msg, pms7003_pid);
        }
        framePointer = 0;
    }
}

static inline enum state _pms7003_handle_error(char* debugMessage){
    DEBUG("[pms7003] FAIL : %s\n[pms7003] Reinitialization ...", debugMessage);
    uart_write(USED_UART, wakeupFrame, 7);
    return initialization;
}

void* _pms7003_event_loop(void *arg){
    (void) arg;

    static ztimer_t backIntoSleepModeTimer = {0};

    msg_init_queue(rcv_queue, RCV_QUEUE_SIZE);

    while (1) {
        msg_t msg;
        msg_receive(&msg);

        switch (msg.type)
        {
        case MSG_TYPE_INIT_SENSOR:
            switch (currentState)
            {
            case uninitialized:
                DEBUG("[pms7003] Initialization...\n");
                uart_write(USED_UART, wakeupFrame, 7);
                useTheSleepMode = msg.content.value;

                if(msg.content.value){
                    DEBUG("[pms7003] Init in powersave mode\n");
                } else {
                    DEBUG("[pms7003] Init without powersave\n");
                }

                currentState=initialization;
                
                break;
            default:
                DEBUG("[pms7003] WARNING : Sensor was already initialized, action ignored\n");
                break;
            }
            break;
        
        case MSG_TYPE_PMS_RECEIVED_DATA:
            DEBUG("[pms7003] Received data from sensor\n");
            switch (currentState)
            {
            case initialization:
                if(useTheSleepMode){
                    uart_write(USED_UART, sleepFrame, 7);
                    currentState = sleepingNotConfirmed;
                } else {
                    uart_write(USED_UART, passiveModeFrame, 7);
                    currentState = passiveNotConfirmed;
                }
                break;

            case exitingSleep:
                uart_write(USED_UART, passiveModeFrame, 7);
                currentState = passiveNotConfirmed;
                break;

            case readAsked:
                kernel_pid_t respondTo;
                if(queue_pop_pid(&respondTo)){
                    DEBUG("[pms7003] WARNING : Data was read for user but no users waiting\n");
                } else {
                    DEBUG("[pms7003] Sent data to user thread\n");
                    msg_t msgSend;
                    msg.type = EVENT_LOOP_RESPONSE_SUCCESS;
                    msgSend.content.ptr=&lastMesure;
                    msg_send(&msgSend, respondTo);
                }
                if(useTheSleepMode){
                    msg_t msgSend;
                    msgSend.type = MSG_TYPE_TIMER_SLEEP_TIMEOUT;

                    if(ztimer_remove(ZTIMER_MSEC, &backIntoSleepModeTimer)){
                        DEBUG("[pms7008] Timer to go back to sleep is resetting...");
                    }

                    ztimer_set_msg(ZTIMER_MSEC, &backIntoSleepModeTimer, TIME_BEFORE_GOING_BACK_TO_SLEEP_SEC*1000, &msgSend, pms7003_pid);
                    DEBUG("[pms7003] timer to go back to sleep is reset\n");
                }
                
                if(!queue_empty_pid()){
                    msg_t msgReadAgain;
                    msgReadAgain.type = MSG_TYPE_READ_SENSOR_DATA;
                    msg_try_send(&msgReadAgain, pms7003_pid);
                }
                currentState=readReady;
                break;
            default:
                _pms7003_handle_error("Unexpected data read from sensor");
                break;
            }
            break;

        case MSG_TYPE_PMS_RECEIVED_SLEEP_CONFIRM:
            DEBUG("[pms7003] Received Sleep confirm from sensor\n");
            switch (currentState)
            {
            case sleepingNotConfirmed:
                DEBUG("[pms7003] Sensor now sleeping\n");
                currentState = sleeping;
                break;
            default:
                currentState = _pms7003_handle_error("SLEEP CONFIRMED BUT NOT ASKED...");
                break;
            }
            break;

        case MSG_TYPE_PMS_RECEIVED_PASSIVE_CONFIRM:
            DEBUG("[pms7003] Received passive confirm from sensor\n");
            switch (currentState)
            {
            case passiveNotConfirmed:
                msg_t msgSend;
                msgSend.type = MSG_TYPE_TIMER_VALID_DATA;
                ztimer_t timer = {0};
                ztimer_set_msg(ZTIMER_MSEC, &timer, VALID_DATA_AFTER_WAKEUP_SEC*1000, &msgSend, pms7003_pid); //TODO : change to seconds for better sleep?
                DEBUG("[pms7003] now in passive mode, it will be ready in x seconds\n");
                currentState = passive;
                break;
            
            default:
                currentState = _pms7003_handle_error("PASSIVE CONFIRMED BUT NOT ASKED");
                break;

            }
            break;

        case MSG_TYPE_PMS_RECEIVED_ACTIVE_CONFIRM:
            DEBUG("[pms7003] Received active confirm from sensor\n");
            break;

        case MSG_TYPE_PMS_RECIEVED_ERROR:
            currentState = _pms7003_handle_error("Received error from rx handler");
            break;

        case MSG_TYPE_TIMER_VALID_DATA:
            DEBUG("[pms7003] Received timer event valid data\n");
            switch (currentState)
            {
            case passive:
                DEBUG("[pms7003] Sensor is ready\n");
                
                if(queue_empty_pid()){
                    DEBUG("[pms7003] No users waiting");
                } else {
                    msg_t msgRead;
                    msgRead.type = MSG_TYPE_READ_SENSOR_DATA;
                    if(!msg_try_send(&msgRead, pms7003_pid)){
                        DEBUG("[pms7003] WARNING : Could send read sensor data to self");
                    }
                    DEBUG("[pms7003] An user waiting, asked read");
                }
                currentState=readReady;
                break;
            
            default:
                currentState = _pms7003_handle_error("Unexpected data valid event");
                break;
            }
            break;

        case MSG_TYPE_TIMER_SLEEP_TIMEOUT:
            DEBUG("[pms7003] Received timer event sleep\n");
            switch (currentState)
            {       
            case readReady:
                uart_write(USED_UART, sleepFrame, 7);
                currentState=sleepingNotConfirmed;
                break;

            default:
                currentState  = _pms7003_handle_error("Unexpected sleep timeout event");
                break;
            }
            break;

        case MSG_TYPE_READ_SENSOR_DATA:
            DEBUG("[pms7003] Received read event\n");
            switch (currentState){
                case readReady:
                    uart_write(USED_UART, readFrame, 7);
                    currentState = readAsked;
                    break;
                default:
                    currentState = _pms7003_handle_error("Read sensor event but not ready");
                    break;
            }
            break;
        
        case MSG_TYPE_USER_READ_SENSOR_DATA:
            DEBUG("[pms7003] Received read event from user\n");
            
            //Saving the user pid to rely them later
            if(queue_push_pid(msg.sender_pid)){
                DEBUG("[pms7003] user read event could not be added, queue full!\n");
                msg_t msgSend;
                msg.type = EVENT_LOOP_RESPONSE_ERROR;
                msgSend.content.ptr = NULL;
                msg_send(&msgSend, msg.sender_pid);
            } else {
                DEBUG("[ pms7003] user read event added to queue\n");
            }

            //if in read mode, fire a read event
            if(currentState == readReady){
                msg_t msgRead;
                msgRead.type = MSG_TYPE_READ_SENSOR_DATA;
                msg_try_send(&msgRead, pms7003_pid);
            }

            //in in sleeping mode, waking up and change state
            if(currentState == sleeping){
                uart_write(USED_UART, wakeupFrame, 7);
                currentState = exitingSleep;
            }
            
            break;
        
        default:
            DEBUG("[pms7003] UNKNOWN event, ignoring...\n");
            break;
        }
    }
}

//---------USER METHODS--------

void pms7003_init(uint8_t useSleepMode){    
    uart_init(USED_UART,9600, _pms7003_rx_handler, NULL);

    pms7003_pid = thread_create(pms7003_thread_stack,
                                        sizeof(pms7003_thread_stack),
                                        THREAD_PRIORITY_MAIN - 1,
                                        THREAD_CREATE_STACKTEST,
                                        _pms7003_event_loop, NULL,
                                        "pms7003_thread");

    msg_t msg;
    msg.type = MSG_TYPE_INIT_SENSOR;
    msg.content.value = useSleepMode;
    msg_send(&msg, pms7003_pid);
}

void pms7003_print(struct pms7003Data *data){
    printf("\n====PMS DATA====\n\
    ---Concentrations---\n\
    pm1.0 Standard : %i\n\
    pm2_5 Standard : %i\n\
    pm10 Standard : %i\n\
    \n\
    pm1_0 Atmospheric : %i\n\
    pm2_5 Atmospheric : %i\n\
    pm10 Atmospheric : %i\n\
    \n\
    ---Particles---\n\
    >=0.3 : %i\n\
    >=0.5 : %i\n\
    >=1.0 : %i\n\
    >=2.5 : %i\n\
    >=5.0 : %i\n\
    >= 10 : %i\n",
    data->pm1_0Standard,
    data->pm2_5Standard,
    data->pm10Standard,
    data->pm1_0Atmospheric,
    data->pm2_5Atmospheric,
    data->pm10Atmospheric,
    data->particuleGT0_3,
    data->particuleGT0_5,
    data->particuleGT1_0,
    data->particuleGT2_5,
    data->particuleGT5_0,
    data->particuleGT10);
}

uint8_t pms7003_measure(struct pms7003Data *data){
    msg_t msgSend;
    msgSend.type = MSG_TYPE_USER_READ_SENSOR_DATA;

    if(pms7003_pid==0){
        return 1;
    }
    msg_send(&msgSend, pms7003_pid);

    msg_t msgRecieve;
    msg_receive(&msgRecieve);

    memcpy(data, msgRecieve.content.ptr, sizeof(struct pms7003Data));
    return 0;
}
