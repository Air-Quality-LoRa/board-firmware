#include <stdio.h>
#include <string.h>
#include "board.h"
#include "periph/uart.h"
#include "stdio_uart.h"
#include "msg.h"
#include "thread.h"
#include "pms7003_driver.h"

struct pms7003Data lastMesure;

#define FRAME_SIZE 32
char currentFrame[FRAME_SIZE];
uint8_t curr;

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

int pms7003_checkFrame(char* frame){
    (void) frame;
    return 0;
}


void pms7003_decode(struct pms7003Data *data ,char* frame){
    // if (!pms7003_checkFrame(frame)){
    //     //return 1;
    // }

    memcpy(data, &frame[4], 24);

    // data->pm1_0Standard = (frame[6]<<8) | frame[5];
    // data->pm2_5Standard = (frame[8]<<8) | frame[7];
    // data->pm10Standard = (frame[10]<<8) | frame[9];

    // data->pm1_0Atmospheric = (frame[12]<<8) | frame[11];
    // data->pm2_5Atmospheric = (frame[14]<<8) | frame[13];
    // data->pm10Atmospheric = (frame[16]<<8) | frame[15];

    // data->particuleGT0_3 = (frame[18]<<8) | frame[17];
    // data->particuleGT0_5 = (frame[20]<<8) | frame[19];
    // data->particuleGT1_0 = (frame[22]<<8) | frame[21];
    // data->particuleGT2_5 = (frame[24]<<8) | frame[23];
    // data->particuleGT5_0 = (frame[26]<<8) | frame[25];
    // data->particuleGT10 = (frame[28]<<8) | frame[27];

    // // return 0;
}

static void pms7003_rx_handler(void *arg, uint8_t data)
{
    (void) arg;
    currentFrame[curr++] = data;
    // printf("%02X\n", data);
    if(curr >= FRAME_SIZE){
        // if(!pms_7003_decode(&lastMesure, currentFrame)){
        //     //TODO: how to handle errors
        // }
        pms7003_decode(&lastMesure, currentFrame);
        curr = 0;
    }
}

void pms7003_init(void){    
    uart_init(UART_DEV(1),9600, pms7003_rx_handler, NULL);
    curr = 0;
}

int pms7003_measure(struct pms7003Data *data){
    memcpy(data, &lastMesure, sizeof(struct pms7003Data));
    return 0;
}
