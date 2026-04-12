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

    typedef enum { WAIT_START, READ_ANGLE, READ_SPEED } parse_state_t;
    parse_state_t state     = WAIT_START;
    int8_t        tmp_angle = 0;

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
            uint8_t b = buf[i];

            // 0x80 = marqueur de début → resync automatique
            if (b == 0x80)
            {
                state = READ_ANGLE;
                continue;
            }

            switch (state)
            {
                case WAIT_START:
                    // on attend 0x80, on ignore tout le reste
                    break;

                case READ_ANGLE:
                    tmp_angle = (int8_t)b;
                    state = READ_SPEED;
                    break;

                case READ_SPEED:
                {
                    zybo_frame_t frame;

                    if (gpio_get_level(UART_SWAP_XY_PIN) == 1)
                    {
                        frame.angle = (int8_t)b;   // speed → angle
                        frame.speed = tmp_angle;    // angle → speed
                    }
                    else
                    {
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