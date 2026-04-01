#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Pins ──────────────────────────────────────────────────────────────────────
#define UART_ZYBO_PORT      UART_NUM_0
#define UART_ZYBO_RX_PIN    16

#define UART_BASYS_PORT     UART_NUM_1
#define UART_BASYS_TX_PIN   19

#define UART_BAUD           115200   

// ── Trames ────────────────────────────────────────────────────────────────────
// RX Zybo  → [angle : int8_t] [checksum : uint8_t]  (XOR du byte angle)
#define UART_ZYBO_FRAME_LEN  2

// TX Basys → [rpmL_hi] [rpmL_lo] [rpmR_hi] [rpmR_lo]  (big-endian, int16_t)
#define UART_BASYS_FRAME_LEN 4

// ── API ───────────────────────────────────────────────────────────────────────
void uart_bridge_init(void);

// Lit l'angle reçu du Zybo (non-bloquant, retourne 0 si rien de disponible)
int  uart_zybo_read_angle(int8_t *out_angle);

// Envoie les RPM vers la Basys
void uart_basys_send_rpm(int16_t rpmL, int16_t rpmR);

#ifdef __cplusplus
}
#endif