#include "Uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "UART";

static QueueHandle_t s_angle_queue = NULL;

// ──────────────────────────────────────────────────────────────────────────────
// TX loopback : envoie un byte qui incrémente chaque seconde
// ──────────────────────────────────────────────────────────────────────────────
static void vTaskUartLoopbackTx(void *arg)
{
    uint8_t val = 0;
    while (true)
    {
        uart_write_bytes(UART_NUM_1, (const char*)&val, 1);
        val++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// RX : affiche chaque byte reçu
// ──────────────────────────────────────────────────────────────────────────────
static void vTaskUartZyboRx(void *arg)
{
    uint8_t byte;
    while (true)
    {
        int n = uart_read_bytes(UART_NUM_1, &byte, 1, portMAX_DELAY);
        if (n == 1)
        {
            int8_t val = (int8_t)byte;
            if (s_angle_queue)
                xQueueSend(s_angle_queue, &val, 0);
        }
    }
}
// ──────────────────────────────────────────────────────────────────────────────
void uart_bridge_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1,
                                 UART_BASYS_TX_PIN,  // TX → GPIO 19
                                 UART_ZYBO_RX_PIN,   // RX ← GPIO 16
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 512, 256, 0, NULL, 0));

    s_angle_queue = xQueueCreate(ANGLE_QUEUE_LEN, sizeof(int8_t));
    configASSERT(s_angle_queue != NULL);

    xTaskCreate(vTaskUartZyboRx,     "uart_rx", 2048, NULL, 4, NULL);
    xTaskCreate(vTaskUartLoopbackTx, "uart_tx", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "UART1 pret — RX GPIO%d | TX GPIO%d | %d baud",
             UART_ZYBO_RX_PIN, UART_BASYS_TX_PIN, UART_BAUD);
}

// ──────────────────────────────────────────────────────────────────────────────
int uart_zybo_read_angle(int8_t *out_angle)
{
    if (!s_angle_queue || !out_angle) return 0;
    return (xQueueReceive(s_angle_queue, out_angle, 0) == pdTRUE) ? 1 : 0;
}

// ──────────────────────────────────────────────────────────────────────────────
void uart_basys_send_speed(int16_t speed_kmh)
{
    uint8_t frame[2] = {
        (uint8_t)(speed_kmh >> 8),
        (uint8_t)(speed_kmh & 0xFF),
    };
    uart_write_bytes(UART_BASYS_PORT, (const char*)frame, sizeof(frame));
    printf("[UART TX] vitesse -> %d km/h\n", speed_kmh);
}