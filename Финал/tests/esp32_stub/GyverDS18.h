#pragma once
#include <stdint.h>
struct GyverDS18Single{ GyverDS18Single(int,bool){} bool requestTemp(){return true;} bool ready(){return true;} bool readTemp(){return true;} float getTemp(){return 0;} bool isWaiting(){return false;} bool setResolution(int){return true;} bool online(){return true;} bool initialized(){return true;} void begin(){} bool tick(){return false;} int getTempInt(){return 0;} };
