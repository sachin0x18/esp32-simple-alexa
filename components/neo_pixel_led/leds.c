/*
*
* Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/


#include <stdio.h>
#include <stdlib.h>
#include "driver/rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"

//WS2812B
#define LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812 34 // 400ns
#define LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812  16 // 850ns
#define LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812 16 // 800ns
#define LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812  34 // 450ns

static uint8_t *led_val;
static int chancnt;
static int ledcnt[8];
static int led_gpios[8];
static uint8_t led_count;
static int status[8][4];

static rmt_item32_t *rmtdata[8];

static SemaphoreHandle_t mux;


int leds_init(int *cnt, int *gpio, int no)
{
    rmt_config_t rmt_cfg = {
        .rmt_mode = RMT_MODE_TX,
        .clk_div = 2, //10MHz pulse
        .mem_block_num = 1,
        .tx_config = {
            .loop_en = false,
            .carrier_freq_hz = 100, // Not used, but has to be set to avoid divide by 0 err
            .carrier_duty_percent = 50,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .carrier_en = false,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .idle_output_en = true,
        }
    };
    chancnt = no;
    for (int i = 0; i < chancnt; i++) {
        ledcnt[i] = cnt[i];
        led_gpios[i] = gpio[i];
    }

    for (int i = 0; i < chancnt; i++) {
        rmt_cfg.channel = i;
        rmt_cfg.gpio_num = led_gpios[i];
        esp_err_t cfg_ok = rmt_config(&rmt_cfg);
        if (cfg_ok != ESP_OK) {
            printf("Can't configure RMT chan %d!\n", i);
            return false;
        }
        esp_err_t install_ok = rmt_driver_install(i, 0, 0);
        if (install_ok != ESP_OK) {
            printf("Can't install RMT ist for chan %d!\n", i);
            return false;
        }

        rmtdata[i] = malloc(((ledcnt[i] + 1) * 32 + 2) * sizeof(rmt_item32_t));
        if (rmtdata[i] == NULL) {
            printf("Can't allocate data for %d leds on channel %d\n", ledcnt[i], i);
            return false;
        }
    }
    mux = xSemaphoreCreateMutex();
    return true;
}

static const rmt_item32_t wsOne = {
    .duration0 = LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812,
    .level0 = 1,
    .duration1 = LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812,
    .level1 = 0,
};

static const rmt_item32_t wsZero = {
    .duration0 = LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812,
    .level0 = 1,
    .duration1 = LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812,
    .level1 = 0,
};

static const rmt_item32_t wsReset = {
    .duration0 = LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812, //50uS
    .level0 = 1,
    .duration1 = 5000,
    .level1 = 0,
};

static void encByte(rmt_item32_t *rmtdata, uint8_t byte)
{

    int j = 0;
    for (int mask = 0x80; mask != 0; mask >>= 1) {
        if (byte & mask) {
            rmtdata[j++] = wsOne;
        } else {
            rmtdata[j++] = wsZero;
        }
    }
}


#if HAS_STATUS
void leds_set_status(int chn, int r, int g, int b)
{
    status[chn][0] = r;
    status[chn][1] = g;
    status[chn][2] = b;
}

void leds_send_status(int chn, int r, int g, int b)
{
    leds_set_status(chn, r, g, b);
    xSemaphoreTake(mux, portMAX_DELAY);
    int j = 0;
    rmtdata[chn][j++] = wsReset;
    encByte(&rmtdata[chn][j], status[chn][1]);
    encByte(&rmtdata[chn][j + 8], status[chn][0]);
    encByte(&rmtdata[chn][j + 16], status[chn][2]);
#if IS_RGBW
    encByte(&rmtdata[chn][j + 24], status[chn][2]);
    j += 8 * 4;
#else
    j += 8 * 3;
#endif
    rmtdata[chn][j++] = wsReset;
    rmt_write_items(chn, rmtdata[chn], j, true);
    xSemaphoreGive(mux);
}
#endif

void leds_send(uint8_t *data)
{
    int i = 0;
    int j = 0;
    int n = 0;
    int chn = 0;
    xSemaphoreTake(mux, portMAX_DELAY);
    while (chn != chancnt) {
#if HAS_STATUS
        if (j == 0) {
            encByte(&rmtdata[chn][j], status[chn][1]); //GRB -> RGB
            encByte(&rmtdata[chn][j + 8], status[chn][0]); //GRB -> RGB
            encByte(&rmtdata[chn][j + 16], status[chn][2]);
#if IS_RGBW
            encByte(&rmtdata[chn][j + 24], status[chn][3]);
            j += 8 * 4;
#else
            j += 8 * 3;
#endif //IS_RGBW
        }
#endif //HAS_STATUS
        encByte(&rmtdata[chn][j], data[i + 1]);
        encByte(&rmtdata[chn][j + 8], data[i]);
        encByte(&rmtdata[chn][j + 16], data[i + 2]);
#if IS_RGBW
        encByte(&rmtdata[chn][j + 24], data[i + 3]);
        j += 8 * 4;
        i += 4;
#else
        j += 8 * 3;
        i += 3;
#endif
        n++;
        if (n >= ledcnt[chn]) {
            rmtdata[chn][j++] = wsReset;
            rmt_write_items(chn, rmtdata[chn], j, false);
            chn++;
            n = 0;
            j = 0;
        }
    }
    for (i = 0; i < chancnt; i++) {
        rmt_wait_tx_done(i, portMAX_DELAY);
    }
    xSemaphoreGive(mux);
}

void init_led_colour()
{
    led_count = ledcnt[0];
    printf("led_count %d\n", led_count);
    led_val = (uint8_t *)calloc((3 * led_count), sizeof(uint8_t));
}

void glow_led(uint8_t red, uint8_t green, uint8_t blue, uint8_t position)
{

    uint8_t total_led = 3 * position;
    for (int j = 0; j < position; j++) {
        led_val[3 * j]       = red;
        led_val[(3 * j) + 1] = green;
        led_val[(3 * j) + 2] = blue;

    }
    for (int k = 0; k < ((3 * led_count) - (3 * position)); k++) {
        led_val[(3 * position) + k] = 0;
    }
    leds_send(led_val);
}
