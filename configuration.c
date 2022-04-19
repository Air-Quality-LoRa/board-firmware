#define ENABLE_DEBUG  (1)
#include <debug.h>

#include <stdio.h>
#include <string.h>
#include <board.h>
#include <periph/flashpage.h>
#include <semtech_loramac.h>
#include "configuration.h"
#include "error.h"
#include "util.h"

uint8_t deveui[LORAMAC_DEVEUI_LEN];
uint8_t appeui[LORAMAC_APPEUI_LEN];
uint8_t appkey[LORAMAC_APPKEY_LEN];
uint8_t pmsUseAtmoshphericMesure; //true is atmoshperic, false standard
uint8_t pmsUsePowersaveMode;
uint8_t pmsNumberOfMesures;
uint8_t pmsMesurementAlgorithm; //see PMS_MESURMENT_XXX

uint8_t _sendIntervalMinutes[6];
uint8_t _eccSendInterval[6];
enum dataToSend _dataToSend[6];


static uint8_t flashPage[2048] __attribute__ ((aligned (FLASHPAGE_WRITE_BLOCK_ALIGNMENT))) = {0};

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

static void askInterractiveHexNumbers(uint8_t* dataPtr, uint8_t len)
{
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

static void askInterractiveNumber(uint8_t* number){
    char c;
    do{
        c = getchar();
    }while(c<'1' || c>'9');
    putchar(c);
    fflush(stdout);
    *number = c-'0';

    uint8_t tmpUser;
    while(*number*10<256){
        do{
            c = getchar();
        }while(!((c>='0' && c<='9') || (c=='\r' || c=='\n')));

        if (c=='\r' || c=='\n') return;
        tmpUser = c-'0';
        if(*number*10+tmpUser<256){
            putchar(c);
            fflush(stdout);
            *number = *number*10+tmpUser;
        }
    }
}

void loadConfig(void){
    flashpage_read(127, flashPage);
    
    memcpy(deveui,flashPage,                                        LORAMAC_DEVEUI_LEN);
    memcpy(appeui,flashPage+LORAMAC_DEVEUI_LEN,                     LORAMAC_APPEUI_LEN);
    memcpy(appkey,flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN,  LORAMAC_APPKEY_LEN);

    pmsUsePowersaveMode = *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN);
    pmsUseAtmoshphericMesure = *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+1);
    pmsNumberOfMesures = *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+2);
    pmsMesurementAlgorithm = *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+3);

    DEBUG("The flashpage read is :");
    #if ENABLE_DEBUG
    printHexArray(flashPage, LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+4);
    #endif
    DEBUG("\n");


}

void printConfig(void){
    printf("Current loaded configuration is:");
    //lora
    printf("\ndeveui : ");
    printHexArray(deveui, LORAMAC_DEVEUI_LEN);
    printf("\nappeui : ");
    printHexArray(appeui, LORAMAC_APPEUI_LEN);
    printf("\nappkey : ");
    printHexArray(appkey, LORAMAC_APPKEY_LEN);
    
    //pms
    printf("\npms powersaving mode : ");
    pmsUsePowersaveMode?printf("yes"):printf("no");
    printf("\npms concentration mesure : ");
    pmsUseAtmoshphericMesure?printf("atmospheric"):printf("standard");
    printf("\npms number of mesures : %i", pmsNumberOfMesures);
    printf("\npms mesurement algorithm : ");
    switch (pmsMesurementAlgorithm)
    {
    case PMS_MESURMENT_ALGORITHM_MEDIAN:
        printf("median");
        break;
    default:
    case PMS_MESURMENT_ALGORITHM_MEAN:
        printf("mean");
        break;
    }
    printf("\n");
}

