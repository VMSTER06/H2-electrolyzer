#pragma once
#include <Arduino.h>
struct TwoWire{void begin(){} void begin(int,int){} void beginTransmission(int){} int endTransmission(bool=true){return 0;} void write(uint8_t){} int requestFrom(int,int){return 0;} int read(){return 0;} void setClock(long){}};
extern TwoWire Wire;
