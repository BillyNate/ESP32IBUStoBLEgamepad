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
 */

/*
 * This code is based on the BLE mouse/keyboard by Asterics (https://github.com/asterics/esp32_mouse_keyboard),
 * which in turn is based on the BLE HID sample by Neil Kolban and chegewara (https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLETests/SampleHIDDevice.cpp).
 * It requires two libraries in order to work:
 * - ESP32_BLE_Arduino by Neil Kolban (https://github.com/nkolban/ESP32_BLE_Arduino)
 * - FlySkyIbus by Tim Wilkinson (https://gitlab.com/timwilkinson/FlySkyIBus)
 * 
 * Current project can be found at https://github.com/BillyNate/ESP32IBUStoBLEgamepad
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

#include "FlySkyIBus.h"

#include "esp_gap_ble_api.h"
#include "driver/gpio.h"

static config_data_t config;


void update_config()
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("config_c", NVS_READWRITE, &my_handle);
  if(err != ESP_OK) ESP_LOGE("MAIN", "error opening NVS");
  err = nvs_set_str(my_handle, "btname", config.bt_device_name);
  if(err != ESP_OK) ESP_LOGE("MAIN", "error saving NVS - bt name");
  printf("Committing updates in NVS ... ");
  err = nvs_commit(my_handle);
  printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
  nvs_close(my_handle);
}

void ibus_task(void *pvParameter)
{
  ESP_LOGI("IBUS", "IBus processing task started");

  uint16_t channels[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t i;
  joystick_command_t joystickCmd;
  joystickCmd.buttons = 0;
  
  while(1)
  {
    IBus.loop();
    for(i=0; i<8; i++)
    {
      channels[i] = IBus.readChannel(i);
    }

    if(HID_joystick_isConnected() != 0 && joystick_q != NULL)
    {
      joystickCmd.Xaxis = channels[0] >= 1000 ? channels[0] : 1500;
      joystickCmd.Yaxis = channels[1] >= 1000 ? channels[1] : 1500;
      joystickCmd.Xrotate = channels[2] >= 1000 ? channels[2] : 1500;
      joystickCmd.Yrotate = channels[3] >= 1000 ? channels[3] : 1500;

      // Uncomment for testing:
      /*
      joystickCmd.buttons = (rand() % 4) + 2;
      joystickCmd.Xaxis = (rand() % 1000) + 1000;
      joystickCmd.Yaxis = (rand() % 1000) + 1000;
      joystickCmd.Xrotate = (rand() % 1000) + 1000;
      joystickCmd.Yrotate = (rand() % 1000) + 1000;
      */
      xQueueSend(joystick_q, (void*)&joystickCmd, (TickType_t)0);
    }

    // What would be the best update speed? Too fast is causing errors in sending out the BLE Notify...
    vTaskDelay(20 / portTICK_PERIOD_MS);
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
  
  // Activate joystick BT stack
  HID_joystick_init(config.bt_device_name);
  ESP_LOGI("HIDD", "MAIN finished...");

  // Activate IBus serial receiver
  IBus.begin(Serial2, 115200, SERIAL_8N1, IBUS_SERIAL_RXPIN);

  // Set log level to >= INFO for all
  esp_log_level_set("*", ESP_LOG_INFO); 

  // Make sure the serial connection is real
  while(!Serial2)
  {
    delay(50);
  }
  
  // now start the tasks for processing IBus input and indicator LED
  xTaskCreate(&blink_task, "blink", 4096, NULL, configMAX_PRIORITIES, NULL);
  xTaskCreate(&ibus_task, "ibus", 4096, NULL, configMAX_PRIORITIES, NULL);
}
