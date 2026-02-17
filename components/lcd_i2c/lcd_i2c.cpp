#include "lcd_i2c.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

// ===================== LCD defines =====================
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

#define CMD_CLEAR      0x01
#define CMD_HOME       0x02
#define CMD_ENTRY      0x06
#define CMD_DISPLAY    0x0C
#define CMD_FUNCTION   0x28   // 4 bits, 2 lignes

// ===================== Constructor =====================
LcdI2C::LcdI2C(i2c_port_t port,
               gpio_num_t sda,
               gpio_num_t scl,
               uint8_t i2c_addr,
               uint8_t cols,
               uint8_t rows)
    : _port(port),
      _sda(sda),
      _scl(scl),
      _addr(i2c_addr),
      _cols(cols),
      _rows(rows)
{
    _mutex = xSemaphoreCreateMutex();
}

// ===================== Init =====================
esp_err_t LcdI2C::init()
{
    i2c_init();
    vTaskDelay(pdMS_TO_TICKS(50));   // temps de power-up LCD

    // Init HD44780 en mode 4 bits (séquence robuste)
    sendCommand(0x33);
    vTaskDelay(pdMS_TO_TICKS(5));

    sendCommand(0x33);
    vTaskDelay(pdMS_TO_TICKS(5));

    sendCommand(0x32);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Configuration LCD
    sendCommand(CMD_FUNCTION);   // 4 bits, 2 lignes
    sendCommand(CMD_DISPLAY);    // display ON, cursor OFF
    sendCommand(CMD_CLEAR);      // clear display
    vTaskDelay(pdMS_TO_TICKS(2));
    sendCommand(CMD_ENTRY);      // auto-increment

    return ESP_OK;
}

// ===================== I2C init =====================
void LcdI2C::i2c_init()
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = _sda;
    conf.scl_io_num = _scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;

        i2c_param_config(_port, &conf);

        esp_err_t err = i2c_driver_install(_port, conf.mode, 0, 0, 0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE("LCD", "I2C install failed: %s", esp_err_to_name(err));
        }
}
// ===================== Low-level send (THREAD SAFE) =====================
void LcdI2C::send(uint8_t value, uint8_t mode)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        uint8_t high = (value & 0xF0) | LCD_BACKLIGHT | mode;
        uint8_t low  = ((value << 4) & 0xF0) | LCD_BACKLIGHT | mode;

        auto write_byte = [&](uint8_t data)
        {
            i2c_master_write_to_device(
                _port,
                _addr,
                &data,
                1,
                pdMS_TO_TICKS(50)
            );
        };

        // High nibble
        write_byte(high | LCD_ENABLE);
        esp_rom_delay_us(1);
        write_byte(high);
        esp_rom_delay_us(50);

        // Low nibble
        write_byte(low | LCD_ENABLE);
        esp_rom_delay_us(1);
        write_byte(low);
        esp_rom_delay_us(50);

        xSemaphoreGive(_mutex);
    }
}

// ===================== Commands / Data =====================
void LcdI2C::sendCommand(uint8_t cmd)
{
    send(cmd, 0);
}

void LcdI2C::sendData(uint8_t data)
{
    send(data, LCD_RS);
}

// ===================== High-level API =====================
void LcdI2C::clear()
{
    sendCommand(CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void LcdI2C::setCursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};

    if (row >= _rows) row = 0;
    sendCommand(static_cast<uint8_t>(0x80 | (col + row_offsets[row])));
}

void LcdI2C::print(const char* text)
{
    while (*text)
    {
        sendData(static_cast<uint8_t>(*text++));
    }
}
