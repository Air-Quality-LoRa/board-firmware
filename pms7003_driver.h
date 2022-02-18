/**
 * Data mesured by the pms7003 sensor
 */
struct pms7003Data {
    uint16_t pm1_0Standard;
    uint16_t pm2_5Standard;
    uint16_t pm10Standard;

    uint16_t pm1_0Atmospheric;
    uint16_t pm2_5Atmospheric;
    uint16_t pm10Atmospheric;

    uint16_t particuleGT0_3;
    uint16_t particuleGT0_5;
    uint16_t particuleGT1_0;
    uint16_t particuleGT2_5;
    uint16_t particuleGT5_0;
    uint16_t particuleGT10;
};

enum state{
    uninitialized, 
    initialization, 
    sleepingNotConfirmed, 
    sleeping, 
    passiveNotConfirmed, 
    exitingSleep, 
    passive, 
    readReady,
    readAsked };

enum serviceFrameType{passiveConfirm, activeConfirm, sleepConfirm};

/**
 * @name    Definitions for messages received by the event loop
 */
#define MSG_TYPE_INIT_SENSOR 0x1
#define MSG_TYPE_PMS_RECEIVED_DATA 0x2
#define MSG_TYPE_PMS_RECEIVED_SLEEP_CONFIRM 0x3
#define MSG_TYPE_PMS_RECEIVED_PASSIVE_CONFIRM 0x4
#define MSG_TYPE_PMS_RECEIVED_ACTIVE_CONFIRM 0x5
#define MSG_TYPE_PMS_RECIEVED_ERROR 0x6
#define MSG_TYPE_TIMER_VALID_DATA 0x7
#define MSG_TYPE_TIMER_SLEEP_TIMEOUT 0x8
#define MSG_TYPE_READ_SENSOR_DATA 0x9
#define MSG_TYPE_USER_READ_SENSOR_DATA 0xa

/**
 * @name    Definitions for response messages from the pms thread to the main thread 
 */
#define EVENT_LOOP_RESPONSE_SUCCESS 0x0
#define EVENT_LOOP_RESPONSE_ERROR 0x1


/**
 * Print pms data in formatted way
 * @param data a pointer to a psm7003Data to display 
 */
void pms7003_print(struct pms7003Data *data);

/**
 * Init the uart 1 (or rx2,tx2 on the card)
 * @param useSleepMode set to true if you want to set the sensor sleep when not in use
 */
void pms7003_init(uint8_t useSleepMode);

// /**
//  * Get the last valid mesure. 
//  * @param data a pointer to the pms7003Data to fill in
//  * @return 1 if pms was not initialised and data not filled in, 0 if everything went well
//  */ 
// uint8_t pms7003_measure(struct pms7003Data *data);