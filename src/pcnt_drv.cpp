#include "pcnt_drv.h"
#include "driver/pcnt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <string.h>


#define PWM_IN_GPIO    2
#define PULSE_IN_GPIO  16

// ===================== PWM PCNT =====================
esp_err_t pwm_freq_init_pcnt(void)
{
    pcnt_config_t pcnt_config;
    memset(&pcnt_config, 0, sizeof(pcnt_config));

    pcnt_config.pulse_gpio_num = PWM_IN_GPIO;
    pcnt_config.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    pcnt_config.channel        = PCNT_CHANNEL_0;
    pcnt_config.unit           = PCNT_UNIT_0;
    pcnt_config.pos_mode       = PCNT_COUNT_INC;
    pcnt_config.neg_mode       = PCNT_COUNT_DIS;
    pcnt_config.lctrl_mode     = PCNT_MODE_KEEP;
    pcnt_config.hctrl_mode     = PCNT_MODE_KEEP;
    pcnt_config.counter_h_lim  = 32767;
    pcnt_config.counter_l_lim  = 0;

    ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_config));
    ESP_ERROR_CHECK(pcnt_set_filter_value(PCNT_UNIT_0, 100));
    ESP_ERROR_CHECK(pcnt_filter_enable(PCNT_UNIT_0));

    ESP_ERROR_CHECK(pcnt_counter_pause(PCNT_UNIT_0));
    ESP_ERROR_CHECK(pcnt_counter_clear(PCNT_UNIT_0));
    ESP_ERROR_CHECK(pcnt_counter_resume(PCNT_UNIT_0));

    return ESP_OK;
}

// ===================== Pulse GPIO16 PCNT =====================
esp_err_t pulse_count_init_pcnt(void)
{
    pcnt_config_t pcnt_config;
    memset(&pcnt_config, 0, sizeof(pcnt_config));

    pcnt_config.pulse_gpio_num = PULSE_IN_GPIO;
    pcnt_config.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    pcnt_config.channel        = PCNT_CHANNEL_0;
    pcnt_config.unit           = PCNT_UNIT_1;
    pcnt_config.pos_mode       = PCNT_COUNT_INC;
    pcnt_config.neg_mode       = PCNT_COUNT_DIS;
    pcnt_config.lctrl_mode     = PCNT_MODE_KEEP;
    pcnt_config.hctrl_mode     = PCNT_MODE_KEEP;
    pcnt_config.counter_h_lim  = 32767;
    pcnt_config.counter_l_lim  = 0;

    ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_config));
    ESP_ERROR_CHECK(pcnt_set_filter_value(PCNT_UNIT_1, 100));
    ESP_ERROR_CHECK(pcnt_filter_enable(PCNT_UNIT_1));

    ESP_ERROR_CHECK(pcnt_counter_pause(PCNT_UNIT_1));
    ESP_ERROR_CHECK(pcnt_counter_clear(PCNT_UNIT_1));
    ESP_ERROR_CHECK(pcnt_counter_resume(PCNT_UNIT_1));

    return ESP_OK;
}

// ===================== Mesure pendant 1 seconde =====================
void measure_pcnt_1s_gate(float *pwm_freq_hz_out, int16_t *gpio16_pulses_out)
{
    int16_t pwm_count = 0;
    int16_t pulse16_count = 0;

    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_1);

    vTaskDelay(pdMS_TO_TICKS(1000));

    pcnt_get_counter_value(PCNT_UNIT_0, &pwm_count);
    pcnt_get_counter_value(PCNT_UNIT_1, &pulse16_count);

    *pwm_freq_hz_out = static_cast<float>(pwm_count);
    *gpio16_pulses_out = pulse16_count;
}
