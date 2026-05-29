#pragma once

#include <cstdint>
#include <pthread.h>

bool esp_init();
void* esp_thread(void* arg);
void esp_set_game_data(void* gameData);
