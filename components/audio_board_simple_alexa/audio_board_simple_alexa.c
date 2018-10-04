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

#include <string.h>
#include <esp_log.h>
#include <audio_board.h>

#define PLAT_TAG "AUDIO_BOARD"

#define PLAT_ASSERT(a, format, b, ...) \
    if ((a) == NULL) { \
        ESP_LOGE(PLAT_TAG, format, ##__VA_ARGS__); \
        return b;\
    }

esp_err_t audio_board_i2s_pin_config(int port_num, i2s_pin_config_t *pf_i2s_pin)
{   
    PLAT_ASSERT(pf_i2s_pin, "Error assigning i2s pins", -1);
    switch(port_num) {
        case 0:
            pf_i2s_pin->bck_io_num = GPIO_NUM_26;
            pf_i2s_pin->ws_io_num =  GPIO_NUM_25;
            pf_i2s_pin->data_out_num = GPIO_NUM_22;
            pf_i2s_pin->data_in_num = -1;
            break;
        case 1:
            pf_i2s_pin->bck_io_num = GPIO_NUM_13;
            pf_i2s_pin->ws_io_num =  GPIO_NUM_18;
            pf_i2s_pin->data_out_num = -1;
            pf_i2s_pin->data_in_num = GPIO_NUM_5;
            break;
        default:
            ESP_LOGE(PLAT_TAG, "Entered i2s port number is wrong");
            return ESP_FAIL;
    }

    return ESP_OK; 
}

esp_err_t audio_board_i2c_pin_config(int port_num, i2c_config_t *pf_i2c_pin)
{   
    PLAT_ASSERT(pf_i2c_pin, "Error assigning i2c pins", -1);
    
    switch(port_num) {
        case 0:
            pf_i2c_pin->sda_io_num = GPIO_NUM_21;
            pf_i2c_pin->scl_io_num = GPIO_NUM_23;
            pf_i2c_pin->sda_pullup_en = GPIO_PULLUP_ENABLE;
            pf_i2c_pin->scl_pullup_en = GPIO_PULLUP_ENABLE;
            break;
        case 1:
            pf_i2c_pin->sda_io_num = -1;
            pf_i2c_pin->scl_io_num = -1;
            pf_i2c_pin->sda_pullup_en = GPIO_PULLUP_ENABLE;
            pf_i2c_pin->scl_pullup_en = GPIO_PULLUP_ENABLE;
            break;
        default:
            ESP_LOGE(PLAT_TAG, "Entered i2c port number is wrong");
            return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t audio_board_i2s_init_default(i2s_config_t *i2s_cfg_dft)
{
    i2s_cfg_dft->mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX;
    i2s_cfg_dft->sample_rate = 16000;
    i2s_cfg_dft->bits_per_sample = 16;
    i2s_cfg_dft->channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2s_cfg_dft->communication_format = I2S_COMM_FORMAT_I2S;
    i2s_cfg_dft->dma_buf_count = 3;                   /*!< number of dma buffer */
    i2s_cfg_dft->dma_buf_len = 300;                   /*!< size of each dma buffer (Byte) */
    i2s_cfg_dft->intr_alloc_flags = 0;
    i2s_cfg_dft->use_apll = 0;

    return ESP_OK;
}

esp_err_t audio_board_button_config(adc1_channel_t *pf_button_pin)
{
    *pf_button_pin = ADC1_CHANNEL_3;
    return ESP_OK;
}
