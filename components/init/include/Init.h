#pragma once

#include "driver/rmt.h"
#include "driver/gpio.h"
#include <stdint.h>

// WS2812 intégré
#define LED_PIN GPIO_NUM_21
#define RMT_CHANNEL RMT_CHANNEL_0

// Fonctions WS2812
void ws2812_init(void);
void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b);

