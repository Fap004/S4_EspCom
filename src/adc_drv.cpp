#include "adc_drv.h"
#include "esp_log.h"

#define ADC_ATTEN             ADC_ATTEN_DB_12  // ~0-3.3V
#define ADC_WIDTH             ADC_WIDTH_BIT_12  // 12 bits par défaut

static esp_adc_cal_characteristics_t adc_chars;
static bool adc_cal_ok = false;

void adc_init(void)
{
    // Configuration canaux
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN);  // GPIO34
    adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN);  // GPIO33

    // Calibration
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        1100,          // Vref par défaut si eFuse non dispo
        &adc_chars
    );

    if (val_type == ESP_ADC_CAL_VAL_DEFAULT_VREF) {
        ESP_LOGW("ADC_DRV", "Calibration par défaut utilisée");
        adc_cal_ok = false;
    } else {
        ESP_LOGI("ADC_DRV", "Calibration ADC OK");
        adc_cal_ok = true;
    }
}

float adc_read_voltage(adc1_channel_t ch)
{
    int raw = adc1_get_raw(ch);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    return (float)mv / 1000.0f;
}

bool adc_is_calibration_ok(void)
{
    return adc_cal_ok;
}
