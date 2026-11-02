#pragma once
#include <Arduino.h>
struct IPAddress{ IPAddress(int=0,int=0,int=0,int=0){} const char* toString()const{return "";} operator uint32_t()const{return 0;} };
enum{WIFI_AP};
struct APc{bool enableDhcpCaptivePortal(){return true;}};
struct WiFiC{ APc AP; bool softAP(const char*,const char* =nullptr,int=1,int=0,int=4){return true;} IPAddress softAPIP(){return IPAddress();} void mode(int){} bool softAPConfig(IPAddress,IPAddress,IPAddress){return true;} void setTxPower(int){} void setSleep(bool){} int softAPgetStationNum(){return 0;} };
extern WiFiC WiFi;
