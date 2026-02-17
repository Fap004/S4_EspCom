#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// ⚠️ Ajout indispensable : TickType_t est défini ici
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// Données utiles envoyées (2 octets comme dans l'exemple)
#define MSG_DATA_LEN 2

// Taille du buffer circulaire et de la queue RX
#define MAX_MESSAGES   256
#define MSG_QUEUE_LEN  128

// Structure compacte envoyée en ESP-NOW
typedef struct __attribute__((packed)) {
    uint16_t seq;                 // numéro de séquence TX
    uint8_t  data[MSG_DATA_LEN];  // charge utile
    uint16_t crc;                 // CRC16 sur (seq+data)
} msg_t;

void com_init(uint8_t channel);
void com_add_peer(const uint8_t mac[6]);
esp_err_t com_send(const uint8_t mac[6], const uint8_t *data, size_t len);
int  com_read_msg(uint8_t *out_data, size_t *out_len, uint16_t *out_seq);
void com_get_mac(uint8_t mac[6]);

// Version bloquante (attend jusqu'à ticks_to_wait pour un message)
int  com_read_msg_wait(uint8_t *out_data, size_t *out_len, uint16_t *out_seq,
                       TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif