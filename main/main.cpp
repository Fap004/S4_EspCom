#include "com.h"
#include "protocol.h"
#include "Uart.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

// MAC du robot
static const uint8_t robot_mac[6] = { 0x20, 0x6E, 0xF1, 0x0D, 0x4D, 0xB8 };

#define MSG_DATA_LEN  8
#define V_MAX         1.0f    // ← vitesse max robot en m/s (à ajuster)
#define WHEEL_RADIUS  0.035f  // ← rayon roue en m (à ajuster)

// Consigne partagée
static volatile float g_speed_pct  = 0.0f;   // 0–100%
static volatile float g_angle_deg  = 0.0f;   // -30° à +30°
static volatile bool  g_reverse    = false;
static volatile bool  g_running    = false;

// ──────────────────────────────────────────────────────────────────────────────
static void send_cmd(float speed_pct, float angle_deg, bool reverse)
{
    if (speed_pct < 0.0f)   speed_pct = 0.0f;
    if (speed_pct > 100.0f) speed_pct = 100.0f;

    // Clamp angle -30 à +30°
    if (angle_deg < -30.0f) angle_deg = -30.0f;
    if (angle_deg >  30.0f) angle_deg =  30.0f;

    uint8_t speed8 = (uint8_t)(speed_pct / 100.0f * 255.0f + 0.5f);
    uint8_t angle6 = (uint8_t)((angle_deg + 30.0f) / 60.0f * 63.0f + 0.5f); // 0–63
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
static void vTaskRx(void* arg)
{
    uint8_t  data[MSG_DATA_LEN];
    size_t   len;
    uint16_t seq;
    int      msg_count = 0;

    while (true)
    {
        int ret = com_read_msg_wait(data, &len, &seq, pdMS_TO_TICKS(500));

        if (ret == 0) {
            printf("[RX] timeout — aucun message depuis 500ms, total=%d\n", msg_count);
            continue;
        }

        msg_count++;

        // Nouveau format proto_tlm_t
        proto_tlm_t tlm;
        memcpy(&tlm, data, sizeof(proto_tlm_t));

        float speed = tlm.speed_x100 / 100.0f;
        const char* unit_str = (tlm.unit == PROTO_UNIT_KMH) ? "km/h" :
                               (tlm.unit == PROTO_UNIT_MPS)  ? "m/s"  : "RPM";

        printf("vitesse=%.2f %s | seq=%u | count=%d\n",
               speed, unit_str, seq, msg_count);

        int16_t speed_kmh = tlm.speed_x100 / 100;
        uart_basys_send_speed(speed_kmh);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
static void vTaskTx(void* arg)
{
    const TickType_t period = pdMS_TO_TICKS(50);   // 20 Hz
    TickType_t last = xTaskGetTickCount();

    for (;;)
    {
        if (g_running)
            send_cmd(g_speed_pct, g_angle_deg, g_reverse);
        else
            send_cmd(0.0f, 0.0f, false);

        vTaskDelayUntil(&last, period);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
static void vTaskTestSequence(void* arg)
{
    const struct { float pct; bool rev; uint32_t dur_ms; } steps[] = {   
        {  100, false, 30000 },   
        {  100, false, 3000 },   
        {   0, false, 1000 },   // stop        1s

    };
    const int n = sizeof(steps) / sizeof(steps[0]);

    for (int i = 0; i < n; i++)
    {
        printf("[SEQ] étape %d/%d : %.0f%% %s pendant %lu ms\n",
               i + 1, n,
               steps[i].pct,
               steps[i].rev ? "RECULONS" : "AVANT",
               (unsigned long)steps[i].dur_ms);

        g_speed_pct = steps[i].pct;
        g_reverse   = steps[i].rev;
        g_running   = (steps[i].pct > 0.0f);

        vTaskDelay(pdMS_TO_TICKS(steps[i].dur_ms));
    }

    printf("[SEQ] séquence terminée — moteur arrêté\n");
    g_running = false;
    vTaskDelete(NULL);
}

// ──────────────────────────────────────────────────────────────────────────────
static void vTaskTerminal(void* arg)
{
    char buf[64];
    int  pos = 0;

    printf("\n=== Controle robot via terminal ===\n");
    printf("Commandes: speed <0-100> | angle <-30 a 30> | fwd | rev | stop | go | seq | status\n\n");

    while (true)
    {
        int c = getchar();
        if (c == EOF || c < 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (c == '\r') continue;

        if (c == '\n')
        {
            buf[pos] = '\0';
            pos = 0;

            if (strlen(buf) == 0) continue;

            if (strncmp(buf, "speed ", 6) == 0)
            {
                float pct = (float)atof(buf + 6);
                if (pct < 0.0f || pct > 100.0f)
                    printf("[ERR] vitesse hors limites (0 a 100%%)\n");
                else {
                    g_speed_pct = pct;
                    g_running   = true;
                    printf("[CMD] consigne -> %.0f%% %s\n",
                           g_speed_pct, g_reverse ? "ARRIERE" : "AVANT");
                }
            }
            else if (strncmp(buf, "angle ", 6) == 0)
            {
                float ang = (float)atof(buf + 6);
                if (ang < -30.0f || ang > 30.0f)
                    printf("[ERR] angle hors limites (-30 a +30 deg)\n");
                else {
                    g_angle_deg = ang;
                    printf("[CMD] angle -> %.1f deg\n", g_angle_deg);
                }
            }
            else if (strcmp(buf, "fwd") == 0)  { g_reverse = false; printf("[CMD] sens -> AVANT\n"); }
            else if (strcmp(buf, "rev") == 0)  { g_reverse = true;  printf("[CMD] sens -> ARRIERE\n"); }
            else if (strcmp(buf, "stop") == 0) { g_running = false; printf("[CMD] moteur STOP\n"); }
            else if (strcmp(buf, "go") == 0)
            {
                g_running = true;
                printf("[CMD] reprise -> %.0f%% %s\n",
                       g_speed_pct, g_reverse ? "ARRIERE" : "AVANT");
            }
            else if (strcmp(buf, "seq") == 0)
            {
                printf("[CMD] lancement sequence de test\n");
                xTaskCreate(vTaskTestSequence, "seq_task", 4096, NULL, 2, NULL);
            }
            else if (strcmp(buf, "status") == 0)
            {
                printf("[STATUS] speed=%.0f%% | angle=%.1fdeg | sens=%s | actif=%s\n",
                       g_speed_pct, g_angle_deg,
                       g_reverse ? "ARRIERE" : "AVANT",
                       g_running  ? "OUI"    : "NON");
            }
            else
            {
                printf("[ERR] commande inconnue: '%s'\n", buf);
                printf("Commandes: speed <0-100> | angle <-30 a 30> | fwd | rev | stop | go | seq | status\n");
            }
        }
        else if (pos < (int)sizeof(buf) - 1)
        {
            buf[pos++] = (char)c;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
extern "C" void app_main(void)
{
    const uint8_t channel = 1;

    com_init(channel);
    com_add_peer(robot_mac);
    uart_bridge_init();

    uint8_t my_mac[6];
    com_get_mac(my_mac);
    printf("[NODE] My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           my_mac[0], my_mac[1], my_mac[2],
           my_mac[3], my_mac[4], my_mac[5]);

    xTaskCreate(vTaskRx,           "rx_task",  8192, NULL, 5, NULL);
    xTaskCreate(vTaskTx,           "tx_task",  4096, NULL, 3, NULL);
    xTaskCreate(vTaskTestSequence, "seq_task", 4096, NULL, 2, NULL);
    // xTaskCreate(vTaskTerminal, "term_task", 4096, NULL, 2, NULL);
}
 