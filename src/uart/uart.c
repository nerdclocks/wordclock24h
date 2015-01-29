/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart.c - UART routines for STM32F4 Discovery and STM32F401 Nucleo
 *
 * Ports/Pins:
 *  STM32F4 Discovery: USART2, TX=PA2,  RX=PA3
 *  STM32Fx1 Nucleo:   USART6, TX=PA11, RX=PA12
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include "uart.h"

#define _CONCAT(a,b)            a##b
#define CONCAT(a,b)             _CONCAT(a,b)

#define UART_TXBUFLEN          64                               // 64 Bytes ringbuffer for UART
#define UART_RXBUFLEN          64                               // 64 Bytes ringbuffer for UART

#if defined (STM32F407VG)                                       // STM32F4 Discovery Board TX2 = PA2, RX2 = PA3

#define UART_TX_PORT_LETTER     A
#define UART_TX_PIN_NUMBER      2
#define UART_RX_PORT_LETTER     A
#define UART_RX_PIN_NUMBER      3
#define UART_AHB_CLOCK_CMD(c,e) RCC_AHB1PeriphClockCmd(c,e)
#define UART_APB_CLOCK_CMD(c,e) RCC_APB1PeriphClockCmd(c,e)

#define UART_NAME               USART2
#define UART_APB_CLOCK_USART    RCC_APB1Periph_USART2
#define UART_GPIO_AF_UART       GPIO_AF_USART2
#define UART_IRQ_HANDLER        USART2_IRQHandler
#define UART_IRQ_CHANNEL        USART2_IRQn

#elif defined (STM32F401RE) || defined (STM32F411RE)            // STM32F401 // STM32F411 Nucleo Board TX6 = PA11, RX6 = PA12

#define UART_TX_PORT_LETTER     A
#define UART_TX_PIN_NUMBER      11
#define UART_RX_PORT_LETTER     A
#define UART_RX_PIN_NUMBER      12
#define UART_AHB_CLOCK_CMD(c,e) RCC_AHB1PeriphClockCmd(c,e)
#define UART_APB_CLOCK_CMD(c,e) RCC_APB2PeriphClockCmd(c,e)

#define UART_NAME               USART6
#define UART_APB_CLOCK_USART    RCC_APB2Periph_USART6
#define UART_GPIO_AF_UART       GPIO_AF_USART6
#define UART_IRQ_HANDLER        USART6_IRQHandler
#define UART_IRQ_CHANNEL        USART6_IRQn

#else
#error unknown STM32
#endif

#define UART_TX_PORT            CONCAT(GPIO, UART_TX_PORT_LETTER)
#define UART_TX_AHB_CLOCK_PORT  CONCAT(RCC_AHB1Periph_GPIO, UART_TX_PORT_LETTER)
#define UART_TX_PIN             CONCAT(GPIO_Pin_, UART_TX_PIN_NUMBER)
#define UART_TX_PINSOURCE       CONCAT(GPIO_PinSource,  UART_TX_PIN_NUMBER)
#define UART_RX_PORT            CONCAT(GPIO, UART_RX_PORT_LETTER)
#define UART_RX_AHB_CLOCK_PORT  CONCAT(RCC_AHB1Periph_GPIO, UART_RX_PORT_LETTER)
#define UART_RX_PIN             CONCAT(GPIO_Pin_, UART_RX_PIN_NUMBER)
#define UART_RX_PINSOURCE       CONCAT(GPIO_PinSource, UART_RX_PIN_NUMBER)

static volatile uint8_t         uart_txbuf[UART_TXBUFLEN];      // tx ringbuffer
static volatile uint_fast8_t    uart_txsize = 0;                // tx size
static volatile uint8_t         uart_rxbuf[UART_RXBUFLEN];      // rx ringbuffer
static volatile uint_fast8_t    uart_rxsize = 0;                // rx size


