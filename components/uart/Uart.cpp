#include "Uart.h"

#include "esp_task_wdt.h"
#include "freertos/task.h"

static QueueHandle_t s_frame_queue = nullptr;

/* ========================================================================== */
/* RX task : UART Zybo                                                         */
/* ========================================================================== */
static void vTaskUartZyboRx(void *arg)
{
    esp_task_wdt_add(nullptr);

    uint8_t buf[128];
    uint8_t pending     = 0;
    bool    has_pending = false;

    while (true)
    {
        esp_task_wdt_reset();

        int len = uart_read_bytes(
            UART_ZYBO_PORT,
            buf,
            sizeof(buf),
            pdMS_TO_TICKS(10)
        );

        if (len <= 0) {
            vTaskDelay(1);
            continue;
        }

        for (int i = 0; i < len; i++)
        {
            if (!has_pending)
            {
                pending     = buf[i];
                has_pending = true;
            }
            else
            {
                int8_t raw_angle = (int8_t)pending;
                int8_t raw_speed = (int8_t)buf[i];

                if (raw_angle == -128) raw_angle = -127;
                if (raw_speed == -128) raw_speed = -127;

                zybo_frame_t frame;

                if (gpio_get_level(UART_SWAP_XY_PIN) == 0)
                {
                    frame.angle = raw_speed;
                    frame.speed = raw_angle;
                }
                else
                {
                    frame.angle = raw_angle;
                    frame.speed = raw_speed;
                }

                has_pending = false;

                if (s_frame_queue)
                    xQueueSend(s_frame_queue, &frame, 0);
            }
        }

        taskYIELD();
    }
}

/* ========================================================================== */
/* INIT                                                                        */
/* ========================================================================== */
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
    ESP_ERROR_CHECK(uart_set_pin(
        UART_ZYBO_PORT,
        UART_BASYS_TX_PIN,
        UART_ZYBO_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));
    ESP_ERROR_CHECK(uart_driver_install(
        UART_ZYBO_PORT,
        2048, 256, 0, nullptr, 0
    ));

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask  = (1ULL << UART_SWAP_XY_PIN);
    io_conf.mode          = GPIO_MODE_INPUT;
    io_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    s_frame_queue = xQueueCreate(ANGLE_QUEUE_LEN, sizeof(zybo_frame_t));
    configASSERT(s_frame_queue != nullptr);

    xTaskCreate(vTaskUartZyboRx, "uart_zybo_rx", 4096, nullptr, 6, nullptr);
}

/* ========================================================================== */
/* API                                                                         */
/* ========================================================================== */
int uart_zybo_read_frame(zybo_frame_t *out)
{
    if (!out || !s_frame_queue) return 0;
    return xQueueReceive(s_frame_queue, out, 0) == pdTRUE;
}

void uart_basys_send_speed(int16_t rpm)
{
    uint8_t frame[UART_BASYS_FRAME_LEN] = {
        (uint8_t)(rpm >> 8),
        (uint8_t)(rpm & 0xFF),
    };
    uart_write_bytes(UART_BASYS_PORT, (const char *)frame, sizeof(frame));
}