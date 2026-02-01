/*
#pragma once
#include "driver/gpio.h"
#include "esp_timer.h"
#include "rom/ets_sys.h" // pour ets_delay_us
#include <stdint.h>

class TriacController {
public:
    TriacController(gpio_num_t triacPin, gpio_num_t zcPin);

    void begin();
    void setConsigne(uint8_t pct); // 0-100%

private:
    gpio_num_t _triacPin;
    gpio_num_t _zcPin;
    volatile uint8_t _consigne;

    esp_timer_handle_t _triacTimer;

    // Singleton pour ISR
    static TriacController* _instance;

    static void IRAM_ATTR zeroCrossISR(void* arg);
    static void IRAM_ATTR triacFire(void* arg);

    void IRAM_ATTR fireTriac();
};
*/