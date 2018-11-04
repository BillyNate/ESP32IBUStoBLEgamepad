/*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
* 
*   http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
* 
* Copyright Neil Kolban
 */

/** @file
 * @brief This file is the implementation for Neil Kolbans CPP utils
 * 
 * It initializes a queue for sending joystick data
 * from within the C-side. C++ classes are instantiated here.
 * If you want to have a different BLE HID device, you need to adapt this file.
 * 
 * @note Thank you very much Neil Kolban for this impressive work!
*/

#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include "sdkconfig.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include <esp_log.h>
#include <string>
#include <Task.h>
#include "HID_joystick.h"

static char LOG_TAG[] = "HAL_BLE";

/// @brief Input queue for sending joystick reports
QueueHandle_t joystick_q;

/// @brief Is the BLE currently connected?
uint8_t isConnected = 0;

///@brief The BT Devicename for advertising
char btname[40];


//static BLEHIDDevice class instance for communication (sending reports)
static BLEHIDDevice* hid;
//BLE server handle
BLEServer *pServer;
//characteristic for sending joystick reports to the host
BLECharacteristic* inputJoystick;

/** @brief Constant report map for joystick
 * 
 * This report map will be used on init do build a report map according
 * to init functions (with activated interfaces).
 * 
 * @note Report id is on all reports in offset 7.
 * */
const uint8_t reportMapJoystick[] = {
  USAGE_PAGE(1),            0x01,  // Generic Desktop
  USAGE(1),                 0x05,  // Gamepad?
  COLLECTION(1),            0x01,  // Application
    COLLECTION(1),          0x00,  // Physical
      REPORT_ID(1),         0x01,

      /*
      USAGE_PAGE(1),        0x01,
      */
      USAGE(1),             0x30,  // X axis
      USAGE(1),             0x31,  // Y axis
      USAGE(1),             0x33,  // X rotation
      USAGE(1),             0x34,  // Y rotation
      /*
      LOGICAL_MINIMUM(2),   0xE8, 0x03,// 1000
      LOGICAL_MAXIMUM(2),   0xD0, 0x07,// 2000
      */
      LOGICAL_MINIMUM(2),   0x00, 0x00,// 0000
      LOGICAL_MAXIMUM(2),   0xE8, 0x03,// 1000
      PHYSICAL_MINIMUM(2),  0x00, 0x00,// 0000
      PHYSICAL_MAXIMUM(2),  0xE8, 0x03,// 1000
      REPORT_SIZE(1),       0x10,  // 16bit values
      REPORT_COUNT(1),      0x04,  // 4 values (the 4 usages)
      HIDINPUT(1),          0x02,  // data, variable, absolute
      
      USAGE_PAGE(1),        0x09,  // Button
      USAGE_MINIMUM(1),     0x01,
      USAGE_MAXIMUM(1),     0x08,  // 8 buttons
      LOGICAL_MINIMUM(1),   0x00,
      LOGICAL_MAXIMUM(1),   0x01,
      REPORT_SIZE(1),       0x01,
      REPORT_COUNT(1),      0x08,  // 8 reports
      HIDINPUT(1),          0x02,  // data, variable, absolute
      
    END_COLLECTION(0),
  END_COLLECTION(0)
};

