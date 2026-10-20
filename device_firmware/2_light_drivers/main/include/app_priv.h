// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// #ifndef __APP_PRIVATE_H__
// #define __APP_PRIVATE_H__

// /**
//  * @brief
//  *
//  */
// void app_driver_init(void);

// /**
//  * @brief
//  *
//  * @param state
//  * @return int
//  */
// int app_driver_set_state(bool state);

// /**
//  * @brief
//  *
//  * @return true
//  * @return false
//  */
// bool app_driver_get_state(void);

// #endif /**< __APP_PRIVATE_H__ */

#ifndef __APP_PRIVATE_H__
#define __APP_PRIVATE_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Application driver initialization
 */
void app_driver_init(void);

/**
 * @brief Set on/off state
 */
int app_driver_set_state(bool state);

/**
 * @brief Get on/off state
 */
bool app_driver_get_state(void);

/**
 * @brief Set color using HSV
 */
int app_driver_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value);

/**
 * @brief Get current HSV color
 */
void app_driver_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value);

/**
 * @brief Set brightness (0-100)
 */
int app_driver_set_brightness(uint8_t brightness);

/**
 * @brief Get current brightness
 */
uint8_t app_driver_get_brightness(void);

/**
 * @brief Set color temperature (0-100)
 */
int app_driver_set_color_temperature(uint8_t color_temperature);

/**
 * @brief Get current color temperature
 */
uint8_t app_driver_get_color_temperature(void);

#endif /**< __APP_PRIVATE_H__ */