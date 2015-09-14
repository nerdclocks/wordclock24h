/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * ws2812.c - WS2812 driver
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * Timings:
 *          WS2812          WS2812B         Tolerance       Common symmetric(!) values
 *   T0H    350 ns          400 ns          +/- 150 ns      470 ns
 *   T1H    700 ns          800 ns          +/- 150 ns      800 ns
 *   T0L    800 ns          850 ns          +/- 150 ns      800 ns
 *   T1L    600 ns          450 ns          +/- 150 ns      470 ns
 *
 * WS23812 format : (8G 8R 8B)
 *   24bit per LED  (24 * 1.25 = 30us per LED)
 *    8bit per color (MSB first)
 *
 * After each frame of n LEDs there has to be a pause of >= 50us
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */

#include "ws2812.h"
#include "ws2812-config.h"

#include "delay.h"

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * timer calculation:
 *
 *  freq = WS2812_TIM_CLK / (WS2812_TIM_PRESCALER + 1) / (WS2812_TIM_PERIOD + 1)
 *
 *  STM32F4XX:
 *    freq = 84000000 / (0 + 1) / (106 + 1) = 785 kHz = 1.274 us
 *
 *  STM32F10X:
 *    freq = 72000000 / (0 + 1) / ( 91 + 1) = 783 kHz = 1.277 us
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if defined (STM32F401RE) || defined (STM32F411RE)              // STM32F401/STM32F411 Nucleo Board
#define WS2812_TIM_CLK              84000000L                   // 84 MHz = 11.90ns
#define WS2812_TIM_PRESCALER          0
#define WS2812_TIM_PERIOD           106

#define WS2812_T1H                   67                         // 800ns: 67 * 11.90ns = 797ns
#define WS2812_T1L                   39                         // 470ns: 39 * 11.90ns = 464ns

#elif defined (STM32F103)                                       // STM32F103 Mini Development Board
#define WS2812_TIM_CLK              72000000L                   // 72 MHz = 13.89ns
#define WS2812_TIM_PRESCALER          0
#define WS2812_TIM_PERIOD            91

#define WS2812_T1H                   58                         // 800ns: 58 * 13.89ns = 806ns
#define WS2812_T1L                   34                         // 470ns: 34 * 13.89ns = 472ns

#else
#error STM32 unknown
#endif

#define  WS2812_BIT_PER_LED          24                         // 3 * 8bit per LED, each bit costs 1.25us time
#define  WS2812_PAUSE_LEN            (2 * WS2812_BIT_PER_LED)   // pause, should be longer than 50us (2 * 24 * 1.25us = 60us)

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * Timer for data: TIM3
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#if defined (STM32F4XX)
// Timer:
#  define WS2812_TIM_CLOCK_CMD          RCC_APB1PeriphClockCmd
#  define WS2812_TIM_CLOCK              RCC_APB1Periph_TIM3
#  define WS2812_TIM                    TIM3
#  define WS2812_TIM_AF                 GPIO_AF_TIM3
#  define WS2812_TIM_CCR_REG1           TIM3->CCR1
#  define WS2812_TIM_DMA_TRG1           TIM_DMA_CC1
// GPIO:
#  define WS2812_GPIO_CLOCK_CMD         RCC_AHB1PeriphClockCmd
#  define WS2812_GPIO_CLOCK             RCC_AHB1Periph_GPIOC
#  define WS2812_GPIO_PORT              GPIOC
#  define WS2812_GPIO_PIN               GPIO_Pin_6
#  define WS2812_GPIO_SOURCE            GPIO_PinSource6
// DMA TIM3 - DMA1, Channel5, Stream4
#  define WS2812_DMA_CLOCK_CMD          RCC_AHB1PeriphClockCmd
#  define WS2812_DMA_CLOCK              RCC_AHB1Periph_DMA1
#  define WS2812_DMA_STREAM             DMA1_Stream4
#  define WS2812_DMA_CHANNEL            DMA_Channel_5
// transfer complete interrupt - DMA1, Stream4
#  define WS2812_DMA_CHANNEL_IRQn       DMA1_Stream4_IRQn
#  define WS2812_DMA_CHANNEL_ISR        DMA1_Stream4_IRQHandler
#  define WS2812_DMA_CHANNEL_IRQ_FLAG   DMA_IT_TCIF4

