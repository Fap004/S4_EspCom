#include "com.h"
#include "protocol.h"
#include "Uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static const uint8_t robot_mac[6] = { 0x20, 0x6E, 0xF1, 0x0D, 0x4D, 0xB8 };

#define MSG_DATA_LEN 8

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
            //printf("[RX] timeout — total=%d\n", msg_count);
            continue;
        }

        msg_count++;

        proto_tlm_t tlm;
        memcpy(&tlm, data, sizeof(proto_tlm_t));

        float speed = tlm.speed_x100 / 100.0f;
        const char *unit_str = (tlm.unit == PROTO_UNIT_KMH) ? "km/h" :
                               (tlm.unit == PROTO_UNIT_MPS)  ? "m/s"  : "RPM";

        //printf("[RX] vitesse=%.2f %s | seq=%u | count=%d\n",
        //       speed, unit_str, seq, msg_count);

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
        // Vider la queue, garder uniquement la trame la plus récente
        zybo_frame_t frame;
        while (uart_zybo_read_frame(&frame))
        {
            g_angle_deg = (float)frame.angle / 127.0f * 30.0f;
            g_reverse   = (frame.speed < 0);
            int8_t abs_speed = frame.speed < 0 ? -frame.speed : frame.speed;
            g_speed_pct = (float)abs_speed / 127.0f * 100.0f;
            g_running   = (frame.speed != 0);
        }

        if (g_running)
            send_cmd(g_speed_pct, g_angle_deg, g_reverse);
////////////////////CI je veux afficher des valeurs je peux les voir ici je crois/////////////////////////////////////////
        else
            send_cmd(0.0f, 0.0f, false);

        vTaskDelayUntil(&last, period);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
extern "C" void app_main(void)
{
    com_init(1);
    com_add_peer(robot_mac);
    uart_bridge_init();

    uint8_t my_mac[6];
    com_get_mac(my_mac);
    //printf("[NODE] My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    //       my_mac[0], my_mac[1], my_mac[2],
    //       my_mac[3], my_mac[4], my_mac[5]);

    xTaskCreate(vTaskRx, "rx_task", 8192, NULL, 5, NULL);
    xTaskCreate(vTaskTx, "tx_task", 4096, NULL, 3, NULL);
}