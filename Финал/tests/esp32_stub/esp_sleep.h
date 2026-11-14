#pragma once
#include <stdint.h>
typedef int gpio_num_t; enum{ESP_SLEEP_WAKEUP_UNDEFINED,ESP_SLEEP_WAKEUP_EXT0,ESP_SLEEP_WAKEUP_EXT1}; enum{ESP_EXT1_WAKEUP_ALL_LOW};
inline int esp_sleep_get_wakeup_cause(){return 0;} inline void esp_sleep_enable_ext0_wakeup(gpio_num_t,int){} inline void esp_sleep_enable_ext1_wakeup(uint64_t,int){} inline void esp_deep_sleep_start(){}
