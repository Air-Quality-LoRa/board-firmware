#include "board.h"

void printHexArray(const uint8_t* data, uint8_t length){
    for(uint8_t dataPtr = 0; dataPtr<length; dataPtr++ ){
        printf("%02X ", data[dataPtr]);
    }
}