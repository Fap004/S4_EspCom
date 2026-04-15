#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── UART unique ─────────────────────────────── */
#define UART_PORT        UART_NUM_1

/* ─── Pins ───────────────────────────────────── */
#define UART_RX_PIN      GPIO_NUM_18   // Zybo → ESP
#define UART_TX_PIN      GPIO_NUM_19   // ESP → Basys

/* ─── Baudrate ───────────────────────────────── */
#define UART_BAUD        460800

#define ANGLE_QUEUE_LEN  16
#define UART_SWAP_XY_PIN GPIO_NUM_4

typedef struct {
    int8_t angle;
    int8_t speed;
} zybo_frame_t;

void uart_bridge_init(void);
int  uart_zybo_read_frame(zybo_frame_t *out);
void uart_basys_send_speed(int16_t kmh);

#ifdef __cplusplus
}
#endif