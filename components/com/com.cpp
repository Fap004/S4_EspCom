#include "com.h"

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_crc.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "COM";

// ----------- Buffer circulaire RX + queue d'indices -----------
static msg_t            rx_buffer[MAX_MESSAGES];
static volatile uint16_t rx_w = 0; // écrit par callback Wi‑Fi
static volatile uint16_t rx_r = 0; // lu par tâche appli
static QueueHandle_t     msg_queue = NULL; // transporte les indices écrits

// ----------- Sémaphore d'envoi (flow control 1 envoi en vol) -----------
static SemaphoreHandle_t tx_sem = NULL;

// ----------- Canal Wi‑Fi fixé à l'init -----------
static uint8_t s_channel = 1;

// ----------- Callbacks ESPNOW -----------
static void espnow_send_cb(const esp_now_send_info_t *info, esp_now_send_status_t status)
{
    // callback exécuté dans la tâche Wi‑Fi (pas ISR)
    (void)info; (void)status;
    if (tx_sem) xSemaphoreGive(tx_sem);
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (!data || len != (int)sizeof(msg_t)) return; // on attend exactement msg_t

    const uint16_t cur  = rx_w;
    const uint16_t next = (uint16_t)((cur + 1) % MAX_MESSAGES);

    if (next != rx_r) { // pas plein
        memcpy(&rx_buffer[cur], data, sizeof(msg_t));
        rx_w = next;
        if (msg_queue) {
            uint16_t idx = cur;
            // callback dans tâche Wi‑Fi ⇒ xQueueSend (pas FromISR)
            (void)xQueueSend(msg_queue, &idx, 0);
        }
    }
}

// ----------- API -----------
void com_get_mac(uint8_t mac[6])
{
    esp_wifi_get_mac(WIFI_IF_STA, mac);
}

void com_init(uint8_t channel)
{
    s_channel = channel;

    // NVS (obligatoire pour Wi‑Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    msg_queue = xQueueCreate(MSG_QUEUE_LEN, sizeof(uint16_t));
    configASSERT(msg_queue != NULL);

    tx_sem = xSemaphoreCreateBinary();
    configASSERT(tx_sem != NULL);
    xSemaphoreGive(tx_sem); // libre au démarrage

    ESP_LOGI(TAG, "ESPNOW ready on channel %u", s_channel);
}

void com_add_peer(const uint8_t mac[6])
{
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = s_channel;
    peer.ifidx   = WIFI_IF_STA; // important en mode STA
    peer.encrypt = false;       // simple/rapide
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

static uint16_t s_send_seq = 0;

esp_err_t com_send(const uint8_t mac[6], const uint8_t *data, size_t len)
{
    if (len > MSG_DATA_LEN) len = MSG_DATA_LEN;

    msg_t m = {};
    m.seq = s_send_seq++;
    memcpy(m.data, data, len);

    // CRC16 sur (seq + data)
    m.crc = esp_crc16_le(UINT16_MAX, (const uint8_t*)&m.seq, sizeof(m.seq) + MSG_DATA_LEN);

    // Un seul envoi en vol (flow control)
    xSemaphoreTake(tx_sem, portMAX_DELAY);
    esp_err_t err = esp_now_send(mac, (const uint8_t*)&m, sizeof(m));

    if (err != ESP_OK) {
        // ne jamais bloquer si erreur immédiate
        xSemaphoreGive(tx_sem);
    }
    return err;
}

int com_read_msg(uint8_t *out_data, size_t *out_len, uint16_t *out_seq)
{
    if (!msg_queue) return 0;

    uint16_t idx;
    if (xQueueReceive(msg_queue, &idx, 0) == pdTRUE) {
        const msg_t *m = &rx_buffer[idx];
        // vérifier le CRC
        uint16_t crc = esp_crc16_le(UINT16_MAX, (const uint8_t*)&m->seq, sizeof(m->seq) + MSG_DATA_LEN);
        if (crc == m->crc) {
            if (out_seq)  *out_seq  = m->seq;
            if (out_len)  *out_len  = MSG_DATA_LEN;
            if (out_data) memcpy(out_data, m->data, MSG_DATA_LEN);
            // avancer le lecteur
            rx_r = (uint16_t)((idx + 1) % MAX_MESSAGES);
            return 1;
        }
        // CRC fail → drop quand même l'entrée
        rx_r = (uint16_t)((idx + 1) % MAX_MESSAGES);
    }
    return 0;
}

int com_read_msg_wait(uint8_t *out_data, size_t *out_len, uint16_t *out_seq, TickType_t ticks_to_wait)
{
    if (!msg_queue) return 0;
    uint16_t idx;
    if (xQueueReceive(msg_queue, &idx, ticks_to_wait) == pdTRUE) {
        const msg_t *m = &rx_buffer[idx];
        uint16_t crc = esp_crc16_le(UINT16_MAX, (const uint8_t*)&m->seq, sizeof(m->seq) + MSG_DATA_LEN);
        if (crc == m->crc) {
            if (out_seq)  *out_seq  = m->seq;
            if (out_len)  *out_len  = MSG_DATA_LEN;
            if (out_data) memcpy(out_data, m->data, MSG_DATA_LEN);
            rx_r = (uint16_t)((idx + 1) % MAX_MESSAGES);
            return 1;
        }
        rx_r = (uint16_t)((idx + 1) % MAX_MESSAGES);
    }
    return 0;
}