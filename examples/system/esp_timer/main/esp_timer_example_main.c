/* esp_timer (high resolution timer) example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "sdkconfig.h"

static void periodic_timer_callback(void* arg);
static void oneshot_timer_callback(void* arg);

static const char* TAG = "example";

void app_main()
{
    /* Create two timers:
     * 1. a periodic timer which will run every 0.5s, and print a message
     * 2. a one-shot timer which will fire after 5s, and re-start periodic
     *    timer with period of 1s.
     */

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    esp_timer_handle_t periodic_timer;
    printf(">>>>>>>>1>>>>>>>>创建周期性定时器\n");
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    /* The timer has been created but is not running yet */

    const esp_timer_create_args_t oneshot_timer_args = {
            .callback = &oneshot_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .arg = (void*) periodic_timer,
            .name = "one-shot"
    };
    esp_timer_handle_t oneshot_timer;
    printf(">>>>>>>>1>>>>>>>>创建单次定时器\n");
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));

    /* Start the timers */
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 500000));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, 5000000));
    ESP_LOGI(TAG, "Started timers, time since boot: %lld us", esp_timer_get_time());

    /* Print debugging information about timers to console every 2 seconds */
    for (int i = 0; i < 5; ++i) {
        printf("|------------------当前运行的定时器:%d--------------------\n",i);
        ESP_ERROR_CHECK(esp_timer_dump(stdout));
        printf("--------------------------------------|\n");
        usleep(2000000);
    }

    
    // esp_light_sleep_start();
    // ESP_LOGI(TAG, "进入轻度睡眠%lld us",esp_timer_get_time());

    // ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(500000));
    // ESP_LOGI(TAG, ">>>>>>>>>>>>唤醒定时器启动%lld us",esp_timer_get_time());

    // ESP_LOGI(TAG, ">>>>>>>>>>>>Woke up from light sleep, time since boot: %lld us",
    // esp_timer_get_time());

    // /* Let the timer run for a little bit more */
    // usleep(2000000);

    /* Clean up and finish the example */
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
    ESP_ERROR_CHECK(esp_timer_delete(periodic_timer));
    ESP_ERROR_CHECK(esp_timer_delete(oneshot_timer));
    ESP_LOGI(TAG, "Stopped and deleted timers");
}

static void periodic_timer_callback(void* arg)
{
    int64_t time_since_boot = esp_timer_get_time();
    ESP_LOGI(TAG, ">>>>>>>>>>>>>>111定期定时器调用，时间自启动: %lld us\n", time_since_boot);
}

static void oneshot_timer_callback(void* arg)
{
    int64_t time_since_boot = esp_timer_get_time();
    ESP_LOGE(TAG, ">>>>>>>>>>>>>>222单次定时器: %lld us", time_since_boot);
    esp_timer_handle_t periodic_timer_handle = (esp_timer_handle_t) arg;
    /* To start the timer which is running, need to stop it first */
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer_handle));
    ESP_LOGE(TAG, ">>>>>>>>>>>>>>333停止周期性定时>>>>>>>\n");
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer_handle, 1000000));
    time_since_boot = esp_timer_get_time();
    ESP_LOGE(TAG, "重启周期性定时 1 S,  %lld us",time_since_boot);
}
