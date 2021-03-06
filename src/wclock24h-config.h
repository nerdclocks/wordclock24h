/*-------------------------------------------------------------------------------------------------------------------------------------------
 * wclock24h-config.h - configuration constants of wclock24h project
 *
 * Copyright (c) 2014-2016 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#ifndef WCLOCK24H_CONFIG_H
#define WCLOCK24H_CONFIG_H

#if defined (STM32F103) && defined(DEBUG)               // STM32F103 has not enough RAM in DEBUG
#  define SAVE_RAM 1
#else
#  define SAVE_RAM 0
#endif

// not used yet, see project settings for WCLOCK24H=1 (WCLOCK24H) or WCLOCK24H=0 (WCLOCK12H)

#endif
