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
#define UART_BAUD           1000000

#define UART_ZYBO_FRAME_LEN  1
#define UART_BASYS_FRAME_LEN 4
#define ANGLE_QUEUE_LEN      16

void uart_bridge_init(void);
int  uart_zybo_read_angle(int8_t *out_angle);
void uart_basys_send_speed(int16_t rpm);

#ifdef __cplusplus
}
#endif