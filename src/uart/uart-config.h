/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart-config.h - configuration file for uart lib
 *
 * Possible UARTs of STM32F401 / STM32F411:
 *           ALTERNATE=0    ALTERNATE=1    ALTERNATE=2
 *  +--------------------------------------------------+
 *  | UART | TX   | RX   || TX   | RX   || TX   | RX   |
 *  |======|======|======||======|======||======|======|
 *  | 1    | PA9  | PA10 || PB6  | PB7  ||      |      |
 *  | 2    | PA2  | PA3  || PD5  | PD6  ||      |      |
 *  | 6    | PC6  | PC7  || PA11 | PA12 ||      |      |
 *  +--------------------------------------------------+
 *
 * Possible UARTs of STM32F407:
 *           ALTERNATE=0    ALTERNATE=1    ALTERNATE=2
 *  +--------------------------------------------------+
 *  | UART | TX   | RX   || TX   | RX   || TX   | RX   |
 *  |======|======|======||======|======||======|======|
 *  | 1    | PA9  | PA10 || PB6  | PB7  ||      |      |
 *  | 2    | PA2  | PA3  || PD5  | PD6  ||      |      |
 *  | 3    | PB10 | PB11 || PC10 | PC11 || PD8  | PD9  |
 *  | 4    | PA0  | PA1  || PC10 | PC11 ||      |      |
 *  | 5    | PC12 | PD2  ||      |      ||      |      |
 *  | 6    | PC6  | PC7  || PG14 | PG9  ||      |      |
 *  +--------------------------------------------------+
 *
 * Possible UARTs of STM32F10x:
 *           ALTERNATE=0    ALTERNATE=1    ALTERNATE=2
 *  +--------------------------------------------------+
 *  | UART | TX   | RX   || TX   | RX   || TX   | RX   |
 *  |======|======|======||======|======||======|======|
 *  | 1    | PA9  | PA10 || PB6  | PB7  ||      |      |
 *  | 2    | PA2  | PA3  || PD5  | PD6  ||      |      |
 *  | 3    | PB10 | PB11 || PC10 | PC11 || PD8  | PD9  |
 *  +--------------------------------------------------+
 *
 * Copyright (c) 2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */

#if defined (STM32F4XX)                                 // STM32F401/STM32F411 Nucleo Board: we use USART2 connected to ST-Link (USB)

#  define UART_NUMBER       6                           // UART number on STM32F4xx (1-6 for UART)
#  define UART_ALTERNATE    1                           // ALTERNATE number

#elif defined (STM32F10X)                               // STM32F103C8T6 Mini Development Board: we use USART2 connected to ST-Link (USB)

#  define UART_NUMBER       3                           // UART number on STM32F1xx (1-3 for UART)
#  define UART_ALTERNATE    0                           // ALTERNATE number

#else
#  errror unknown STM32
#endif

#define UART_TXBUFLEN       128                         // 128 Bytes ringbuffer for UART TX
#define UART_RXBUFLEN       128                         // 128 Bytes ringbuffer for UART RX
