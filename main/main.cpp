// main.cpp
#include "com.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <inttypes.h>

// ⚠️ Remplace par la MAC de l'autre ESP (peer)
static const uint8_t peer_mac[6] = { 0x20, 0x6E, 0xF1, 0x09, 0xB3, 0xA0 };

#define MSG_DATA_LEN 2  // longueur du message en octets

// ===================== TX TASK =====================
void tx_task(void *arg)
{
    uint16_t vitesse = 0;
    const uint8_t id = 3;
    const TickType_t period = pdMS_TO_TICKS(2); // ~500 Hz

    while (true)
    {
        uint16_t packed = ((vitesse & 0x1FFF) << 3) | (id & 0x07);

        uint8_t data[MSG_DATA_LEN] = {
            (uint8_t)(packed >> 8),
            (uint8_t)(packed & 0xFF)
        };

        esp_err_t err = com_send(peer_mac, data, sizeof(data));
        if (err != ESP_OK) {
            printf("[TX] esp_now_send err=%d\n", (int)err);
        }

        vitesse = (vitesse + 1) % 32001;
        vTaskDelay(period);
    }
}

// ===================== RX TASK =====================
void rx_task(void *arg)
{
    uint8_t  data[MSG_DATA_LEN];
    size_t   len;
    uint16_t seq;
    TickType_t last_print = xTaskGetTickCount();
    const TickType_t print_period = pdMS_TO_TICKS(250);
    uint32_t rx_count = 0;

    while (true)
    {
        // attend indéfiniment un message (bloquant)
        if (com_read_msg_wait(data, &len, &seq, portMAX_DELAY))
        {
            uint16_t packed  = ((uint16_t)data[0] << 8) | data[1];
            uint16_t vitesse = (packed >> 3) & 0x1FFF;
            uint8_t  id      = packed & 0x07;

            rx_count++;
            TickType_t now = xTaskGetTickCount();
            if (now - last_print >= print_period)
            {
                printf("[RX] Dernier: ID=%u | Vitesse=%u | seq=%u | total=%u\n",
                       id, vitesse, seq, (unsigned)rx_count);
                last_print = now;
            }
        }
    }
}

// ===================== MAIN =====================
extern "C" void app_main(void)
{
    const uint8_t channel = 1;  // canal commun
    com_init(channel);

    uint8_t my_mac[6];
    com_get_mac(my_mac);
    printf("[NODE] My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           my_mac[0], my_mac[1], my_mac[2],
           my_mac[3], my_mac[4], my_mac[5]);

    // Ajouter le peer pour l'émission
    com_add_peer(peer_mac);

    // Création des tâches TX et RX simultanées
    xTaskCreate(tx_task, "tx_task", 4096, NULL, 5, NULL);
    xTaskCreate(rx_task, "rx_task", 4096, NULL, 5, NULL);
}
