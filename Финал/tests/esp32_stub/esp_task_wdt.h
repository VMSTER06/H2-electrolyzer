#pragma once
#include <esp_timer.h>
struct esp_task_wdt_config_t{int timeout_ms; int idle_core_mask; bool trigger_panic;};
inline esp_err_t esp_task_wdt_reconfigure(esp_task_wdt_config_t*){return 0;} inline esp_err_t esp_task_wdt_init(esp_task_wdt_config_t*){return 0;}
