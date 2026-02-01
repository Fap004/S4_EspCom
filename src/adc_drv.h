#pragma once
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdbool.h>

/**
 * @brief Initialise les ADC pour les canaux définis
 */
void adc_init(void);

/**
 * @brief Lit la tension d'un canal ADC
 * @param ch ADC1 channel (ADC1_CHANNEL_0..ADC1_CHANNEL_7)
 * @return Tension en Volts (float)
 */
float adc_read_voltage(adc1_channel_t ch);

/**
 * @brief Vérifie si la calibration est disponible
 * @return true si calibration valide, false sinon
 */
bool adc_is_calibration_ok(void);