class JoystickTask : public Task
{
  void run(void*)
  {
    joystick_command_t cmd;
    while(1)
    {
      // wait for a new joystick command
      if(xQueueReceive(joystick_q, &cmd, 10000))
      {
        //ESP_LOGI(LOG_TAG, "Joystick received: %d/%d/%d/%d", cmd.Xaxis, cmd.Yaxis, cmd.Xrotate, cmd.Yrotate);
        uint8_t a[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        a[0] = (cmd.Xaxis - 1000) & 0xff;
        a[1] = ((cmd.Xaxis - 1000) >> 8);
        a[2] = (cmd.Yaxis - 1000) & 0xff;
        a[3] = ((cmd.Yaxis - 1000) >> 8);
        a[4] = (cmd.Xrotate - 1000) & 0xff;
        a[5] = ((cmd.Xrotate - 1000) >> 8);
        a[6] = (cmd.Yrotate - 1000) & 0xff;
        a[7] = ((cmd.Yrotate - 1000) >> 8);
        a[8] = cmd.buttons;
        
        inputJoystick->setValue(a, sizeof(a));
        inputJoystick->notify();
      }
    }
  }
};
JoystickTask *joystick; //instance for this task

class CBs: public BLEServerCallbacks
{
  void onConnect(BLEServer* pServer)
  {
    BLE2902* desc;
    
    isConnected = 1;
    
    desc = (BLE2902*) inputJoystick->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(true);
    joystick->start();
      
    ESP_LOGI(LOG_TAG, "Client connected!");
  }

  void onDisconnect(BLEServer* pServer)
  {
    BLE2902* desc;
    
    isConnected = 0;
    
    desc = (BLE2902*) inputJoystick->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(false);
    joystick->stop();
    
    //restart advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
    ESP_LOGI(LOG_TAG, "Client disconnected, restarting advertising");
  }
};

uint32_t passKey = 1307;
/** @brief security callback
 * 
 * This class is passed to the BLEServer as callbacks for security
 * related actions. Depending on IO_CAP configuration & host, different
 * types of security actions are required for bonding this device to a
 * host. */
class CB_Security: public BLESecurityCallbacks 
{
  // Request a pass key to be typed in on the host
  uint32_t onPassKeyRequest()
  {
    ESP_LOGE(LOG_TAG, "The passkey request %d", passKey);
    vTaskDelay(25000);
    return passKey;
  }
  
  // The host sends a pass key to the ESP32 which needs to be displayed
  //and typed into the host PC
  void onPassKeyNotify(uint32_t pass_key)
  {
    ESP_LOGE(LOG_TAG, "The passkey Notify number: %d", pass_key);
    passKey = pass_key;
  }
  
  // CB for testing if a host is allowed to connect, in our case always yes.
  bool onSecurityRequest()
  {
    return true;
  }

  // CB on a completed authentication (not depending on status)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl)
  {
    if(auth_cmpl.success)
    {
      ESP_LOGI(LOG_TAG, "remote BD_ADDR:");
      esp_log_buffer_hex(LOG_TAG, auth_cmpl.bd_addr, sizeof(auth_cmpl.bd_addr));
      ESP_LOGI(LOG_TAG, "address type = %d", auth_cmpl.addr_type);
    }
    ESP_LOGI(LOG_TAG, "pair status = %s", auth_cmpl.success ? "success" : "fail");
  }
  
  // You need to confirm the given pin
  bool onConfirmPIN(uint32_t pin)
  {
    ESP_LOGE(LOG_TAG, "Confirm pin: %d", pin);
    return true;
  }
};

/** @brief Main BLE HID-over-GATT task
 * 
 * This task is used to initialize 1 task for joystick
 * as well as intializing the BLE device:
 * * Init device
 * * Create a new server
 * * Attach a HID over GATT implementation to the server
 * * Create the input & output reports for the different HID devices
 * * Create & add the HID report map
 * * Finally start the server & services and start advertising
 * */
class BLE_HOG: public Task
{
  void run(void *data)
  {
    ESP_LOGD(LOG_TAG, "Initialising BLE HID device.");

    /*
     * Create new task instances, if necessary
     */
    joystick = new JoystickTask();
    joystick->setStackSize(8096);
    
    BLEDevice::init(btname);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new CBs());
    //BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
    BLEDevice::setSecurityCallbacks(new CB_Security());

    /*
     * Instantiate hid device
     */
    hid = new BLEHIDDevice(pServer);
    
    /*
     * Set manufacturer name
     * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.manufacturer_name_string.xml
     */
    std::string name = "Unknown";
    hid->manufacturer()->setValue(name);

