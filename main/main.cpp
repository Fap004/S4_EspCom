#include "com.h"

#include "protocol.h"

#include "freertos/FreeRTOS.h"

#include "freertos/task.h"

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <inttypes.h>

#include "Uart.h"

#include "driver/uart.h"
 
// MAC du robot

static const uint8_t robot_mac[6] = { 0x20, 0x6E, 0xF1, 0x0D, 0x4D, 0xB8 };
 
#define MSG_DATA_LEN  8

#define RPM_MAX       8000.0f
 
// Consigne partagée entre la tâche terminal et la tâche TX

static volatile float g_rpm_desired = 0.0f;

static volatile bool  g_reverse     = false;

static volatile bool  g_running     = false;
 
// ──────────────────────────────────────────────────────────────────────────────

// Helper : encode et envoie une commande RPM au robot

static void send_cmd(float rpm, bool reverse)

{

    if (rpm < 0.0f)    rpm = 0.0f;

    if (rpm > RPM_MAX) rpm = RPM_MAX;
 
    uint16_t speed13 = (uint16_t)((rpm / RPM_MAX) * 8191.0f + 0.5f);

    uint8_t  dir2    = reverse ? PROTO_DIR_REV : PROTO_DIR_FWD;
 
    uint16_t w = (uint16_t)(((speed13 & 0x1FFF) << 3) |

                             ((dir2    & 0x03)   << 1) |

                             PROTO_TYPE_CMD);
 
    uint8_t data[MSG_DATA_LEN] = {

        (uint8_t)(w >> 8),

        (uint8_t)(w & 0xFF),

        0, 0, 0, 0, 0, 0

    };
 
    com_send(robot_mac, data, sizeof(data));

}
 
// ──────────────────────────────────────────────────────────────────────────────

// RX : reçoit la télémétrie (timestamp + rpmL + rpmR) du robot

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
 
        if (len != MSG_DATA_LEN) {

            printf("[RX] mauvaise longueur: %d\n", (int)len);

            continue;

        }
 
        uint32_t timestamp =

            ((uint32_t)data[0] << 24) |

            ((uint32_t)data[1] << 16) |

            ((uint32_t)data[2] <<  8) |

             data[3];
 
        int16_t rpmL = ((int16_t)data[4] << 8) | data[5];

        int16_t rpmR = ((int16_t)data[6] << 8) | data[7];
 
        printf("t=%" PRIu32 " ms | rpmL=%d | rpmR=%d | seq=%u | count=%d\n",

               timestamp, rpmL, rpmR, seq, msg_count);

        uart_basys_send_rpm(rpmL, rpmR);

    }

}
 
// ──────────────────────────────────────────────────────────────────────────────

// TX : envoie la consigne au robot à 20 Hz

static void vTaskTx(void* arg)

{

    const TickType_t period = pdMS_TO_TICKS(50);   // 20 Hz

    TickType_t last = xTaskGetTickCount();
 
    for (;;)

    {

        if (g_running)

            send_cmd(g_rpm_desired, g_reverse);

        else

            send_cmd(0.0f, false);
 
        vTaskDelayUntil(&last, period);

    }

}
 
// ──────────────────────────────────────────────────────────────────────────────

// Séquence de test automatique

static void vTaskTestSequence(void* arg)

{

    const struct { float rpm; bool rev; uint32_t dur_ms; } steps[] = {

        {  2000, false, 3000 },   //  500 RPM avant  3s

        { 4000, false, 3000 },   // 1000 RPM avant  3s

        { 3000, false, 3000 },   // 2000 RPM avant  3s

        {    0, false, 1000 },   // stop             1s

        { 2000, true,  3000 },   // 1000 RPM arrière 3s

        {    0, false, 1000 },   // stop             1s

    };

    const int n = sizeof(steps) / sizeof(steps[0]);
 
    for (int i = 0; i < n; i++)

    {

        printf("[SEQ] étape %d/%d : %.0f RPM %s pendant %lu ms\n",

               i + 1, n, steps[i].rpm,

               steps[i].rev ? "ARRIERE" : "AVANT",

               (unsigned long)steps[i].dur_ms);
 
        g_rpm_desired = steps[i].rpm;

        g_reverse     = steps[i].rev;

        g_running     = (steps[i].rpm > 0.0f);
 
        vTaskDelay(pdMS_TO_TICKS(steps[i].dur_ms));

    }
 
    printf("[SEQ] séquence terminée — moteur arrêté\n");

    g_running = false;

    vTaskDelete(NULL);

}
 
