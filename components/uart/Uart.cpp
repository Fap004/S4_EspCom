#include "Uart.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>

static const char *TAG = "UART";

// Queue interne : angles validés prêts à être consommés
#define ANGLE_QUEUE_LEN  16
static QueueHandle_t s_angle_queue = NULL;

// ──────────────────────────────────────────────────────────────────────────────
// Tâche RX Zybo : lit octet par octet, valide le checksum
// ──────────────────────────────────────────────────────────────────────────────
static void vTaskUartZyboRx(void *arg)
{
    uint8_t frame[UART_ZYBO_FRAME_LEN];

    while (true)
    {
        // Lecture bloquante du premier octet (angle)
        int n = uart_read_bytes(UART_ZYBO_PORT,
                                &frame[0], 1,
                                portMAX_DELAY);
        if (n <= 0) continue;

        // Lecture du checksum avec timeout 10 ms
        n = uart_read_bytes(UART_ZYBO_PORT,
                            &frame[1], 1,
                            pdMS_TO_TICKS(10));
        if (n <= 0) {
            ESP_LOGW(TAG, "Zybo: timeout checksum");
            continue;
        }

        // Validation XOR
        uint8_t expected = (uint8_t)frame[0];   // XOR d'un seul byte = lui-même
        if (frame[1] != expected) {
            ESP_LOGW(TAG, "Zybo: bad checksum got=0x%02X exp=0x%02X",
                     frame[1], expected);
            continue;
        }

        int8_t angle = (int8_t)frame[0];

        // Clamp -90..+90
        if (angle < -90) angle = -90;
        if (angle >  90) angle =  90;

        // Envoie dans la queue (non-bloquant : on écrase si plein)
        if (xQueueSend(s_angle_queue, &angle, 0) != pdTRUE)
            ESP_LOGW(TAG, "Zybo: angle queue full, dropping");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
void uart_bridge_init(void)
{
    // ── UART0 : Zybo (RX seulement) ──────────────────────────────────────
    uart_config_t cfg_zybo = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_ZYBO_PORT, &cfg_zybo));
    ESP_ERROR_CHECK(uart_set_pin(UART_ZYBO_PORT,
                                 UART_PIN_NO_CHANGE,  // TX — laissé au terminal
                                 UART_ZYBO_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
   ESP_ERROR_CHECK(uart_driver_install(UART_ZYBO_PORT, 128, 0, 0, NULL, 0));

    // ── UART1 : Basys (TX seulement) ─────────────────────────────────────
    uart_config_t cfg_basys = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_BASYS_PORT, &cfg_basys));
    ESP_ERROR_CHECK(uart_set_pin(UART_BASYS_PORT,
                                 UART_BASYS_TX_PIN,
                                 UART_PIN_NO_CHANGE,  // RX — inutilisé
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_BASYS_PORT, 128, 256, 0, NULL, 0));

    // ── Queue angles ──────────────────────────────────────────────────────
    s_angle_queue = xQueueCreate(ANGLE_QUEUE_LEN, sizeof(int8_t));
    configASSERT(s_angle_queue != NULL);

    // ── Tâche RX Zybo ─────────────────────────────────────────────────────
    xTaskCreate(vTaskUartZyboRx, "uart_zybo_rx", 2048, NULL, 4, NULL);

    ESP_LOGI(TAG, "UART bridge ready — %d baud | Zybo RX=GPIO%d | Basys TX=GPIO%d",
             UART_BAUD, UART_ZYBO_RX_PIN, UART_BASYS_TX_PIN);
}

// ──────────────────────────────────────────────────────────────────────────────
int uart_zybo_read_angle(int8_t *out_angle)
{
    if (!s_angle_queue || !out_angle) return 0;
    return (xQueueReceive(s_angle_queue, out_angle, 0) == pdTRUE) ? 1 : 0;
}

// ──────────────────────────────────────────────────────────────────────────────
void uart_basys_send_rpm(int16_t rpmL, int16_t rpmR)
{
    uint8_t frame[UART_BASYS_FRAME_LEN] = {
        (uint8_t)(rpmL >> 8),
        (uint8_t)(rpmL & 0xFF),
        (uint8_t)(rpmR >> 8),
        (uint8_t)(rpmR & 0xFF),
    };
    uart_write_bytes(UART_BASYS_PORT, (const char *)frame, sizeof(frame));
}