#include <stdio.h>
#include <string.h>
#include "board.h"
#include "periph/uart.h"
#include "pms7003_driver.h"

static struct pms7003Data lastMesure;
static uint8_t initilized = 0;

#define FRAME_SIZE 32
static char currentFrame[FRAME_SIZE];

uint8_t pms7003_checkFrame(char* frame){
    uint16_t checksum = 0;
    for(uint8_t i = 0; i<FRAME_SIZE-2; i++){
        checksum+=frame[i];
    }
    return (checksum==((frame[30]<<8)|frame[31]));
}

void pms7003_print(struct pms7003Data *data){
    if (pms7003_checkFrame(currentFrame) == 0){
        printf("Warning : incorrect checksum\nprinting last correct mesure\n\n");
    }

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

uint8_t pms7003_decode(struct pms7003Data *data ,char* frame){
    if (pms7003_checkFrame(frame) == 0){
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

/**
 * The rx handler that fills in the currentFrame
 */
static void pms7003_rx_handler(void *arg, uint8_t data){
    (void) arg;
    static uint8_t curr = 0;
    currentFrame[curr++] = data;
    if(curr >= FRAME_SIZE){     
        //TODO: put mutex on lastMesure, so it is not used by pms7003_measure in the same time
        if(!pms7003_decode(&lastMesure, currentFrame)){
            //TODO: handle errors
        }
        curr = 0;
    }
}

void pms7003_init(void){    
    uart_init(UART_DEV(1),9600, pms7003_rx_handler, NULL);
    initilized = 1;
}

uint8_t pms7003_measure(struct pms7003Data *data){
    if(!initilized){
        return 1;
    }
    memcpy(data, &lastMesure, sizeof(struct pms7003Data));
    return 0;
}
