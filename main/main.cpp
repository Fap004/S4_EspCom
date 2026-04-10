#include "com.h"
#include "protocol.h"
#include "Uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "driver/gpio.h"

static const uint8_t robot_mac[6] = { 0x20, 0x6E, 0xF1, 0x0D, 0x4D, 0xB8 };

#define MSG_DATA_LEN    8
#define AXIS_DEBUG_PIN  GPIO_NUM_4

static float g_speed_pct = 0.0f;
static float g_angle_deg = 0.0f;
static bool  g_reverse   = false;
static bool  g_running   = false;

// ──────────────────────────────────────────────────────────────────────────────
static void send_cmd(float speed_pct, float angle_deg, bool reverse)
{
    if (speed_pct < 0.0f)   speed_pct = 0.0f;
    if (speed_pct > 100.0f) speed_pct = 100.0f;
    if (angle_deg < -30.0f) angle_deg = -30.0f;
    if (angle_deg >  30.0f) angle_deg =  30.0f;

    uint8_t speed8 = (uint8_t)(speed_pct / 100.0f * 255.0f + 0.5f);
    uint8_t angle6 = (uint8_t)((angle_deg + 30.0f) / 60.0f * 63.0f + 0.5f);
    uint8_t dir2   = reverse ? PROTO_DIR_REV : PROTO_DIR_FWD;

    uint16_t w = proto_pack_cmd(speed8, angle6, dir2);

    uint8_t data[MSG_DATA_LEN] = {
        (uint8_t)(w >> 8),
        (uint8_t)(w & 0xFF),
        0, 0, 0, 0, 0, 0
    };

    com_send(robot_mac, data, sizeof(data));
}

// ──────────────────────────────────────────────────────────────────────────────
// RX télémétrie robot → renvoie au Basys
// ──────────────────────────────────────────────────────────────────────────────
static void vTaskRx(void *arg)
{
    uint8_t  data[MSG_DATA_LEN];
    size_t   len;
    uint16_t seq;
    int      msg_count = 0;

    while (true)
    {
        if (!com_read_msg_wait(data, &len, &seq, pdMS_TO_TICKS(500))) {
            continue;
        }

        msg_count++;

        proto_tlm_t tlm;
        memcpy(&tlm, data, sizeof(proto_tlm_t));

        uart_basys_send_speed(tlm.speed_x100 / 100);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// TX 20 Hz : lit les trames Zybo + envoie consigne ESP-NOW
// ──────────────────────────────────────────────────────────────────────────────
static void vTaskTx(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(50);
    TickType_t last = xTaskGetTickCount();

    for (;;)
    {
        zybo_frame_t frame;
        while (uart_zybo_read_frame(&frame))
        {
            // ── X-axis (angle) ──────────────────────────
            gpio_set_level(AXIS_DEBUG_PIN, 1);
            g_angle_deg = (float)frame.angle / 127.0f * 30.0f;
            gpio_set_level(AXIS_DEBUG_PIN, 0);

            // ── Y-axis (speed) — GPIO stays LOW ─────────
            g_reverse   = (frame.speed < 0);
            int8_t abs_speed = frame.speed < 0 ? -frame.speed : frame.speed;
            g_speed_pct = (float)abs_speed / 127.0f * 100.0f;
            g_running   = (frame.speed != 0);
        }

        if (g_running)
            send_cmd(g_speed_pct, g_angle_deg, g_reverse);
        else
            send_cmd(0.0f, 0.0f, false);

        vTaskDelayUntil(&last, period);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
extern "C" void app_main(void)
{
    // GPIO debug pin init
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << AXIS_DEBUG_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(AXIS_DEBUG_PIN, 0);

    com_init(1);
    com_add_peer(robot_mac);
    uart_bridge_init();

    xTaskCreate(vTaskRx, "rx_task", 8192, NULL, 5, NULL);
    xTaskCreate(vTaskTx, "tx_task", 4096, NULL, 3, NULL);
}