// ──────────────────────────────────────────────────────────────────────────────

// Terminal : lit les commandes depuis le moniteur série

//

// Commandes :

//   rpm <valeur>   ex: rpm 1500   → fixe la consigne RPM

//   fwd            → sens avant

//   rev            → sens arrière

//   stop           → arrête le moteur

//   go             → reprend avec la dernière consigne

//   seq            → lance la séquence de test automatique

//   status         → affiche l'état actuel

static void vTaskTerminal(void* arg)

{

    char buf[64];

    int  pos = 0;
 
    printf("\n=== Controle robot via terminal ===\n");

    printf("Commandes: rpm <val> | fwd | rev | stop | go | seq | status\n\n");
 
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
 
            if (strncmp(buf, "rpm ", 4) == 0)

            {

                float rpm = (float)atof(buf + 4);

                if (rpm < 0.0f || rpm > RPM_MAX)

                    printf("[ERR] RPM hors limites (0 a %.0f)\n", RPM_MAX);

                else {

                    g_rpm_desired = rpm;

                    g_running     = true;

                    printf("[CMD] consigne -> %.0f RPM %s\n",

                           g_rpm_desired, g_reverse ? "ARRIERE" : "AVANT");

                }

            }

            else if (strcmp(buf, "fwd") == 0)

            {

                g_reverse = false;

                printf("[CMD] sens -> AVANT\n");

            }

            else if (strcmp(buf, "rev") == 0)

            {

                g_reverse = true;

                printf("[CMD] sens -> ARRIERE\n");

            }

            else if (strcmp(buf, "stop") == 0)

            {

                g_running = false;

                printf("[CMD] moteur STOP\n");

            }

            else if (strcmp(buf, "go") == 0)

            {

                g_running = true;

                printf("[CMD] reprise -> %.0f RPM %s\n",

                       g_rpm_desired, g_reverse ? "ARRIERE" : "AVANT");

            }

            else if (strcmp(buf, "seq") == 0)

            {

                printf("[CMD] lancement sequence de test\n");

                xTaskCreate(vTaskTestSequence, "seq_task", 4096, NULL, 2, NULL);

            }

            else if (strcmp(buf, "status") == 0)

            {

                printf("[STATUS] rpm=%.0f | sens=%s | actif=%s\n",

                       g_rpm_desired,

                       g_reverse ? "ARRIERE" : "AVANT",

                       g_running  ? "OUI"    : "NON");

            }

            else

            {

                printf("[ERR] commande inconnue: '%s'\n", buf);

                printf("Commandes: rpm <val> | fwd | rev | stop | go | seq | status\n");

            }

        }

        else if (pos < (int)sizeof(buf) - 1)

        {

            buf[pos++] = (char)c;

        }

    }

}

//TEST UART SIMON 

static void vTaskUartTest(void* arg)
{
    uint8_t val = 0;
    while (true)
    {
        uart_write_bytes(UART_NUM_1, (const char*)&val, 1);
        printf("[TX] envoi byte=%d\n", val);
        val++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
 
static void vTaskAngleMonitor(void* arg)
{
    while (true)
    {
        int8_t byte;
        if (uart_zybo_read_angle(&byte))
            printf("[RX] recu byte=%d\n", byte);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ──────────────────────────────────────────────────────────────────────────────

extern "C" void app_main(void)

{

    const uint8_t channel = 1;
 
    com_init(channel);

    com_add_peer(robot_mac);

    uart_bridge_init(); //UART 
 
    uint8_t my_mac[6];

    com_get_mac(my_mac);

    printf("[NODE] My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",

           my_mac[0], my_mac[1], my_mac[2],

           my_mac[3], my_mac[4], my_mac[5]);
 
    xTaskCreate(vTaskRx,       "rx_task",   8192, NULL, 5, NULL);

    xTaskCreate(vTaskTx,       "tx_task",   4096, NULL, 3, NULL);

    //xTaskCreate(vTaskTerminal, "term_task", 4096, NULL, 2, NULL);
 
    // Décommente pour lancer la séquence automatique au démarrage :

    xTaskCreate(vTaskTestSequence, "seq_task", 4096, NULL, 2, NULL);
    //xTaskCreate(vTaskUartTest,  "uart_test",  2048, NULL, 2, NULL);
    //xTaskCreate(vTaskAngleMonitor, "angle_mon", 2048, NULL, 2, NULL);

}
