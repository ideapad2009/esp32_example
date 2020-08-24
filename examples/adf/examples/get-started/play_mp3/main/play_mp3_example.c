/* Play mp3 file by audio pipeline

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "board.h"
#include <stdio.h>
#include "esp_attr.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

static const char *TAG = "PLAY_MP3_FLASH";

//You can get these value from the datasheet of servo you use, in general pulse width varies between 1000 to 2000 mocrosecond
#define SERVO_MIN_PULSEWIDTH 1000 //Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2000 //Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 90 //Maximum angle in degree upto which servo can rotate

static void mcpwm_example_gpio_initialize()
{
    printf("initializing mcpwm servo control gpio......\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 18);    //Set GPIO 18 as PWM0A, to which servo is connected
}

/**
 * @brief Use this function to calcute pulse width for per degree rotation
 *
 * @param  degree_of_rotation the angle in degree to which servo has to rotate
 *
 * @return
 *     - calculated pulse width
 */
static uint32_t servo_per_degree_init(uint32_t degree_of_rotation)
{
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}

/**
 * @brief Configure MCPWM module
 */
void mcpwm_example_servo_control(void *arg)
{
    uint32_t angle, count;
    //1. mcpwm gpio initialization
    mcpwm_example_gpio_initialize();

    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm......\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
    while (1) {
        for (count = 0; count < SERVO_MAX_DEGREE;) {
            count += 10;
            printf("Angle of rotation: %d\n", count);
            angle = servo_per_degree_init(count);
            printf("pulse width: %dus\n", angle);
            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
            vTaskDelay(10);     //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation at 5V
        }
    }
}

void test()
{
    printf("Testing servo motor.......\n");
    xTaskCreate(mcpwm_example_servo_control, "mcpwm_example_servo_control", 4096, NULL, 5, NULL);
}


extern const uint8_t adf_music_mp3_start[] asm("_binary_adf_music_mp3_start");
extern const uint8_t adf_music_mp3_end[]   asm("_binary_adf_music_mp3_end");
int mp3_pos;
int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
   
    int read_size = adf_music_mp3_end - adf_music_mp3_start - mp3_pos;
    if (read_size == 0) {
        return AEL_IO_DONE;
    } else if (len < read_size) {
        read_size = len;
    }
    memcpy(buf, adf_music_mp3_start + mp3_pos, read_size);
    mp3_pos += read_size;
    return read_size;
}

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_writer, mp3_decoder;
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

        pipeline = audio_pipeline_init(&pipeline_cfg);
        mem_assert(pipeline);

        ESP_LOGI(TAG, "[2.1] Create mp3 decoder to decode mp3 file and set custom read callback");
        mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
        mp3_decoder = mp3_decoder_init(&mp3_cfg);
        audio_element_set_read_cb(mp3_decoder, mp3_music_read_cb, NULL);

        ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
        i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
        i2s_cfg.type = AUDIO_STREAM_WRITER;
        i2s_cfg.i2s_config.sample_rate = 48000;
        i2s_stream_writer = i2s_stream_init(&i2s_cfg);  
        ESP_LOGI(TAG, "[2.3] Register all elements to audio pipeline");
        audio_pipeline_register(pipeline, mp3_decoder, "mp3");
        audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

        ESP_LOGI(TAG, "[2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]");

        /**Zl38063 does not support 44.1KHZ frequency, so resample needs to be used to convert files to other rates.
         * You can transfer to 16kHZ or 48kHZ.
         */

        const char *link_tag[2] = {"mp3", "i2s"};
        audio_pipeline_link(pipeline, &link_tag[0], 2);
        ESP_LOGI(TAG, "[ 3 ] Set up  event listener");
        audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
        audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

        ESP_LOGI(TAG, "[3.1] Listening event from all elements of pipeline");
        audio_pipeline_set_listener(pipeline, evt);
        ESP_LOGI(TAG, "[ 4 ] Start audio_pipeline");
        audio_pipeline_run(pipeline);

    uint32_t angle, count;
    //1. mcpwm gpio initialization
    mcpwm_example_gpio_initialize();

    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm......\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;    //frequency = 50Hz, i.e. for every servo motor time period should be 20ms
    pwm_config.cmpr_a = 0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings


        // test();
    while (1) {
        audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                switch (el_state) {
                    case AEL_STATE_INIT :
                        ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                        audio_pipeline_run(pipeline);
                        break;
                    case AEL_STATE_FINISHED :
                        ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
                        audio_pipeline_stop(pipeline);
                        audio_pipeline_wait_for_stop(pipeline);

                        mp3_pos = 0;
                        audio_pipeline_resume(pipeline);

                        vTaskDelay(100);
                        for(int i=100;i>60;i--){
                            angle = servo_per_degree_init(i);
                            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
                            vTaskDelay(1);
                        }

                        for(int i=60;i<100;i++){
                            angle = servo_per_degree_init(i);
                            mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, angle);
                            vTaskDelay(1);
                        }
                         
                        break;
                    default :
                    ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
                }
  

        vTaskDelay(1000);
        ESP_LOGI(TAG, "===========================================================");
    }
}
