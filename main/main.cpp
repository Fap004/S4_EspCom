#include "com.h"
#include "protocol.h"
#include "Uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static const uint8_t robot_mac[6] = { 0x20, 0x6E, 0xF1, 0x0D, 0x4D, 0xB8 };

#define MSG_DATA_LEN        8
#define TX_PERIOD_MS       20
#define SMOOTH_ALPHA       0.2f

static float g_speed_target_pct = 0.0f;
static float g_angle_target_deg = 0.0f;
static bool  g_reverse          = false;
static bool  g_running          = false;

static float g_speed_filt_pct   = 0.0f;
static float g_angle_filt_deg   = 0.0f;

/* ========================================================================== */
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

/* ========================================================================== */
static void vTaskRx(void *arg)
{
    uint8_t  data[MSG_DATA_LEN];
    size_t   len;
    uint16_t seq;

    while (true)
    {
        if (!com_read_msg_wait(data, &len, &seq, pdMS_TO_TICKS(500)))
            continue;

        proto_tlm_t tlm;
        memcpy(&tlm, data, sizeof(proto_tlm_t));

        uart_basys_send_speed(tlm.speed_x100 / 100);
    }
}

/* ========================================================================== */
static void vTaskTx(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(TX_PERIOD_MS);
    TickType_t last = xTaskGetTickCount();

    zybo_frame_t frame;

    for (;;)
    {
        bool new_frame = false;
        while (uart_zybo_read_frame(&frame))
            new_frame = true;

        if (new_frame)
        {
            int8_t safe_angle = frame.angle;
            if (safe_angle == -128) safe_angle = -127;
            g_angle_target_deg = ((float)safe_angle / 127.0f) * 30.0f;

            g_reverse = (frame.speed < 0);
            int8_t abs_speed = frame.speed < 0 ? -frame.speed : frame.speed;
            g_speed_target_pct = ((float)abs_speed / 127.0f) * 100.0f;

            g_running = (frame.speed != 0);
        }

        g_angle_filt_deg += SMOOTH_ALPHA * (g_angle_target_deg - g_angle_filt_deg);
        g_speed_filt_pct += SMOOTH_ALPHA * (g_speed_target_pct - g_speed_filt_pct);

        if (g_running)
            send_cmd(g_speed_filt_pct, g_angle_filt_deg, g_reverse);
        else
            send_cmd(0.0f, g_angle_filt_deg, false);

        vTaskDelayUntil(&last, period);
    }
}

/* ========================================================================== */
extern "C" void app_main(void)
{
    com_init(1);
    com_add_peer(robot_mac);

    uart_bridge_init();

    xTaskCreate(vTaskRx, "rx_task", 8192, NULL, 5, NULL);
    xTaskCreate(vTaskTx, "tx_task", 4096, NULL, 6, NULL);
}