#elif defined (STM32F10X)
// Timer:
#  define WS2812_TIM_CLOCK_CMD          RCC_APB2PeriphClockCmd
#  define WS2812_TIM_CLOCK              RCC_APB2Periph_TIM1
#  define WS2812_TIM                    TIM1
#  define WS2812_TIM_AF                 GPIO_AF_TIM1
#  define WS2812_TIM_CCR_REG1           TIM1->CCR1
#  define WS2812_TIM_DMA_TRG1           TIM_DMA_CC1
// GPIO:
#  define WS2812_GPIO_CLOCK_CMD         RCC_APB2PeriphClockCmd
#  define WS2812_GPIO_CLOCK             RCC_APB2Periph_GPIOA
#  define WS2812_GPIO_PORT              GPIOA
#  define WS2812_GPIO_PIN               GPIO_Pin_8
#  define WS2812_GPIO_SOURCE            GPIO_PinSource8
// DMA TIM1 - DMA1, Channel2
#  define WS2812_DMA_CLOCK_CMD          RCC_AHBPeriphClockCmd
#  define WS2812_DMA_CLOCK              RCC_AHBPeriph_DMA1
#  define WS2812_DMA_STREAM             DMA1_Channel2
// transfer complete interrupt - DMA1, Channel2
#  define WS2812_DMA_CHANNEL_IRQn       DMA1_Channel2_IRQn
#  define WS2812_DMA_CHANNEL_ISR        DMA1_Channel2_IRQHandler
#  define WS2812_DMA_CHANNEL_IRQ_FLAG   DMA1_IT_TC2

#endif

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * DMA buffer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#define  WS2812_TIMER_BUF_LEN       (WS2812_MAX_LEDS * WS2812_BIT_PER_LED + WS2812_PAUSE_LEN)   // DMA buffer length

