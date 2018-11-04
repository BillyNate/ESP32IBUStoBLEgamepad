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

/** @file
 * @brief This file is a C compatible wrapper for Neil Kolbans CPP utils
 * 
 * It initializes 3 queues for sending mouse, keyboard and joystick data
 * from within the C-side. C++ classes are instantiated from here.
 * If you want to have a different BLE HID device, you need to adapt this file.
 * 
 * @note Thank you very much Neil Kolban for this impressive work!
*/

#ifndef _HID_joystick_H_
#define _HID_joystick_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <esp_log.h>

/** @brief Queue for sending joystick reports
 * @see joystick_command_t */
extern QueueHandle_t joystick_q;

/** @brief Main init function to start HID interface
 * 
 * @param enableJoystick If != 0, joystick will be active
 * to the queue. If set != 0, keyboard/mouse/joystick will send test data.
 * @note After init, just use the queues! */
esp_err_t HID_joystick_init(char * name);

/** @brief Activate pairing, disconnect from paired device
 * */
esp_err_t HID_joystick_activatePairing(void);

/** @brief Deactivate pairing, disconnect from paired device
 * */
esp_err_t HID_joystick_deactivatePairing(void);

/** @brief Is the BLE currently connected?
 * @return 0 if not connected, 1 if connected */  
uint8_t HID_joystick_isConnected(void);

/** @brief One command (report) to be issued via BLE joystick profile
 * @see joystick_q */
typedef struct joystick_command {
  /** @brief Button mask, allows 8 different buttons */
  uint8_t buttons;
  /** @brief X-axis value, 1000 - 2000 */
  uint16_t Xaxis;
  /** @brief Y-axis value, 1000 - 2000 */
  uint16_t Yaxis;
  /** @brief X-rotate value, 1000 - 2000 */
  uint16_t Xrotate;
  /** @brief Y-rotate value, 1000 - 2000 */
  uint16_t Yrotate;
} joystick_command_t;

/** @brief Type of keycode action.
 * 
 * * Use press to add the keycode/modifier to the report.
 * * Use release to remove the keycode/modifier from the report.
 * * Use press_release to send 2 reports: one with the added keycode, one without it.
 * */
typedef enum {
  PRESS,
  RELEASE,
  PRESS_RELEASE,
  RELEASE_ALL
} keyboard_action;

#ifdef __cplusplus
}
#endif

#endif /* _HID_joystick_H_ */
