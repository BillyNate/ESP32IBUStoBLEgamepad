/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * Copyright 2017 Benjamin Aigner <beni@asterics-foundation.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "driver/gpio.h"

#include "config.h"
#include "HID_joystick.h"

#include "esp_gap_ble_api.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#define EXT_UART_TAG "EXT_UART"
#define CONSOLE_UART_TAG "CONSOLE_UART"

//static joystick_data_t joystick;//currently unused, no joystick implemented
static config_data_t config;


void update_config()
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("config_c", NVS_READWRITE, &my_handle);
  if(err != ESP_OK) ESP_LOGE("MAIN","error opening NVS");
  err = nvs_set_str(my_handle, "btname", config.bt_device_name);
  if(err != ESP_OK) ESP_LOGE("MAIN","error saving NVS - bt name");
  err = nvs_set_u8(my_handle, "locale", config.locale);
  if(err != ESP_OK) ESP_LOGE("MAIN","error saving NVS - locale");
  printf("Committing updates in NVS ... ");
  err = nvs_commit(my_handle);
  printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
  nvs_close(my_handle);
}

void uart_console(void *pvParameters)
{
  char character;
  joystick_command_t joystickCmd;
  
  ESP_LOGI("UART", "console UART processing task started");
  
  while(1)
  {
    if(joystick_q == NULL)
    {
      ESP_LOGE(CONSOLE_UART_TAG, "queue not initialized");
      continue;
    }

    if(HID_joystick_isConnected() != 0)
    {
      joystickCmd.buttons = rand() % 256;
      joystickCmd.Xaxis = (rand() % 256) - 128;
      joystickCmd.Yaxis = (rand() % 256) - 128;
      joystickCmd.Xrotate = (rand() % 256) - 128;
      joystickCmd.Yrotate = (rand() % 256) - 128;

      ESP_LOGI(CONSOLE_UART_TAG, "testing joystick: %d/%d/%d/%d/%d", joystickCmd.buttons, joystickCmd.Xaxis, joystickCmd.Yaxis, joystickCmd.Xrotate, joystickCmd.Yrotate);
      xQueueSend(joystick_q, (void*)&joystickCmd, (TickType_t)0);
      
      vTaskDelay(1000);
    }
    else
    {
      vTaskDelay(100);
    }
  }
}

void blink_task(void *pvParameter)
{
  // Initialize GPIO pins
  gpio_pad_select_gpio(INDICATOR_LED_PIN);
  gpio_set_direction(INDICATOR_LED_PIN, GPIO_MODE_OUTPUT);
  int blinkTime;
  
  while(1)
  {
    if(HID_joystick_isConnected())
    {
      blinkTime = 1000;
    }
    else
    {
      blinkTime = 250;
    }
    
    /* Blink off (output low) */
    gpio_set_level(INDICATOR_LED_PIN, 0);
    vTaskDelay(blinkTime / portTICK_PERIOD_MS);
    /* Blink on (output high) */
    gpio_set_level(INDICATOR_LED_PIN, 1);
    vTaskDelay(blinkTime / portTICK_PERIOD_MS);
  }
}

extern "C" void app_main()
{
  esp_err_t ret;
  
  // Initialize NVS.
  ret = nvs_flash_init();
  if(ret == ESP_ERR_NVS_NO_FREE_PAGES)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  
  // Read config
  nvs_handle my_handle;
  
  ESP_LOGI("MAIN", "loading configuration from NVS");
  
  ret = nvs_open("config_c", NVS_READWRITE, &my_handle);
  
  if(ret != ESP_OK)
  {
    ESP_LOGE("MAIN", "error opening NVS");
  }
  
  size_t available_size = MAX_BT_DEVICENAME_LENGTH;
  strcpy(config.bt_device_name, GATTS_TAG);
  
  nvs_get_str(my_handle, "btname", config.bt_device_name, &available_size);
  if(ret != ESP_OK) 
  {
    ESP_LOGE("MAIN", "error reading NVS - bt name, setting to default");
    strcpy(config.bt_device_name, GATTS_TAG);
  }
  else
  {
    ESP_LOGI("MAIN", "bt device name is: %s", config.bt_device_name);
  }
  nvs_close(my_handle);
  
  //activate mouse & keyboard BT stack (joystick is not working yet)
  HID_joystick_init(0, config.bt_device_name);
  ESP_LOGI("HIDD", "MAIN finished...");
  
  esp_log_level_set("*", ESP_LOG_INFO); 
  
  // now start the tasks for processing UART input and indicator LED  
  xTaskCreate(&uart_console, "console", 4096, NULL, configMAX_PRIORITIES, NULL);
  xTaskCreate(&blink_task, "blink", 4096, NULL, configMAX_PRIORITIES, NULL);
}
