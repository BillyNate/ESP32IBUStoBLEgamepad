#ifndef _CONFIG_H_
#define _CONFIG_H_

#define MODULE_ID                 "ESP32miniBT_v0.1"
#define GATTS_TAG                 "IBUS2BLE"
#define MAX_BT_DEVICENAME_LENGTH  40

// serial port for connection to IBus receiver
#define IBUS_SERIAL_RXPIN         (GPIO_NUM_16)

// indicator LED
#define INDICATOR_LED_PIN         (GPIO_NUM_0)

typedef struct config_data
{
  char bt_device_name[MAX_BT_DEVICENAME_LENGTH];
} config_data_t;

#endif
