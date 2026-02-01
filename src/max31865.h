#pragma once
#include "esp_err.h"

/**
 * @brief Initialise le bus SPI et le MAX31865
 */
esp_err_t max31865_init_spi(void);

/**
 * @brief Lit la température du RTD en °C
 * @param temp_c_out : pointeur vers float où stocker la température
 * @return ESP_OK si ok, sinon code erreur
 */
esp_err_t max31865_read_temperature_c(float *temp_c_out);

/**
 * @brief Dump registers pour debug
 */
void max31865_dump_regs(void);

/**
 * @brief Configure seuils Low/High pour PT100
 */
void max31865_set_thresholds_pt100(void);
