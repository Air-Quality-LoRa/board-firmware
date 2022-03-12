#include <board.h>
#include <semtech_loramac.h>

extern uint8_t deveui[LORAMAC_DEVEUI_LEN];
extern uint8_t appeui[LORAMAC_APPEUI_LEN];
extern uint8_t appkey[LORAMAC_APPKEY_LEN];

extern enum sendInterval uplinkInterval;
extern enum dataToSend dataToSend;
extern enum eccFrameFrequency eccFrameFrequency;

enum sendInterval{
    every_5_minutes = 0x7,
    every_10_minutes = 0x6,
    every_15_minutes = 0x5,
    every_20_minutes = 0x4,
    every_30_minutes = 0x3,
    every_45_minutes = 0x2,
    every_60_minutes = 0x1,
    every_120_minutes = 0x0,
};

enum dataToSend{
    undefined = 0x0,
    pms_concentration = 0x1,
    pms_particle_units = 0x2,
    all = 0x3
};

enum eccFrameFrequency{
    never = 0x0,
    every_2_frames = 0x1,
    every_4_frames = 0x2,
    every_8_frames = 0x3,
    every_16_frames = 0x4,
    every_32_frames = 0x5,
    every_64_frames = 0x6,
    every_128_frames = 0x7,
};

/**
 * Load configuration from flash memory.
**/
void loadConfig(void);

/**
 * Display the current configuration
**/
void printConfig(void);

/**
 * Interractivly configure
**/
void interactiveConfig(void);

/**
 * Write the current configuration in the flash memory.
*/
void saveConfig(void);