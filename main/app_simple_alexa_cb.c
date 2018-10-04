/*
 *      Copyright 2018, Espressif Systems (Shanghai) Pte Ltd.
 *  All rights regarding this code and its modifications reserved.
 *
 * This code contains confidential information of Espressif Systems
 * (Shanghai) Pte Ltd. No licenses or other rights express or implied,
 * by estoppel or otherwise are granted herein.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <alexa_app_cb.h>
#include <resampling.h>
#include <audio_board.h>
#include "ui_led.h"

#define I2S_PORT_NUM I2S_NUM_0
#define DES_BITS_PER_SAM 16
#define SRC_BITS_PER_SAM 16
/* The standard sampling rate we will setup for the hardware */
#define SAMPLING_RATE    48000
#define CONVERT_BUF_SIZE 1024
#define BUF_SZ (CONVERT_BUF_SIZE * 2)

static const char *TAG = "SIMPLE_ALEXA_CB";

static int16_t convert_buf[BUF_SZ];

static inline int heap_caps_get_free_size_sram()
{
    return heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void alexa_app_dialog_states(alexa_dialog_states_t alexa_states)
{
    ui_led_set(alexa_states);
    ESP_LOGI(TAG, "Current mode is: %d", alexa_states);
}

int alexa_app_set_volume(int vol)
{
    ESP_LOGW(TAG, "Set volume not supported !");
    return 0;
}

int alexa_app_set_mute(alexa_mute_state_t alexa_mute_state)
{
    ESP_LOGW(TAG, "Mute not supported !");
    return 0;
}

int alexa_app_raise_alert(alexa_alert_types_t alexa_alert_type, alexa_alert_state_t alexa_alert_state)
{
    return 0;
}

int alexa_app_playback_data(alexa_resample_param_t *alexa_resample_param, void *buf, ssize_t len)
{
    memset(convert_buf, 0, BUF_SZ);
    audio_resample_config_t resample = {0};
    int current_convert_block_len;
    int convert_block_len = 0;
    int send_offset = 0;
    size_t sent_len = 0;
    int conv_len = 0;

    if (alexa_resample_param->alexa_resample_ch == 1) {
        /* If mono recording, we need to up-sample, so need half the buffer empty, also uint16_t data*/
        convert_block_len = CONVERT_BUF_SIZE / 4;
    } else {
        /* If stereo, we won't need additional buffer space but data is uint16_t*/
        convert_block_len = CONVERT_BUF_SIZE / 2;
    }

    while (len) {
        current_convert_block_len = (convert_block_len > len) ? len : convert_block_len;
        if (current_convert_block_len & 1) {
            printf("Odd bytes in up sampling data, this should be backed up\n");
        }
        conv_len = audio_resample((short *)((char *)buf + send_offset), (short *)convert_buf, alexa_resample_param->alexa_resample_freq, SAMPLING_RATE,
                            current_convert_block_len / 2, BUF_SZ, alexa_resample_param->alexa_resample_ch, &resample);
        if (alexa_resample_param->alexa_resample_ch == 1) {
            conv_len = audio_resample_up_channel((short *)convert_buf, (short *)convert_buf, SAMPLING_RATE, SAMPLING_RATE, conv_len, BUF_SZ, &resample);
        }
        len -= current_convert_block_len;
        /* The reason send_offset and send_len are different is because we could be converting from 24K to 16K */
        send_offset += current_convert_block_len;
        if (DES_BITS_PER_SAM == SRC_BITS_PER_SAM) {
            i2s_write((i2s_port_t)I2S_PORT_NUM, (char *)convert_buf, conv_len * 2, &sent_len, portMAX_DELAY);
        } else {
            if (DES_BITS_PER_SAM > SRC_BITS_PER_SAM) {
                if (DES_BITS_PER_SAM % SRC_BITS_PER_SAM != 0) {
                    ESP_LOGE(TAG, "destination bits need to be multiple of source bits");
                    return -1;
                }
            } else {
                ESP_LOGE(TAG, "destination bits need to greater then and multiple of source bits");
                return -1;
            }
            i2s_write_expand((i2s_port_t)I2S_PORT_NUM, (char *)convert_buf, conv_len * 2, SRC_BITS_PER_SAM, DES_BITS_PER_SAM, &sent_len, portMAX_DELAY);
        }
    }
    return sent_len;
}

int i2s_playback_init()
{
    int ret;
    i2s_config_t i2s_cfg = {};
    audio_board_i2s_init_default(&i2s_cfg);
    ret = i2s_driver_install(I2S_PORT_NUM, &i2s_cfg, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error installing i2s driver for stream");
    } else {
        i2s_pin_config_t pf_i2s_pin = {0};
        audio_board_i2s_pin_config(I2S_PORT_NUM, &pf_i2s_pin);
        i2s_set_pin(I2S_PORT_NUM, &pf_i2s_pin);
        i2s_set_clk(I2S_PORT_NUM, 48000, 16, 2);
    }
    ret = i2s_zero_dma_buffer(I2S_PORT_NUM);
    return ret;
}
