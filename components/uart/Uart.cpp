#include "Uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "UART";

static QueueHandle_t s_frame_queue = NULL;

// ──────────────────────────────────────────────────────────────────────────────
// RX : lecture par blocs, regroupe les bytes en paires [angle, speed]
// Sans header de synchro → on lit par multiples de 2, on aligne sur le premier
// ──────────────────────────────────────────────────────────────────────────────
static void vTaskUartZyboRx(void *arg)
{
    esp_task_wdt_add(NULL);

    uint8_t  buf[128];
    uint8_t  pending     = 0;       // byte en attente d'être apparié
    bool     has_pending = false;   // est-ce qu'on a un byte orphelin?
    uint32_t count       = 0;

    while (true)
    {
        esp_task_wdt_reset();

        int len = uart_read_bytes(UART_ZYBO_PORT, buf, sizeof(buf),
                                  pdMS_TO_TICKS(10));

        if (len <= 0) {
            vTaskDelay(1);
            continue;
        }

        for (int i = 0; i < len; i++)
        {
            if (!has_pending)
            {
                // Premier byte de la paire = angle
                pending     = buf[i];
                has_pending = true;
            }
            else
            {
                // Deuxième byte = vitesse → trame complète
                zybo_frame_t frame;
                //frame.angle = (int8_t)pending;
                //frame.speed = (int8_t)buf[i];
                int8_t raw_angle = (int8_t)pending;
                int8_t raw_speed = (int8_t)buf[i];

                // 🚨 correction valeur interdite
                if (raw_angle == -128) raw_angle = -127;
                if (raw_speed == -128) raw_speed = -127;

                frame.angle = raw_angle;
                frame.speed = raw_speed;
                has_pending = false;
                count++;

                //ESP_LOGI(TAG, "[%6" PRIu32 "]  angle=%4d  |  speed=%3u",
                //         count, frame.angle, frame.speed);

                if (s_frame_queue)
                    xQueueSend(s_frame_queue, &frame, 0);
            }
        }

        taskYIELD();
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

    ESP_ERROR_CHECK(uart_param_config(UART_ZYBO_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_ZYBO_PORT,
                                 UART_BASYS_TX_PIN,   // TX → GPIO 19
                                 UART_ZYBO_RX_PIN,    // RX ← GPIO 18
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_ZYBO_PORT, 1024 * 2, 256, 0, NULL, 0));

    s_frame_queue = xQueueCreate(ANGLE_QUEUE_LEN, sizeof(zybo_frame_t));
    configASSERT(s_frame_queue != NULL);

    xTaskCreate(vTaskUartZyboRx, "uart_rx", 4096, NULL, 5, NULL);

    //ESP_LOGI(TAG, "UART1 pret — RX GPIO%d | TX GPIO%d | %d baud",
    //         UART_ZYBO_RX_PIN, UART_BASYS_TX_PIN, UART_BAUD);
}

// ──────────────────────────────────────────────────────────────────────────────
int uart_zybo_read_frame(zybo_frame_t *out)
{
    if (!s_frame_queue || !out) return 0;
    return (xQueueReceive(s_frame_queue, out, 0) == pdTRUE) ? 1 : 0;
}

// ──────────────────────────────────────────────────────────────────────────────
void uart_basys_send_speed(int16_t speed_kmh)
{
    uint8_t frame[2] = {
        (uint8_t)(speed_kmh >> 8),
        (uint8_t)(speed_kmh & 0xFF),
    };
    uart_write_bytes(UART_BASYS_PORT, (const char*)frame, sizeof(frame));
    //printf("[UART TX] vitesse -> %d km/h\n", speed_kmh);
}