#pragma once
#include <stdint.h>
#include <stddef.h>
struct Preferences{bool begin(const char*,bool=false){return true;} void end(){} bool clear(){return true;} bool remove(const char*){return true;} bool isKey(const char*){return false;}
 template<class T> T getChar(const char*,T d){return d;} int8_t getChar(const char*,int8_t d=0){return d;}
 uint8_t getUChar(const char*,uint8_t d=0){return d;} uint16_t getUShort(const char*,uint16_t d=0){return d;} int16_t getShort(const char*,int16_t d=0){return d;}
 uint32_t getUInt(const char*,uint32_t d=0){return d;} int32_t getInt(const char*,int32_t d=0){return d;} unsigned long getULong(const char*,unsigned long d=0){return d;} long getLong(const char*,long d=0){return d;}
 float getFloat(const char*,float d=0){return d;} double getDouble(const char*,double d=0){return d;} bool getBool(const char*,bool d=false){return d;} uint64_t getULong64(const char*,uint64_t d=0){return d;}
 size_t getBytesLength(const char*){return 0;} size_t getBytes(const char*,void*,size_t){return 0;}
 template<class T> size_t putChar(const char*,T){return 1;} template<class T> size_t putUChar(const char*,T){return 1;} template<class T> size_t putUShort(const char*,T){return 2;} template<class T> size_t putShort(const char*,T){return 2;}
 template<class T> size_t putUInt(const char*,T){return 4;} template<class T> size_t putInt(const char*,T){return 4;} template<class T> size_t putULong(const char*,T){return 4;} template<class T> size_t putLong(const char*,T){return 4;}
 template<class T> size_t putFloat(const char*,T){return 4;} template<class T> size_t putDouble(const char*,T){return 8;} template<class T> size_t putBool(const char*,T){return 1;} template<class T> size_t putULong64(const char*,T){return 8;}
 size_t putBytes(const char*,const void*,size_t n){return n;} };
