#pragma once

#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>

class LcdI2C
{
public:
    LcdI2C(i2c_port_t port,
           gpio_num_t sda,
           gpio_num_t scl,
           uint8_t i2c_addr,
           uint8_t cols,
           uint8_t rows);

    esp_err_t init();
    void clear();
    void setCursor(uint8_t col, uint8_t row);
    void print(const char* text);

private:
    i2c_port_t _port;
    gpio_num_t _sda;
    gpio_num_t _scl;
    uint8_t _addr;
    uint8_t _cols;
    uint8_t _rows;

    SemaphoreHandle_t _mutex;

    void i2c_init();
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void send(uint8_t value, uint8_t mode);
};