static volatile uint32_t            ws2812_dma_status;                                          // DMA status
static WS2812_RGB                   rgb_buf[WS2812_MAX_LEDS];                                   // RGB values
static uint16_t                     timer_buf[WS2812_TIMER_BUF_LEN];                            // DMA buffer

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * INTERN: initialize DMA
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_dma_init (void)
{
    DMA_InitTypeDef         dma;
    DMA_StructInit (&dma);

    DMA_Cmd(WS2812_DMA_STREAM, DISABLE);
    DMA_DeInit(WS2812_DMA_STREAM);

    dma.DMA_Mode                = DMA_Mode_Normal;
    dma.DMA_PeripheralBaseAddr  = (uint32_t) &WS2812_TIM_CCR_REG1;
    dma.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;          // 16bit
    dma.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;
    dma.DMA_BufferSize          = WS2812_TIMER_BUF_LEN;
    dma.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc           = DMA_MemoryInc_Enable;
    dma.DMA_Priority            = DMA_Priority_VeryHigh;                    // DMA_Priority_High;

#if defined(STM32F4XX)

    dma.DMA_DIR                 = DMA_DIR_MemoryToPeripheral;
    dma.DMA_Channel             = WS2812_DMA_CHANNEL;
    dma.DMA_Memory0BaseAddr     = (uint32_t)timer_buf;
    dma.DMA_FIFOMode            = DMA_FIFOMode_Disable;
    dma.DMA_FIFOThreshold       = DMA_FIFOThreshold_HalfFull;
    dma.DMA_MemoryBurst         = DMA_MemoryBurst_Single;
    dma.DMA_PeripheralBurst     = DMA_PeripheralBurst_Single;

#elif defined (STM32F10X)

    dma.DMA_DIR                 = DMA_DIR_PeripheralDST;
    dma.DMA_M2M                 = DMA_M2M_Disable;
    dma.DMA_MemoryBaseAddr      = (uint32_t)timer_buf;

#endif

    DMA_Init(WS2812_DMA_STREAM, &dma);
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * start DMA & timer (stopped when Transfer-Complete-Interrupt arrives)
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_dma_start (void)
{
    ws2812_dma_status = 1;                                      // set status to "busy"

    ws2812_dma_init ();
    // fm: other method instead of ws2812_dma_init():
    // DMA_SetCurrDataCounter(DMA1_Channel1, WS2812_TIMER_BUF_LEN);       // set new buffer size

    DMA_ITConfig(WS2812_DMA_STREAM, DMA_IT_TC, ENABLE);         // enable transfer complete interrupt
    DMA_Cmd(WS2812_DMA_STREAM, ENABLE);                         // DMA enable
    TIM_Cmd(WS2812_TIM, ENABLE);                                // Timer enable
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * clear all LEDs
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_clear_all (uint_fast16_t n_leds)
{
    WS2812_RGB  rgb = { 0, 0, 0 };

    while (ws2812_dma_status != 0)
    {
        ;
    }

    ws2812_set_all_leds (&rgb, n_leds, 1);
}


/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * setup timer buffer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_setup_timer_buf (uint_fast16_t n_leds)
{
    uint_fast8_t    i;
    uint32_t        n;
    uint32_t        pos;
    WS2812_RGB *    led;

    pos = 0;
    led = rgb_buf;

    for (n = 0; n < n_leds; n++)
    {
        for (i = 0x80; i != 0; i >>= 1)                         // color green
        {
            timer_buf[pos++] = (led->green & i) ? WS2812_T1H : WS2812_T1L;
        }

        for (i = 0x80; i != 0; i >>= 1)                         // color red
        {
            timer_buf[pos++] = (led->red & i) ? WS2812_T1H : WS2812_T1L;
        }

        for (i = 0x80; i != 0; i >>= 1)                         // color blue
        {
            timer_buf[pos++] = (led->blue & i) ? WS2812_T1H : WS2812_T1L;
        }

        led++;
    }

    for (n = 0; n < WS2812_PAUSE_LEN; n++)                      // Pause (2 * 24 * 1.25us = 60us)
    {
        timer_buf[pos++] = 0;
    }
}


/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * ISR DMA (will be called, when all data has been transferred)
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
WS2812_DMA_CHANNEL_ISR (void)
{
#if defined (STM32F4XX)
    if (DMA_GetITStatus(WS2812_DMA_STREAM, WS2812_DMA_CHANNEL_IRQ_FLAG))            // check transfer complete interrupt flag
    {
        DMA_ClearITPendingBit (WS2812_DMA_STREAM, WS2812_DMA_CHANNEL_IRQ_FLAG);     // reset flag
#elif defined (STM32F10X)
    if (DMA_GetITStatus(WS2812_DMA_CHANNEL_IRQ_FLAG))                               // check transfer complete interrupt flag
    {
        DMA_ClearITPendingBit (WS2812_DMA_CHANNEL_IRQ_FLAG);
#endif
        TIM_Cmd (WS2812_TIM, DISABLE);                                              // disable timer
        DMA_Cmd (WS2812_DMA_STREAM, DISABLE);                                       // disable DMA
        ws2812_dma_status = 0;                                                      // set status to ready
    }
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * refresh buffer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_refresh (uint_fast16_t n_leds)
{
    while (ws2812_dma_status != 0)
    {
        ;                                                        // wait until DMA transfer is ready
    }

    ws2812_setup_timer_buf (n_leds);
    ws2812_dma_start();
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * set one RGB value
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_set_led (uint_fast16_t n, WS2812_RGB * rgb)
{
    if (n < WS2812_MAX_LEDS)
    {
        rgb_buf[n].red      = rgb->red;
        rgb_buf[n].green    = rgb->green;
        rgb_buf[n].blue     = rgb->blue;
    }
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * set all LEDs to RGB value
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_set_all_leds (WS2812_RGB * rgb, uint_fast16_t n_leds, uint_fast8_t refresh)
{
    uint_fast16_t n;

    for (n = 0; n < n_leds; n++)
    {
        rgb_buf[n].red      = rgb->red;
        rgb_buf[n].green    = rgb->green;
        rgb_buf[n].blue     = rgb->blue;
    }

    if (refresh)
    {
        ws2812_refresh (n_leds);
    }
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * initialize WS2812
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_init (void)
{
    GPIO_InitTypeDef        gpio;
    TIM_TimeBaseInitTypeDef tb;
    TIM_OCInitTypeDef       toc;
    NVIC_InitTypeDef        nvic;

    ws2812_dma_status = 0;

    /*-------------------------------------------------------------------------------------------------------------------------------------------
     * initialize gpio
     *-------------------------------------------------------------------------------------------------------------------------------------------
     */
    GPIO_StructInit (&gpio);
    WS2812_GPIO_CLOCK_CMD (WS2812_GPIO_CLOCK, ENABLE);          // clock enable

    gpio.GPIO_Pin     = WS2812_GPIO_PIN;

#if defined (STM32F4XX)

    gpio.GPIO_Mode    = GPIO_Mode_AF;
    gpio.GPIO_OType   = GPIO_OType_PP;                          // GPIO_OType_PP: PushPull, GPIO_OType_OD: Open Drain, needs extern PullUp
    gpio.GPIO_PuPd    = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed   = GPIO_Speed_100MHz;
    GPIO_Init(WS2812_GPIO_PORT, &gpio);
    WS2812_GPIO_PORT->BSRRH = WS2812_GPIO_PIN;                  // set pin to Low
    GPIO_PinAFConfig(WS2812_GPIO_PORT, WS2812_GPIO_SOURCE, WS2812_TIM_AF);

#elif defined (STM32F10X)

    gpio.GPIO_Mode    = GPIO_Mode_AF_PP;                        // GPIO_Mode_AF_PP: PushPull, GPIO_Mode_AF_OD: Open Drain, needs extern PullUp
    gpio.GPIO_Speed   = GPIO_Speed_50MHz;
    GPIO_Init(WS2812_GPIO_PORT, &gpio);
    GPIO_WriteBit(WS2812_GPIO_PORT, WS2812_GPIO_PIN, RESET);    // set pin to Low

#endif

    /*-------------------------------------------------------------------------------------------------------------------------------------------
     * initialize TIMER
     *-------------------------------------------------------------------------------------------------------------------------------------------
     */
    TIM_TimeBaseStructInit (&tb);
    TIM_OCStructInit (&toc);

    WS2812_TIM_CLOCK_CMD (WS2812_TIM_CLOCK, ENABLE);            // clock enable (TIM)
    WS2812_DMA_CLOCK_CMD (WS2812_DMA_CLOCK, ENABLE);            // clock Enable (DMA)

    tb.TIM_Period           = WS2812_TIM_PERIOD;
    tb.TIM_Prescaler        = WS2812_TIM_PRESCALER;
    tb.TIM_ClockDivision    = TIM_CKD_DIV1;
    tb.TIM_CounterMode      = TIM_CounterMode_Up;
    TIM_TimeBaseInit (WS2812_TIM, &tb);

    toc.TIM_OCMode          = TIM_OCMode_PWM1;
    toc.TIM_OutputState     = TIM_OutputState_Enable;
    toc.TIM_Pulse           = 0;
    toc.TIM_OCPolarity      = TIM_OCPolarity_High;

    TIM_OC1Init(WS2812_TIM, &toc);
    TIM_OC1PreloadConfig (WS2812_TIM, TIM_OCPreload_Enable);    // fm: necessary on STM32F1xx?

    TIM_ARRPreloadConfig (WS2812_TIM, ENABLE);                  // timer enable, fm: necessary on STM32F1xx?

    TIM_CtrlPWMOutputs(WS2812_TIM, ENABLE);

    TIM_DMACmd (WS2812_TIM, WS2812_TIM_DMA_TRG1, ENABLE);
    /*-------------------------------------------------------------------------------------------------------------------------------------------
     * initialize NVIC
     *-------------------------------------------------------------------------------------------------------------------------------------------
     */
    nvic.NVIC_IRQChannel                    = WS2812_DMA_CHANNEL_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority  = 0;
    nvic.NVIC_IRQChannelSubPriority         = 0;
    nvic.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&nvic);

    /*-------------------------------------------------------------------------------------------------------------------------------------------
     * initialize DMA
     *-------------------------------------------------------------------------------------------------------------------------------------------
     */
    ws2812_dma_init ();

    ws2812_clear_all (WS2812_MAX_LEDS);
}
