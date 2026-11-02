#pragma once
#include <stdint.h>
typedef void* esp_timer_handle_t; typedef int esp_err_t; enum{ESP_OK=0,ESP_ERR_INVALID_STATE=1};
struct esp_timer_create_args_t{void(*callback)(void*); void* arg; const char* name;};
inline esp_err_t esp_timer_create(const esp_timer_create_args_t*,esp_timer_handle_t*){return 0;} inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t,uint64_t){return 0;} inline void esp_timer_stop(esp_timer_handle_t){} inline void esp_timer_delete(esp_timer_handle_t){}
