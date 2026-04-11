#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART_ZYBO_PORT      UART_NUM_1
#define UART_ZYBO_RX_PIN    18
#define UART_BASYS_PORT     UART_NUM_1
#define UART_BASYS_TX_PIN   19
#define UART_BAUD           460800        // ← corrigé (était 1000000)

#define UART_ZYBO_FRAME_LEN  2            // ← 2 bytes par trame (angle + vitesse)
#define UART_BASYS_FRAME_LEN 4
#define ANGLE_QUEUE_LEN      16

// Trame complète reçue du Zybo
typedef struct {
    int8_t  angle;   // -128 à +127  → -30°..+30°
    int8_t  speed;   // -128 à +127  → -100%..+100% (négatif = marche arrière?)
} zybo_frame_t;

void uart_bridge_init(void);
int  uart_zybo_read_frame(zybo_frame_t *out);   // ← remplace uart_zybo_read_angle
void uart_basys_send_speed(int16_t rpm);

#ifdef __cplusplus
}
#endif