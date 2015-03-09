/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * onewire-config.h - onewire configuration
 *
 * Copyright (c) 2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef _ONEWIRE_CONFIG_H_
#define _ONEWIRE_CONFIG_H_

//-----------------------------------------------------------------------------------------------------------------------------------------------
// OneWire data pin:
//-----------------------------------------------------------------------------------------------------------------------------------------------
#define ONE_WIRE_PIN                GPIO_Pin_2
#define ONE_WIRE_PORT               GPIOD
#define ONE_WIRE_CLK                RCC_AHB1Periph_GPIOD

#endif
