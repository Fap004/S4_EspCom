#include "Uart.h"

#include "esp_task_wdt.h"
#include "freertos/task.h"

static QueueHandle_t s_frame_queue = nullptr;

/* =========================================================
 * RX task : Zybo -> ESP (UART1 @ 460800)
 * ========================================================= */
static void vTaskUartZyboRx(void *arg)
{
    esp_task_wdt_add(nullptr);

    uint8_t buf[128];

    typedef enum { WAIT_START, READ_ANGLE, READ_SPEED } parse_state_t;
    parse_state_t state = WAIT_START;
    int8_t tmp_angle = 0;

    while (true)
    {
        esp_task_wdt_reset();

        int len = uart_read_bytes(
            UART_PORT,
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
            uint8_t b = buf[i];

            /* Start byte */
            if (b == 0x80)
            {
                state = READ_ANGLE;
                continue;
            }

            switch (state)
            {
                case WAIT_START:
                    break;

                case READ_ANGLE:
                    tmp_angle = (int8_t)b;
                    state = READ_SPEED;
                    break;

                case READ_SPEED:
                {
                    zybo_frame_t frame;

                    if (gpio_get_level(UART_SWAP_XY_PIN)) {
                        frame.angle = (int8_t)b;
                        frame.speed = tmp_angle;
                    } else {
                        frame.angle = tmp_angle;
                        frame.speed = (int8_t)b;
                    }

                    if (s_frame_queue)
                        xQueueSend(s_frame_queue, &frame, 0);

                    state = WAIT_START;
                    break;
                }

                default:
                    state = WAIT_START;
                    break;
            }
        }
    }
}

/* =========================================================
 * INIT UART UNIQUE (UART1 @ 460800)
 * ========================================================= */
void uart_bridge_init(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_PORT,
        UART_TX_PIN,
        UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));
    ESP_ERROR_CHECK(uart_driver_install(
        UART_PORT,
        2048,   // RX buffer
        0,      // TX buffer
        0,
        nullptr,
        0
    ));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << UART_SWAP_XY_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    s_frame_queue = xQueueCreate(ANGLE_QUEUE_LEN, sizeof(zybo_frame_t));
    configASSERT(s_frame_queue);

    xTaskCreate(vTaskUartZyboRx, "uart_zybo_rx", 4096, nullptr, 6, nullptr);
}

/* =========================================================
 * API
 * ========================================================= */
int uart_zybo_read_frame(zybo_frame_t *out)
{
    if (!out || !s_frame_queue) return 0;
    return xQueueReceive(s_frame_queue, out, 0) == pdTRUE;
}

/* =========================================================
 * ESP -> BASYS (UART1 TX @ 460800)
 * ========================================================= */
void uart_basys_send_speed(int16_t kmh)
{
    uint8_t frame[2] = {
        (uint8_t)(kmh >> 8),
        (uint8_t)(kmh & 0xFF)
    };
    uart_write_bytes(UART_PORT, (const char *)frame, 2);
}