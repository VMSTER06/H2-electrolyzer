#pragma once
#include <Arduino.h>
#define FILE_READ "r"
#define FILE_WRITE "w"
struct File{ bool seek(size_t){return true;} size_t position(){return 0;} int available(){return 0;} size_t print(const char*){return 0;} size_t printf(const char*,...){return 0;} void flush(){} int read(){return -1;} const char* name(){return "";} bool isDirectory(){return false;} File openNextFile(){return File();} explicit operator bool() const {return false;} size_t size(){return 0;} size_t read(uint8_t*,size_t){return 0;} size_t write(const uint8_t*,size_t){return 0;} void close(){} };
struct LittleFSc{ size_t totalBytes(){return 0;} size_t usedBytes(){return 0;} bool rename(const char*,const char*){return true;} bool format(){return true;}bool begin(bool){return true;} bool exists(const char*){return false;} bool mkdir(const char*){return true;} File open(const char*,const char*,bool=false){return File();} bool remove(const char*){return true;}};
extern LittleFSc LittleFS;
