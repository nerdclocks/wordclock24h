/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * mcurses-config.h - configuration file for mcurses lib
 *
 * Copyright (c) 2011-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */

#if defined (STM32F401RE) || defined (STM32F411RE)      // STM32F401/STM32F411 Nucleo Board: we use USART2 connected to ST-Link (USB)
#  define MCURSES_UART_NUMBER       2                   // UART number on STM32Fxx (1-6 for UART, 0 for USB), ignored on other µCs
#  define MCURSES_BAUD              115200              // UART baudrate
#elif defined (STM32F103)                               // STM32F103C8T6 Mini Development Board: we use USART2
#  define MCURSES_UART_NUMBER       2                   // UART number on STM32Fxx (1-6 for UART, 0 for USB), ignored on other µCs
#  define MCURSES_BAUD              115200              // UART baudrate
#else
#  errror unknown STM32
#endif

#define MCURSES_HALFDELAY            0                  // set to 1 if you need halfdelay() - costs one timer

#define MCURSES_LINES               24                  // 24 lines
#define MCURSES_COLS                80                  // 80 columns
