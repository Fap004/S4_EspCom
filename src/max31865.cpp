// max31865.cpp
#include "max31865.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

#define MAX31865_CS_GPIO      26
#define MAX31865_MISO_GPIO    19
#define MAX31865_MOSI_GPIO    23
#define MAX31865_SCLK_GPIO    18

#define MAX31865_RREF_OHMS    430.0f
#define MAX31865_R0_OHMS      100.0f
#define MAX31865_3WIRE        1

// Registers
#define MAX31865_REG_CONFIG   0x00
#define MAX31865_REG_RTD_MSB  0x01
#define MAX31865_REG_RTD_LSB  0x02

// Config bits
#define MAX31865_CFG_BIAS     (1 << 7)
#define MAX31865_CFG_MODEAUTO (1 << 6)
#define MAX31865_CFG_1SHOT    (1 << 5)
#define MAX31865_CFG_3WIRE    (1 << 4)
#define MAX31865_CFG_FAULTCLR (1 << 1)
#define MAX31865_CFG_FILT50HZ (1 << 0) // 0=60Hz, 1=50Hz

static spi_device_handle_t max31865_dev = NULL;

// ===================== SPI write =====================
static esp_err_t max31865_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)(reg | 0x80), val };
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * sizeof(tx);
    t.tx_buffer = tx;
    return spi_device_transmit(max31865_dev, &t);
}

// ===================== SPI read =====================
static esp_err_t max31865_read_regs(uint8_t start_reg, uint8_t *out, size_t len)
{
    uint8_t tx[1 + len] = {0};
    uint8_t rx[1 + len] = {0};

    tx[0] = (uint8_t)(start_reg & 0x7F);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * (1 + len);
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    ESP_ERROR_CHECK(spi_device_transmit(max31865_dev, &t));
    memcpy(out, &rx[1], len);
    return ESP_OK;
}

// ===================== Calcul température =====================
static float rtd_temp_c_from_resistance(float Rt)
{
    const float R0 = MAX31865_R0_OHMS;
    const float A = 3.9083e-3f;
    const float B = -5.775e-7f;

    float c = 1.0f - (Rt / R0);
    float disc = A * A - 4.0f * B * c;
    if (disc < 0) return NAN;

    float T = (-A + sqrtf(disc)) / (2.0f * B);
    if (T < 0.0f) T = (Rt - R0) / (R0 * A);

    return T;
}

// ===================== Init SPI =====================
esp_err_t max31865_init_spi(void)
{
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.mosi_io_num = MAX31865_MOSI_GPIO;
    buscfg.miso_io_num = MAX31865_MISO_GPIO;
    buscfg.sclk_io_num = MAX31865_SCLK_GPIO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz = 1000000; // 1 MHz
    devcfg.mode = 1;                  // SPI mode 1
    devcfg.spics_io_num = MAX31865_CS_GPIO;
    devcfg.queue_size = 1;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &max31865_dev));

    uint8_t cfg = MAX31865_CFG_FAULTCLR;
    if (MAX31865_3WIRE) cfg |= MAX31865_CFG_3WIRE;
    ESP_ERROR_CHECK(max31865_write_reg(MAX31865_REG_CONFIG, cfg));

    return ESP_OK;
}

// ===================== Lire température =====================
esp_err_t max31865_read_temperature_c(float *temp_c_out)
{
    uint8_t cfg = MAX31865_CFG_BIAS | MAX31865_CFG_FAULTCLR;
    if (MAX31865_3WIRE) cfg |= MAX31865_CFG_3WIRE;
    ESP_ERROR_CHECK(max31865_write_reg(MAX31865_REG_CONFIG, cfg));

    vTaskDelay(pdMS_TO_TICKS(10));
    cfg |= MAX31865_CFG_1SHOT;
    ESP_ERROR_CHECK(max31865_write_reg(MAX31865_REG_CONFIG, cfg));
    vTaskDelay(pdMS_TO_TICKS(70));

    uint8_t buf[2] = {0};
    ESP_ERROR_CHECK(max31865_read_regs(MAX31865_REG_RTD_MSB, buf, 2));
    uint16_t rtd = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t rtd_code = (rtd >> 1) & 0x7FFF;

    float Rt = ((float)rtd_code * MAX31865_RREF_OHMS) / 32768.0f;
    *temp_c_out = rtd_temp_c_from_resistance(Rt);

    return ESP_OK;
}

// ===================== Seuils PT100 =====================
void max31865_set_thresholds_pt100(void)
{
    // Low threshold: ~ -50°C
    max31865_write_reg(0x05, 0x05); // LFT MSB
    max31865_write_reg(0x06, 0x1E); // LFT LSB

    // High threshold: ~ 250°C
    max31865_write_reg(0x03, 0x0C); // HFT MSB
    max31865_write_reg(0x04, 0x5C); // HFT LSB
}