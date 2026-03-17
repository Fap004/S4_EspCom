#include "com.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <inttypes.h>

// MAC du robot qui envoie
static const uint8_t peer_mac[6] = { 0x20, 0x6E, 0xF1, 0x0D, 0x4D, 0xB8 };

#define MSG_DATA_LEN 8

// ===================== RX TASK =====================
void rx_task(void *arg)
{
    uint8_t  data[MSG_DATA_LEN];
    size_t   len;
    uint16_t seq;

    while (true)
    {
        if (com_read_msg_wait(data, &len, &seq, portMAX_DELAY))
        {
            if (len != MSG_DATA_LEN) continue;

            uint32_t timestamp =
                ((uint32_t)data[0] << 24) |
                ((uint32_t)data[1] << 16) |
                ((uint32_t)data[2] << 8)  |
                data[3];

            int16_t rpmL =
                ((int16_t)data[4] << 8) |
                data[5];

            int16_t rpmR =
                ((int16_t)data[6] << 8) |
                data[7];

            printf("t=%" PRIu32 " ms | rpmL=%d | rpmR=%d | seq=%u\n",
                   timestamp, rpmL, rpmR, seq);
        }
    }
}

// ===================== MAIN =====================
extern "C" void app_main(void)
{
    const uint8_t channel = 1;

    com_init(channel);

    uint8_t my_mac[6];
    com_get_mac(my_mac);

    printf("[NODE] My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           my_mac[0], my_mac[1], my_mac[2],
           my_mac[3], my_mac[4], my_mac[5]);

    // ajouter le robot comme peer
    com_add_peer(peer_mac);

    xTaskCreate(rx_task, "rx_task", 4096, NULL, 5, NULL);
}