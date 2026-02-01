#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "adc_drv.h"
#include "pcnt_drv.h"
#include "max31865.h"
//#include "triac.h"
#include "lcd_i2c.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include <cstring>

// ===================== LCD =====================
LcdI2C lcd(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22, 0x27, 16, 2);

// ===================== UART =====================
#define UART_PORT       UART_NUM_0
#define UART_BUF_SIZE   256

// ===================== I2C Scan =====================
void i2c_scan()
{
    ESP_LOGI("I2C", "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        esp_err_t ret = i2c_master_write_to_device(
            I2C_NUM_0,
            addr,
            NULL,
            0,
            pdMS_TO_TICKS(50)
        );

        if (ret == ESP_OK)
        {
            ESP_LOGI("I2C", "Found device at 0x%02X", addr);
        }
    }
    ESP_LOGI("I2C", "Scan done");
}

// ===================== Main Task =====================
void main_task(void* arg)
{
    // --- Initialisation capteurs ---
    adc_init();
    pwm_freq_init_pcnt();
    pulse_count_init_pcnt();
    max31865_init_spi();
    max31865_set_thresholds_pt100();

    // --- Scan I2C ---
    i2c_scan();

    // --- Initialisation LCD ---
    lcd.init();
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd.clear();

#ifdef LCD_HAS_BACKLIGHT
    lcd.setBacklight(true);
#endif

    lcd.setCursor(0,0);
    lcd.print("ESP32 READY");
    lcd.setCursor(0,1);
    lcd.print("UART -> LCD");

    // --- Initialisation UART ---
    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(UART_PORT, &uart_config);
    uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

    uint8_t data[UART_BUF_SIZE];

    while (1)
    {
        int len = uart_read_bytes(UART_PORT, data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0)
        {
            data[len] = '\0'; // Terminer la chaîne

            lcd.clear();
            lcd.setCursor(0, 0);

            // Afficher les 16 premiers caractères sur la première ligne
            char line1[17] = {0};
            strncpy(line1, (char*)data, 16);
            lcd.print(line1);

            // Afficher les caractères 17 à 32 sur la deuxième ligne si nécessaire
            if (len > 16)
            {
                lcd.setCursor(0, 1);
                char line2[17] = {0};
                strncpy(line2, (char*)data + 16, 16);
                lcd.print(line2);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ===================== App Main =====================
extern "C" void app_main()
{
    xTaskCreate(main_task, "main_task", 8192, NULL, 5, NULL);
}
