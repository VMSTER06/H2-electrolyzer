#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
typedef uint8_t byte;
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define bitRead(v,b) (((v)>>(b))&1)
#define bitWrite(v,b,x) ((x)?((v)|=(1UL<<(b))):((v)&=~(1UL<<(b))))
#define constrain(a,l,h) ((a)<(l)?(l):((a)>(h)?(h):(a)))
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
#define ESP32 1
#define ESP_ARDUINO_VERSION_MAJOR 3
#define RTC_DATA_ATTR
inline void pinMode(int,int){} inline void delay(int){} inline void fastWrite(int,int){} inline void digitalWrite(int,int){} inline int digitalRead(int){return 1;}
inline void delayMicroseconds(int){}
unsigned long millis();
inline char* dtostrf(double v,signed char w,unsigned char p,char*b){sprintf(b,"%*.*f",w,p,v);return b;}
inline bool ledcAttach(int,int,int){return true;} inline void ledcWrite(int,int){} inline bool ledcChangeFrequency(int,int,int){return true;}
inline void enableLoopWDT(){}
class String{public: String(const char*s=""){} };
class Print{public: virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t*b,size_t n){for(size_t i=0;i<n;i++)write(b[i]);return n;}
 size_t print(const char*s){return write((const uint8_t*)s,strlen(s));}
 size_t print(int v){char b[16];sprintf(b,"%d",v);return print(b);}
 size_t println(const char*s=""){print(s);return print("\n");} template<class T> size_t println(const T&){return 1;} template<class T> size_t print(const T&){return 1;}
 size_t printf(const char*f,...)__attribute__((format(printf,2,3))){char b[256];va_list a;va_start(a,f);int n=vsnprintf(b,sizeof b,f,a);va_end(a);return write((const uint8_t*)b,n);}
 void begin(long){} void flush(){} };
struct SerialC: Print{ size_t write(uint8_t){return 1;} }; extern SerialC Serial;
// ── дополнительно для мозга ──
#include <functional>
#include <string>
#define ARDUINO_RUNNING_CORE 1
#define INPUT_PULLDOWN 3
#define ANALOG 4
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffff
typedef uint32_t TickType_t; typedef void* SemaphoreHandle_t; typedef void* QueueHandle_t; typedef void* TaskHandle_t; typedef int BaseType_t;
struct portMUX_TYPE{int x;}; 
#define portMUX_INITIALIZER_UNLOCKED {0}
inline void portENTER_CRITICAL(portMUX_TYPE*){} inline void portEXIT_CRITICAL(portMUX_TYPE*){}
inline void taskENTER_CRITICAL(portMUX_TYPE*){} inline void taskEXIT_CRITICAL(portMUX_TYPE*){}
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return (void*)1;} inline int xSemaphoreTake(SemaphoreHandle_t,TickType_t){return 1;} inline int xSemaphoreGive(SemaphoreHandle_t){return 1;}
inline QueueHandle_t xQueueCreate(int,int){return (void*)1;} inline int xQueueSend(QueueHandle_t,const void*,TickType_t){return 1;} inline int xQueueReceive(QueueHandle_t,void*,TickType_t){return 0;} inline int xQueuePeek(QueueHandle_t,void*,TickType_t){return 0;} inline void xQueueReset(QueueHandle_t){}
inline TickType_t xTaskGetTickCount(){return 0;} inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,TaskHandle_t*,int){return 1;} inline void vTaskDelay(TickType_t){}
inline int analogRead(int){return 0;} inline uint32_t analogReadMilliVolts(int){return 0;} inline void analogReadResolution(int){} inline void analogSetAttenuation(int){} inline void analogSetPinAttenuation(int,int){}
#define ADC_11db 3
inline void ledcWriteTone(int,int){} inline unsigned long micros(){return 0;} inline long random(long a,long b=0){return 0;}
inline void yield(){}
inline uint32_t esp_random(){return 0;}
using std::max; using std::min;
#include <algorithm>

#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,4,2)
struct EspC{ void restart(){} uint32_t getFreeHeap(){return 0;} uint32_t getMinFreeHeap(){return 0;} uint32_t getMaxAllocHeap(){return 0;} };
extern EspC ESP;
