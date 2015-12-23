/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart.h - declaration of UART routines for STM32F4XX or STM32F10X
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#if defined (STM32F10X)
#  include "stm32f10x.h"
#  include "stm32f10x_gpio.h"
#  include "stm32f10x_usart.h"
#  include "stm32f10x_rcc.h"
#elif defined (STM32F4XX)
#  include "stm32f4xx.h"
#  include "stm32f4xx_gpio.h"
#  include "stm32f4xx_usart.h"
#  include "stm32f4xx_rcc.h"
#endif

#include "misc.h"

extern void             uart_init (uint32_t);
extern void             uart_putc (uint_fast8_t);
extern void             uart_puts (char *);
extern uint_fast8_t     uart_getc (void);
extern uint_fast8_t     uart_poll (uint_fast8_t *);
extern void             uart_flush ();
extern uint_fast16_t    uart_read (char *, uint_fast16_t);
extern uint_fast16_t    uart_write (char *, uint_fast16_t);
