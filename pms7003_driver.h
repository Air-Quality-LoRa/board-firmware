#include <stdio.h>
#include <string.h>
#include "board.h"
#include "periph/uart.h"
#include "stdio_uart.h"
#include "msg.h"
#include "thread.h"

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


void pms7003_print(struct pms7003Data *data);

// int pms7003_checkFrame(char* frame);

void pms7003_decode(struct pms7003Data *data ,char* frame);

void pms7003_init(void);

int pms7003_measure(struct pms7003Data *data);