/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_init()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
uart_init (uint32_t baudrate)
{
    GPIO_InitTypeDef    gpio;
    USART_InitTypeDef   uart;
    NVIC_InitTypeDef    nvic;

    UART_AHB_CLOCK_CMD (UART_TX_AHB_CLOCK_PORT, ENABLE);
    UART_AHB_CLOCK_CMD (UART_RX_AHB_CLOCK_PORT, ENABLE);

    UART_APB_CLOCK_CMD (UART_APB_CLOCK_USART, ENABLE);

    // connect UART functions with IO-Pins
    GPIO_PinAFConfig (UART_TX_PORT, UART_TX_PINSOURCE, UART_GPIO_AF_UART);      // TX
    GPIO_PinAFConfig (UART_RX_PORT, UART_RX_PINSOURCE, UART_GPIO_AF_UART);      // RX

    // UART as alternate function with PushPull
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;                                             // fm: perhaps better: GPIO_PuPd_NOPULL

    gpio.GPIO_Pin = UART_TX_PIN;
    GPIO_Init(UART_TX_PORT, &gpio);

    gpio.GPIO_Pin = UART_RX_PIN;
    GPIO_Init(UART_RX_PORT, &gpio);

    USART_OverSampling8Cmd(UART_NAME, ENABLE);

    // 8 bit no parity, no RTS/CTS
    uart.USART_BaudRate             = baudrate;
    uart.USART_WordLength           = USART_WordLength_8b;
    uart.USART_StopBits             = USART_StopBits_1;
    uart.USART_Parity               = USART_Parity_No;
    uart.USART_HardwareFlowControl  = USART_HardwareFlowControl_None;
    uart.USART_Mode                 = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(UART_NAME, &uart);

    // UART enable
    USART_Cmd(UART_NAME, ENABLE);

    // RX-Interrupt enable
    USART_ITConfig(UART_NAME, USART_IT_RXNE, ENABLE);

    // enable UART Interrupt-Vector
    nvic.NVIC_IRQChannel                    = UART_IRQ_CHANNEL;
    nvic.NVIC_IRQChannelPreemptionPriority  = 0;
    nvic.NVIC_IRQChannelSubPriority         = 0;
    nvic.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init (&nvic);
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_putc()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
uart_putc (uint8_t ch)
{
    static uint_fast8_t    uart_txstop  = 0;                    // tail

    while (uart_txsize >= UART_TXBUFLEN)                        // buffer full?
    {                                                           // yes
        ;                                                       // wait
    }

    uart_txbuf[uart_txstop++] = ch;                             // store character

    if (uart_txstop >= UART_TXBUFLEN)                           // at end of ringbuffer?
    {                                                           // yes
        uart_txstop = 0;                                        // reset to beginning
    }

    __disable_irq();
    uart_txsize++;                                              // increment used size
    __enable_irq();

    USART_ITConfig(UART_NAME, USART_IT_TXE, ENABLE);            // enable TXE interrupt
}

#if 0
void
uart_putc (uint8_t ch)
{
    while (USART_GetFlagStatus(UART_NAME, USART_FLAG_TXE) == RESET)
    {
         ;
    }
    USART_SendData(UART_NAME, ch);
}
#endif

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_getc()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
uint8_t
uart_getc (void)
{
    static uint_fast8_t uart_rxstart = 0;                       // head
    uint8_t             ch;

    while (uart_rxsize == 0)                                    // rx buffer empty?
    {                                                           // yes, wait
        ;
    }

    ch = uart_rxbuf[uart_rxstart++];                            // get character from ringbuffer

    if (uart_rxstart == UART_RXBUFLEN)                          // at end of rx buffer?
    {                                                           // yes
        uart_rxstart = 0;                                       // reset to beginning
    }

    __disable_irq();
    uart_rxsize--;                                              // decrement size
    __enable_irq();

    return ch;
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_poll()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
uint8_t
uart_poll (uint8_t * chp)
{
    static uint_fast8_t uart_rxstart = 0;                       // head
    uint8_t             ch;

    if (uart_rxsize == 0)                                       // rx buffer empty?
    {                                                           // yes, return 0
        return 0;
    }

    ch = uart_rxbuf[uart_rxstart++];                            // get character from ringbuffer

    if (uart_rxstart == UART_RXBUFLEN)                          // at end of rx buffer?
    {                                                           // yes
        uart_rxstart = 0;                                       // reset to beginning
    }

    __disable_irq();
    uart_rxsize--;                                              // decrement size
    __enable_irq();

    *chp = ch;
    return 1;
}

#if 0
uint8_t
uart_getc (void)
{
    uint8_t ch;

    while (USART_GetFlagStatus(UART_NAME, USART_FLAG_RXNE) == RESET)
    {                                                           // wait until character available
        ;
    }
    ch = USART_ReceiveData(USART1);
}
#endif


/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_flush()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
uart_flush ()
{
    while (uart_txsize > 0)                                    // tx buffer empty?
    {
        ;                                                       // no, wait
    }
}

#if 0
/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_read()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast16_t
uart_read (char * p, uint_fast16_t size)
{
    uint_fast16_t i;

    for (i = 0; i < size; i++)
    {
        *p++ = uart_getc ();
    }
    return size;
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * uart_write()
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
uint_fast16_t
uart_write (char * p, uint_fast16_t size)
{
    uint_fast16_t i;

    for (i = 0; i < size; i++)
    {
        uart_putc (*p++);
    }

    return size;
}
#endif

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * UART_IRQ_HANDLER
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void UART_IRQ_HANDLER (void)
{
    static uint_fast16_t    uart_rxstop  = 0;                   // tail
    uint16_t        value;
    uint8_t         ch;

    if (USART_GetITStatus (UART_NAME, USART_IT_RXNE) != RESET)
    {
        USART_ClearITPendingBit (UART_NAME, USART_IT_RXNE);
        value = USART_ReceiveData (UART_NAME);

        ch = value & 0xFF;

        if (uart_rxsize < UART_RXBUFLEN)                        // buffer full?
        {                                                       // no
            uart_rxbuf[uart_rxstop++] = ch;                     // store character

            if (uart_rxstop >= UART_RXBUFLEN)                   // at end of ringbuffer?
            {                                                   // yes
                uart_rxstop = 0;                                // reset to beginning
            }

            uart_rxsize++;                                      // increment used size
        }
    }

    if (USART_GetITStatus (UART_NAME, USART_IT_TXE) != RESET)
    {
        static uint8_t  uart_txstart = 0;                       // head
        uint8_t         ch;

        USART_ClearITPendingBit (UART_NAME, USART_IT_TXE);

        if (uart_txsize > 0)                                    // tx buffer empty?
        {                                                       // no
            ch = uart_txbuf[uart_txstart++];                    // get character to send, increment offset

            if (uart_txstart == UART_TXBUFLEN)                  // at end of tx buffer?
            {                                                   // yes
                uart_txstart = 0;                               // reset to beginning
            }

            uart_txsize--;                                      // decrement size

            USART_SendData(UART_NAME, ch);
        }
        else
        {
            USART_ITConfig(UART_NAME, USART_IT_TXE, DISABLE);   // disable TXE interrupt
        }
    }
}
