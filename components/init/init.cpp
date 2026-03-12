#include "Init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt.h"

// Timings pour WS2812 (avec clk_div = 2)
#define T0H 16  // 0.4us
#define T0L 34  // 0.85us
#define T1H 32  // 0.8us
#define T1L 18  // 0.45us

static rmt_item32_t led_bit(uint8_t bit)
{
    rmt_item32_t item;
    if (bit) {
        item.level0 = 1; item.duration0 = T1H;
        item.level1 = 0; item.duration1 = T1L;
    } else {
        item.level0 = 1; item.duration0 = T0H;
        item.level1 = 0; item.duration1 = T0L;
    }
    return item;
}

static void byte_to_rmt(uint8_t byte, rmt_item32_t* items)
{
    for (int i = 0; i < 8; i++) {
        items[i] = led_bit(byte & (1 << (7 - i)));
    }
}

void ws2812_init(void)
{
    // Configure RMT
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LED_PIN, RMT_CHANNEL);
    config.clk_div = 2;
    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);

    // Éteint la LED au départ
    ws2812_set_color(0, 0, 0);
}

void ws2812_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    rmt_item32_t items[24];
    byte_to_rmt(g, &items[0]); // GRB
    byte_to_rmt(r, &items[8]);
    byte_to_rmt(b, &items[16]);
    rmt_write_items(RMT_CHANNEL, items, 24, true);
    vTaskDelay(pdMS_TO_TICKS(1));
}
