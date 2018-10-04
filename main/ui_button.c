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

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs_flash.h>
#include <esp_timer.h>
#include <speaker.h>
#include <mem_utils.h>
#include <alexa_app_cb.h>
#include <app_dsp.h>
#include "ui_button.h"
#include <ui_led.h>
        
#define UI_BUTTON_QUEUE_LENGTH 1

#define UI_BTN_TASK_DELAY 200
#define FACTORY_RST_DELAY 5000

#define GPIO_RECORD_PIN  ((((uint64_t) 1) << GPIO_RECORD_BUTTON))

static const char *UI_BUTTON_TAG = "ui button";

typedef struct {
    esp_timer_handle_t esp_timer_handler;
    SemaphoreHandle_t ui_button_sem;
    TaskHandle_t ui_button_task_handle;
    xQueueHandle ui_button_queue;
} ui_button_t;

static ui_button_t button_st = {NULL, NULL, NULL, NULL};

static void ui_button_reset_timer_cb()
{
    ESP_LOGI(UI_BUTTON_TAG, "Reset initiated, erasing nvs flash and rebooting");
    ui_led_set(LED_RESET);
    vTaskDelay(3333 / portTICK_RATE_MS);
    ui_led_set(LED_OFF);
    nvs_flash_erase();
    esp_restart();
}

static void ui_button_task(void *arg)
{
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(button_st.ui_button_queue, &io_num, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(UI_BUTTON_TAG, "Failed to receive from led queue");
        }
        if (io_num == GPIO_RECORD_BUTTON) {
            if (!gpio_get_level(GPIO_RECORD_BUTTON)) {
                ESP_LOGI(UI_BUTTON_TAG, "tap to talk pressed");
                app_dsp_send_recognize();
                vTaskDelay(pdMS_TO_TICKS(333));
                esp_timer_start_once(button_st.esp_timer_handler, (FACTORY_RST_DELAY * 1000));
            } else {
                esp_timer_stop(button_st.esp_timer_handler);
            }
        } 
    }
}

static void IRAM_ATTR gpio_isr_handler_t(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(button_st.ui_button_queue, &gpio_num, NULL);
}

static esp_err_t ui_button_gpio_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_RECORD_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_RECORD_BUTTON, gpio_isr_handler_t, (void *) GPIO_RECORD_BUTTON);
    return ESP_OK;
}

esp_err_t ui_button_init()
{
    button_st.ui_button_queue = xQueueCreate(UI_BUTTON_QUEUE_LENGTH, sizeof(uint32_t));
    if (button_st.ui_button_queue == NULL) {
        ESP_LOGE(UI_BUTTON_TAG, "Could not create queue");
        return ESP_FAIL;
    }
    ui_button_gpio_init();
    StackType_t *ui_button_task_stack = (StackType_t *)mem_alloc(UI_BUTTON_TASK_BUFFER_SZ, EXTERNAL);
    static StaticTask_t ui_button_task_buf;
    button_st.ui_button_sem = xSemaphoreCreateBinary();
    if (button_st.ui_button_sem == NULL) {
        ESP_LOGE(UI_BUTTON_TAG, "Could not create queue");
        return ESP_FAIL;
    }
    button_st.ui_button_task_handle = xTaskCreateStatic(ui_button_task, "ui-button-thread", UI_BUTTON_TASK_BUFFER_SZ,
                                      NULL, CONFIG_ESP32_PTHREAD_TASK_PRIO_DEFAULT, ui_button_task_stack, &ui_button_task_buf);
    if (button_st.ui_button_task_handle == NULL) {
        ESP_LOGE(UI_BUTTON_TAG, "Could not create button task");
        return ESP_FAIL;
    }
    esp_timer_init();
    esp_timer_create_args_t timer_arg = {
        .callback = ui_button_reset_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ui button reset timer",
    };
    if (esp_timer_create(&timer_arg, &button_st.esp_timer_handler) != ESP_OK) {
        ESP_LOGE(UI_BUTTON_TAG, "Failed to create esp timer for reset-to-factory button");
        return ESP_FAIL;
    }
    return ESP_OK;
}

