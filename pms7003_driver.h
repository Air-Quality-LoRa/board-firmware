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


/** 
 * Check if a frame is correct
 * @param frame the frame to check
 * @return 1 if the checksum is correct, 0 if not
 */
uint8_t pms7003_checkFrame(char* frame);

/**
 * Print pms data in formatted way
 * @param data a pointer to a psm7003Data to display 
 */
void pms7003_print(struct pms7003Data *data);

/**
 * Decode a frame receved from the psm7003 over serial.
 * @param data a pointer to the pms7003Data to fill in
 * @param frame a pointer the frame to decode
 * @return 0 if it went well, 1 if the frame is invalid (wrong checksum)
 */ 
uint8_t pms7003_decode(struct pms7003Data *data ,char* frame);

/**
 * Init the uart 1 (or rx2,tx2 on the card)
 */
void pms7003_init(void);

/**
 * Get the last valid mesure. 
 * @param data a pointer to the pms7003Data to fill in
 * @return 1 if pms was not initialised and data not filled in, 0 if everything went well
 */ 
uint8_t pms7003_measure(struct pms7003Data *data);