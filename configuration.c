#include <stdio.h>
#include <string.h>
#include <board.h>
#include <periph/flashpage.h>
#include <semtech_loramac.h>
#include "configuration.h"

uint8_t deveui[LORAMAC_DEVEUI_LEN];
uint8_t appeui[LORAMAC_APPEUI_LEN];
uint8_t appkey[LORAMAC_APPKEY_LEN];

enum sendInterval uplinkInterval;
enum dataToSend dataToSend;
enum eccFrameFrequency eccFrameFrequency;

static uint8_t flashPage[2048] __attribute__ ((aligned (FLASHPAGE_WRITE_BLOCK_ALIGNMENT))) = {0};

static void printHexArray(const uint8_t* data, uint8_t length){
    for(uint8_t dataPtr = 0; dataPtr<length; dataPtr++ ){
        printf("%02X ", data[dataPtr]);
    }
}

static inline uint8_t convertSymbolToHalfByte(char symbol){
    uint8_t ret = 0;
    if(symbol>47 && symbol<58){
        ret = symbol-48;
    } else if(symbol>64 && symbol<71){
        ret = symbol-55;
    } else if(symbol>96 && symbol<103){
        ret = symbol-87;
    }
    return ret;
}

static void askHexString(uint8_t* dataPtr, uint8_t len)
{
    fflush(stdin);
    uint8_t state = 0;
    for(uint8_t bytePtr = 0;bytePtr<len;){
        int c = getchar();
        if((c>47 && c<58) || (c>64 && c<71) || (c>96 && c<103)){
            switch (state)
            {
            case 0:
                state = 1;
                dataPtr[bytePtr] = 0 | convertSymbolToHalfByte(c)<<4;
                putchar(c);
                fflush(stdout);
                break;
            case 1:
                state = 0;
                dataPtr[bytePtr++] |= convertSymbolToHalfByte(c);
                putchar(c);
                putchar(' ');
                fflush(stdout);
                break;
            default:
                break;
            }
        }
    }
    putchar('\n');

}

void loadConfig(void){
    flashpage_read(127, flashPage);
    
    memcpy(deveui,flashPage,                                        LORAMAC_DEVEUI_LEN);
    memcpy(appeui,flashPage+LORAMAC_DEVEUI_LEN,                     LORAMAC_APPEUI_LEN);
    memcpy(appkey,flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN,  LORAMAC_APPKEY_LEN);
}

void printConfig(void){
    printf("Current loaded configuration is:");
    printf("\ndeveui : ");
    printHexArray(deveui, LORAMAC_DEVEUI_LEN);
    printf("\nappeui : ");
    printHexArray(appeui, LORAMAC_APPEUI_LEN);
    printf("\nappkey : ");
    printHexArray(appkey, LORAMAC_APPKEY_LEN);

    printf("\n");
}

void interactiveConfig(void){
    printf("Enter hexadecimal values\n");
    printf("Example: 01 02 70 A7 F3 55 7E B8\n");
    
    printf("\ndeveui : \n");
    askHexString(deveui, LORAMAC_DEVEUI_LEN);

    printf("\nappeui : \n");
    askHexString(appeui, LORAMAC_APPEUI_LEN);

    printf("\nappkey : \n");
    askHexString(appkey, LORAMAC_APPKEY_LEN);
}

void saveConfig(void){
    memcpy(flashPage,                                       deveui, LORAMAC_DEVEUI_LEN);
    memcpy(flashPage+LORAMAC_DEVEUI_LEN,                    appeui, LORAMAC_APPEUI_LEN);
    memcpy(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN, appkey, LORAMAC_APPKEY_LEN);
    flashpage_write_page(127, flashPage);
}
