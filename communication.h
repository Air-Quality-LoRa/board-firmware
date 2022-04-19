// TODO : remove
extern semtech_loramac_t loramac;
extern sx126x_t sx126x;

#define DOWNLINK_REQUEST_CONFIG_PORT 1
#define DOWNLINK_SET_CONFIG_PORT 2

/**
 * Configure lora and blocks until the network is joined on the highest datarate that is working.
 * It automatically set adaptative datarate, disable confirmed uplinks.
 */
void loraJoin(void);

/**
 * Continuously probe the server for a configuration util it is given...
 */
void loraGetConfigurationFromNetwork(void);

/**
 * Give the datarate between 6 and 0
 */
uint8_t loraGetDatarate(void);

/**
 * Send a frame, automatically select the right port
 * @param data the data to send
 * @param type 0 for normal packet, 1 for ecc packet
 */
void loraSendData(uint8_t data[], uint8_t type);