#pragma once
#include <Arduino.h>
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#define U_FLASH 0
struct UpdateC{ bool begin(size_t=0,int=0){return true;} size_t write(uint8_t*,size_t n){return n;} bool end(bool=false){return true;} void abort(){} void printError(Print&){} bool isRunning(){return false;} bool hasError(){return false;} };
extern UpdateC Update;