void interactiveConfig(void){
    printf("Enter hexadecimal values\n");
    printf("Example: 01 02 70 A7 F3 55 7E B8\n");
    
    printf("\ndeveui : \n");
    askInterractiveHexNumbers(deveui, LORAMAC_DEVEUI_LEN);

    printf("\nappeui : \n");
    askInterractiveHexNumbers(appeui, LORAMAC_APPEUI_LEN);

    printf("\nappkey : \n");
    askInterractiveHexNumbers(appkey, LORAMAC_APPKEY_LEN);

    printf("\n\nPMS configuration:\n");
    int c;
    printf("\nUse atmospheric mesure (a) or standard mesure (s) : \n");
    do{
        c = getchar();
    }while(c!='a' && c!='s');
    putchar(c);
    putchar('\n');
    fflush(stdout);
    pmsUseAtmoshphericMesure = (c=='a');

    printf("\nUse powersave mode? (This mode stops the fan of the pms between mesures to preserve battery and reduce the fan dirtying.)\nyes (y) or no (n) : \n");
    do{
        c = getchar();
    }while(c!='y' && c!='n');
    putchar(c);
    putchar('\n');
    fflush(stdout);
    pmsUsePowersaveMode = (c=='y');

    printf("\nNumer of mesures (between 1 and 255): \n");
    askInterractiveNumber(&pmsNumberOfMesures); //See if the card have enough ram for 256 mesures

    printf("\n\nMesurement algorithm mean (0), median (1): \n");
    do{
        c = getchar();
    }while(c<'0' || c>'1');
    putchar(c);
    fflush(stdout);
    pmsMesurementAlgorithm = c-'0';
}

void saveConfig(void){
    memcpy(flashPage,                                       deveui, LORAMAC_DEVEUI_LEN);
    memcpy(flashPage+LORAMAC_DEVEUI_LEN,                    appeui, LORAMAC_APPEUI_LEN);
    memcpy(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN, appkey, LORAMAC_APPKEY_LEN);

    *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN) = pmsUsePowersaveMode;
    *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+1) = pmsUseAtmoshphericMesure;
    *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+2) = pmsNumberOfMesures;
    *(flashPage+LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+3) = pmsMesurementAlgorithm;

    DEBUG("[configuration] updated static configuration\n");

    flashpage_write_page(127, flashPage);

    DEBUG("The flashpage written is :");
    #if ENABLE_DEBUG
    printHexArray(flashPage, LORAMAC_DEVEUI_LEN+LORAMAC_APPEUI_LEN+LORAMAC_APPKEY_LEN+4);
    #endif
    DEBUG("\n");
}

void setDynamicConfig(uint8_t rawData[]){
    DEBUG("[configuration] updated dynamic configuration\n");
    for (uint8_t i = 0; i < 6; i++)
    {
        _dataToSend[i] = (rawData[i]&0x18)>>3;
        enum sendInterval _sendInterval = (rawData[i]&0xE0)>>5;
        enum eccFrameFrequency _eccFrameFrequency = rawData[i]&0x7;

        switch (_sendInterval){
            case every_5_minutes:
                _sendIntervalMinutes[i]=5;
                break;
            case every_10_minutes:
                _sendIntervalMinutes[i]=10;
                break;
            case every_15_minutes:
                _sendIntervalMinutes[i]=15;
                break;
            case every_20_minutes:
                _sendIntervalMinutes[i]=20;
                break;
            case every_30_minutes:
                _sendIntervalMinutes[i]=30;
                break;
            case every_45_minutes:
                _sendIntervalMinutes[i]=45;
                break;
            case every_60_minutes:
                _sendIntervalMinutes[i]=60;
                break;
            case every_120_minutes:
                _sendIntervalMinutes[i]=120;
                break;
            default:
                handleError("The send interval received is invalid");
                break;
        }

        if(_eccFrameFrequency == never){
            _eccSendInterval[i] = 0;
        } else {
            //perform sqrt
            _eccSendInterval[i] = 2;
            for (uint8_t j = 1; j < _eccFrameFrequency; j++){
                _eccSendInterval[i]*=2; 
            }
        }
        DEBUG("[configuration] config for datarate%d is sendIntervalMinutes=%d, eccSendInterval=%i\n", i,_sendIntervalMinutes[i], _eccSendInterval[i]);
    }
}

void getDynamicConfiguration(uint8_t datarate, uint8_t *sendIntervalMinutes, uint8_t *eccSendInterval, enum dataToSend* dataToSend){
    //datarate 6 have the same config than 5. check to prevent going out array bounds
    if (datarate>5){
        datarate=5;
    }

    if(sendIntervalMinutes != NULL){
        *sendIntervalMinutes = _sendIntervalMinutes[datarate];
    }
    if(eccSendInterval != NULL){
        *eccSendInterval = _eccSendInterval[datarate];
    }
    if(_dataToSend != NULL){
        *dataToSend = _dataToSend[datarate];
    }
}