#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <functional>
#include <string>
struct StringS{ std::string s; StringS(const char*x=""):s(x){} StringS(const std::string&x):s(x){} StringS(const char*x,size_t n):s(x,n){} int indexOf(const char*o)const{auto r=s.find(o);return r==std::string::npos?-1:(int)r;} int indexOf(char c)const{auto r=s.find(c);return r==std::string::npos?-1:(int)r;} StringS substring(int a,int b=-1)const{return StringS(b<0?s.substr(a):s.substr(a,b-a));} void remove(int a,int n=-1){s.erase(a,n<0?std::string::npos:n);} bool operator==(const StringS&o)const{return s==o.s;} const char* c_str()const{return s.c_str();} size_t length()const{return s.size();} bool operator==(const char*o)const{return s==o;} bool startsWith(const char*o)const{return s.rfind(o,0)==0;} int toInt()const{return atoi(s.c_str());} float toFloat()const{return atof(s.c_str());} bool endsWith(const char*o)const{return s.size()>=strlen(o)&&s.compare(s.size()-strlen(o),strlen(o),o)==0;} StringS& operator+=(const char*o){s+=o;return *this;} StringS operator+(const char*o)const{return StringS(s+o);} };
#define String StringS
enum WebRequestMethod{HTTP_GET=1,HTTP_POST=2,HTTP_ANY=255};
struct AsyncWebParameter{ String v; const String& value()const{return v;} const String& name()const{return v;} };
struct AsyncWebServerResponse{ void addHeader(const char*,const char*){} };
struct AsyncResponseStream: AsyncWebServerResponse, Print{ size_t write(uint8_t){return 1;} using Print::write; };
struct AsyncWebServerRequest;
typedef std::function<void(AsyncWebServerRequest*)> ArRequestHandlerFunction;
typedef std::function<void(AsyncWebServerRequest*,const String&,size_t,uint8_t*,size_t,bool)> ArUploadHandlerFunction;
typedef std::function<void(AsyncWebServerRequest*,uint8_t*,size_t,size_t,size_t)> ArBodyHandlerFunction;
struct AsyncWebServerRequest{
  bool hasParam(const char*,bool=false,bool=false)const{return false;}
  const AsyncWebParameter* getParam(const char*,bool=false,bool=false)const{return nullptr;}
  void send(int,const char* =nullptr,const char* =nullptr){} void send(int,const char*,const uint8_t*,size_t){} void send(int,const char*,const String&){} void send(AsyncWebServerResponse*){}
  AsyncResponseStream* beginResponseStream(const char*,size_t=1460){return new AsyncResponseStream;}
  AsyncWebServerResponse* beginResponse(int,const char*,const uint8_t*,size_t){return new AsyncWebServerResponse;}
  AsyncWebServerResponse* beginResponse(int,const char*,const char*){return new AsyncWebServerResponse;}
  AsyncWebServerResponse* beginResponse(LittleFSc&,const char*,const char*){return new AsyncWebServerResponse;}
  void onDisconnect(std::function<void()>){}
  void redirect(const char*){} String host()const{return String();} String url()const{return String();}
};
struct AsyncWebHandler{};
struct AsyncWebSocketClient{ uint32_t id(){return 0;} void text(const char*){} void text(const String&){} };
enum AwsEventType{WS_EVT_CONNECT,WS_EVT_DISCONNECT,WS_EVT_DATA,WS_EVT_PONG,WS_EVT_ERROR};
struct AsyncWebSocket: AsyncWebHandler{ AsyncWebSocket(const char*){} 
  void onEvent(std::function<void(AsyncWebSocket*,AsyncWebSocketClient*,AwsEventType,void*,uint8_t*,size_t)>){}
  void textAll(const char*){} void textAll(const String&){} size_t count(){return 0;} void cleanupClients(){} };
struct AsyncWebServer{ AsyncWebServer(int){}
  void on(const char*,int,ArRequestHandlerFunction){} void on(const char*,int,ArRequestHandlerFunction,ArUploadHandlerFunction){}
  void on(const char*,int,ArRequestHandlerFunction,ArUploadHandlerFunction,ArBodyHandlerFunction){}
  void onNotFound(ArRequestHandlerFunction){} void begin(){} void addHandler(AsyncWebHandler*){} };

struct AwsFrameInfo{ bool final; uint64_t index, len; int opcode; };
enum{WS_TEXT=1};