    /*
     * Set pnp parameters
     * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.pnp_id.xml
     */
    //hid->pnp(0x01, 0xE502, 0xA111, 0x0210); //BT SIG assigned VID
    hid->pnp(0x02, 0xE502, 0xA111, 0x0210); //USB assigned VID

    /*
     * Set hid informations
     * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.hid_information.xml
     */
    //const uint8_t val1[] = {0x01, 0x11, 0x00, 0x03};
    //hid->hidInfo()->setValue((uint8_t*)val1, 4);
    hid->hidInfo(0x00, 0x01);

    /*
     * Build a report map, depending on init function.
     * For each enabled interface (joystick) the
     * corresponding report map is copied to a new map which is 
     * used for initializing the BLEHID class.
     * Report IDs are changed accordingly.
     */
    size_t reportMapSize = 0;
    reportMapSize += sizeof(reportMapJoystick);
    
    uint8_t *reportMap = (uint8_t *)malloc(reportMapSize);
    uint8_t *reportMapCurrent = reportMap;
    uint8_t reportID = 1;
    
    if(reportMap == nullptr)
    {
      ESP_LOGE(LOG_TAG, "Cannot allocate memory for the report map, cannot start HID");
    }
    else
    {
      //copy report map for joystick to allocated full report map, if activated
      memcpy(reportMapCurrent, reportMapJoystick, sizeof(reportMapJoystick));
      reportMap[7] = reportID;
      reportMapCurrent += sizeof(reportMapJoystick);
      
      //create in characteristics/reports for joystick
      inputJoystick = hid->inputReport(reportID);
      
      ESP_LOGI(LOG_TAG, "Joystick added @report ID %d, current report Map:", reportID);
      ESP_LOG_BUFFER_HEXDUMP(LOG_TAG, reportMap, (uint16_t)(reportMapCurrent-reportMap), ESP_LOG_INFO);
      
      ESP_LOGI(LOG_TAG, "Final report map size: %d B", reportMapSize);
          
      /*
       * Set report map (here is initialized device driver on client side)
       * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.report_map.xml
       */
      hid->reportMap((uint8_t*)reportMap, reportMapSize);
  
      /*
       * We are prepared to start hid device services. Before this point we can change all values and/or set parameters we need.
       * Also before we start, if we want to provide battery info, we need to prepare battery service.
       * We can setup characteristics authorization
       */
      hid->startServices();
    }

    /*
     * Its good to setup advertising by providing appearance and advertised service. This will let clients find our device by type
     */
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(GENERIC_HID);
    pAdvertising->setMinInterval(400); //250ms minimum
    pAdvertising->setMaxInterval(800); //500ms maximum
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
    
    BLESecurity *pSecurity = new BLESecurity();
 
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    ESP_LOGI(LOG_TAG, "Advertising started!");
    while(1)
    {
      delay(1000000);
    }
  }
};


extern "C"
{
  esp_err_t HID_joystick_activatePairing(void)
  {
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(GENERIC_HID);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
    return ESP_OK;
  }
  
  esp_err_t HID_joystick_deactivatePairing(void)
  {
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(GENERIC_HID);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->stop();
    return ESP_OK;
  }

  uint8_t HID_joystick_isConnected(void)
  {
    return isConnected;
  }
  
  /** @brief Main init function to start HID interface (C interface)
   * 
   * @note After init, just use the queues! */
  esp_err_t HID_joystick_init(char *name)
  {
    //init FreeRTOS queues
    //initialise queues, even if they might not be used.
    joystick_q = xQueueCreate(32, sizeof(joystick_command_t));
    
    strncpy(btname, name, sizeof(btname) - 1);
    
    //init Neil Kolban's HOG task
    BLE_HOG* blehid = new BLE_HOG();
    blehid->setStackSize(16192);
    blehid->start();
    return ESP_OK;
  }
}
