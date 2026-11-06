// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
// Licensed under the Apache License, Version 2.0.

#ifndef __BOARD_ESP32C3_DEVKITC_H__
#define __BOARD_ESP32C3_DEVKITC_H__

#define LIGHT_BUTTON_GPIO 4
#define LIGHT_BUTTON_ACTIVE_LEVEL 0

#define LIGHT_DIMMER_BUTTON_GPIO 5
#define LIGHT_DIMMER_BUTTON_ACTIVE_LEVEL 0

/* WCMCU-2812B-8: 8 addressable WS2812B-compatible RGB LEDs */
#define LIGHT_STRIP_GPIO 6      /* ESP32-C3 GPIO connected to DI */
#define LIGHT_STRIP_LED_COUNT 8 /* WCMCU-2812B-8 has 8 LEDs */

#define LIGHT_FADE_PERIOD_MS 100
#define LIGHT_BLINK_PERIOD_MS 1500

#endif /* __BOARD_ESP32C3_DEVKITC_H__ */
