#pragma once
#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Initialise le PCNT pour mesurer la fréquence PWM (GPIO_IN)
 */
esp_err_t pwm_freq_init_pcnt(void);

/**
 * @brief Initialise le PCNT pour compter les pulses sur GPIO16
 */
esp_err_t pulse_count_init_pcnt(void);

/**
 * @brief Mesure PWM et pulses pendant 1 seconde
 * @param pwm_freq_hz_out : fréquence PWM mesurée en Hz
 * @param gpio16_pulses_out : nombre de pulses sur GPIO16
 */
void measure_pcnt_1s_gate(float *pwm_freq_hz_out, int16_t *gpio16_pulses_out);
