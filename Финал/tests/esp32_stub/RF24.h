#pragma once
#include <Arduino.h>
enum{RF24_PA_MAX,RF24_2MBPS,RF24_CRC_16};
struct RF24{RF24(int,int){} bool begin(){return true;} void openWritingPipe(const uint8_t*){} void openReadingPipe(int,const uint8_t*){} void setPALevel(int){} void setDataRate(int){} void setCRCLength(int){} void setRetries(int,int){} void setChannel(int){} void startListening(){} void stopListening(){} bool write(const void*,uint8_t){return true;} bool available(){return false;} uint8_t getPayloadSize(){return 32;} void read(void*,uint8_t){} void powerDown(){} void flush_rx(){} void flush_tx(){} bool testRPD(){return false;} bool isChipConnected(){return true;} uint8_t getChannel(){return 0;}};
