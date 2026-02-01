/*
#include "triac.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

TriacController* TriacController::_instance = nullptr;

TriacController::TriacController(gpio_num_t triacPin, gpio_num_t zcPin)
    : _triacPin(triacPin), _zcPin(zcPin), _consigne(0), _triacTimer(nullptr)
{
    _instance = this;
}

void TriacController::begin() {
    // GPIO triac
    gpio_reset_pin(_triacPin);
    gpio_set_direction(_triacPin, GPIO_MODE_OUTPUT);
    gpio_set_level(_triacPin, 0);

    // GPIO zero-cross
    gpio_reset_pin(_zcPin);
    gpio_set_direction(_zcPin, GPIO_MODE_INPUT);
    gpio_set_intr_type(_zcPin, GPIO_INTR_POSEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(_zcPin, zeroCrossISR, nullptr);

    // Timer pour déclencher triac
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &triacFire;
    timer_args.arg = nullptr;
    timer_args.name = "triacTimer";
    esp_timer_create(&timer_args, &_triacTimer);
}

void TriacController::setConsigne(uint8_t pct) {
    if (pct > 100) pct = 100;
    _consigne = pct;
}

// ===== ISR statiques =====
void IRAM_ATTR TriacController::zeroCrossISR(void* arg) {
    if (_instance) {
        // Demi-cycle 50Hz = 10000us
        uint64_t delay_us = 10000ULL * (100 - _instance->_consigne) / 100;
        esp_timer_start_once(_instance->_triacTimer, delay_us);
    }
}

void IRAM_ATTR TriacController::triacFire(void* arg) {
    if (_instance) _instance->fireTriac();
}

// ===== Déclenchement triac =====
void IRAM_ATTR TriacController::fireTriac() {
    gpio_set_level(_triacPin, 1);
    ets_delay_us(100); // pulse très court
    gpio_set_level(_triacPin, 0);
}
*/