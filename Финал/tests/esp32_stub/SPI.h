#pragma once
#include <stdint.h>
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings{SPISettings(long,int,int){}};
struct SPIc{void begin(){} void begin(int,int,int,int){} void transfer(uint8_t){} void beginTransaction(SPISettings){} void endTransaction(){}}; extern SPIc SPI;
