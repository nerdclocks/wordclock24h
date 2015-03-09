/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * ws2812.c - WS2812 driver
 *
 * Copyright (c) 2014-2015 Frank Meyer - frank(at)fli4l.de
 *
 * uses Pin PC6 (alternatively PB5, PB0, or PB1)
 *
 * WS2812 timing : (1.25us = 800 kHz)
 *   0 => HI:0.35us , LO:0.90us
 *   1 => HI:0.90us , LO:0.35us
 *
 * WS23812 format : (8G 8R 8B)
 *   24bit per LED  (24 * 1.25 = 30us per LED)
 *    8bit per color (MSB first)
 *
 * After each frame of n LEDs there has to be a pause of >= 50us
 *
 * base frequency (TIM3) = 2 * APB1 (APB1 = 42MHz) => TIM_CLK = 84MHz
 * PWM frequency = TIM_CLK / (period + 1) / (prescaler + 1) = (84MHz / 105 / 1) = 0.8 MHz = 1.25 us
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */

#include "ws2812.h"
#include "ws2812-config.h"

/*-------------------------------------------------------------------------------------------------------------------------------------------
 * timer calculation:
 *
 *      freq = WS2812_TIM_CLK / (WS2812_TIM_PRESCALER + 1) / (WS2812_TIM_PERIOD + 1)
 * <==> freq = 84000000 / (0 + 1) / (104 + 1) = 800 kHz = 1.25 us
 *-------------------------------------------------------------------------------------------------------------------------------------------
 */
#if defined (STM32F401RE) || defined (STM32F411RE)              // STM32F401/STM32F411 Nucleo Board PC13
#define WS2812_TIM_CLK              84000000L                   // 84 MHz = 11.9ns
#define WS2812_TIM_PRESCALER        0
#define WS2812_TIM_PERIOD           104
#else
#error STM32 unknown
#endif

#define  WS2812_LO_TIME              29                         // 29 * 11.9ns = 0.345us
#define  WS2812_HI_TIME              76                         // 76 * 11.9ns = 0.905us

#define  WS2812_BIT_PER_LED          24                         // 3 * 8bit per LED, each bit costs 1.25us time
#define  WS2812_PAUSE_LEN            (2 * WS2812_BIT_PER_LED)   // pause, should be longer than 50us (2 * 24 * 1.25us = 60us)

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * Timer for data: TIM3
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#define  WS2812_TIM_CLOCK           RCC_APB1Periph_TIM3
#define  WS2812_TIM                 TIM3
#define  WS2812_TIM_AF              GPIO_AF_TIM3
#define  WS2812_TIM_CH1             1
#define  WS2812_TIM_CCR_REG1        TIM3->CCR1
#define  WS2812_TIM_DMA_TRG1        TIM_DMA_CC1

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * GPIO pins
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#define  WS2812_CH1_CLOCK           RCC_AHB1Periph_GPIOC
#define  WS2812_CH1_PORT            GPIOC
#define  WS2812_CH1_PIN             GPIO_Pin_6
#define  WS2812_CH1_SOURCE          GPIO_PinSource6

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 *  DMA TIM3 - DMA1, Channel5, Stream4
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#define  WS2812_DMA_CLOCK           RCC_AHB1Periph_DMA1
#define  WS2812_DMA_CH1_STREAM      DMA1_Stream4
#define  WS2812_DMA_CH1_CHANNEL     DMA_Channel_5

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * transfer complete interrupt - DMA1, Stream4
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#define  WS2812_DMA_CH1_IRQn        DMA1_Stream4_IRQn
#define  WS2812_DMA_CH1_ISR         DMA1_Stream4_IRQHandler
#define  WS2812_DMA_CH1_IRQ_FLAG    DMA_IT_TCIF4

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * DMA buffer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
#define  WS2812_TIMER_BUF_LEN       (WS2812_LEDS * WS2812_BIT_PER_LED + WS2812_PAUSE_LEN)   // DMA buffer length

static volatile uint32_t            ws2812_dma_status;                                      // DMA status
static WS2812_RGB                   rgb_buf[WS2812_LEDS];                                   // RGB values
static uint16_t		                timer_buf[WS2812_TIMER_BUF_LEN];                        // DMA buffer

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * initialize DMA
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_dma_init (void)
{
    DMA_InitTypeDef dma;

    DMA_Cmd(WS2812_DMA_CH1_STREAM, DISABLE);
    DMA_DeInit(WS2812_DMA_CH1_STREAM);
    dma.DMA_Channel             = WS2812_DMA_CH1_CHANNEL;
    dma.DMA_PeripheralBaseAddr  = (uint32_t) &WS2812_TIM_CCR_REG1;
    dma.DMA_Memory0BaseAddr     = (uint32_t)timer_buf;
    dma.DMA_DIR                 = DMA_DIR_MemoryToPeripheral;
    dma.DMA_BufferSize          = WS2812_TIMER_BUF_LEN;
    dma.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc           = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord; // 16bit
    dma.DMA_MemoryDataSize      = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_Mode                = DMA_Mode_Normal;
    dma.DMA_Priority            = DMA_Priority_VeryHigh;
    dma.DMA_FIFOMode            = DMA_FIFOMode_Disable;
    dma.DMA_FIFOThreshold       = DMA_FIFOThreshold_HalfFull;
    dma.DMA_MemoryBurst         = DMA_MemoryBurst_Single;
    dma.DMA_PeripheralBurst     = DMA_PeripheralBurst_Single;
    DMA_Init(WS2812_DMA_CH1_STREAM, &dma);
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * start DMA & timer (stopped when Transfer-Complete-Interrupt arrives)
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_dma_start (void)
{
    ws2812_dma_status = 1;                                      // set status to "busy"
    ws2812_dma_init();

    DMA_ITConfig(WS2812_DMA_CH1_STREAM, DMA_IT_TC, ENABLE);     // enable transfer complete interrupt
    DMA_Cmd(WS2812_DMA_CH1_STREAM, ENABLE);                     // DMA enable
    TIM_Cmd(WS2812_TIM, ENABLE);                                // Timer enable
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * clear all LEDs
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_clear_all (void)
{
    WS2812_RGB  rgb = { 0, 0, 0 };

    while (ws2812_dma_status != 0)
	{
		;
	}

    ws2812_dma_start ();
    ws2812_set_all_leds (&rgb, 1);
}


/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * setup timer buffer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_setup_timer_buf (void)
{
	uint_fast8_t    i;
	uint32_t		n;
	uint32_t		pos;
	WS2812_RGB *    led;

	pos = 0;
    led = rgb_buf;

	for (n = 0; n < WS2812_LEDS; n++)
	{
		for (i = 0x80; i != 0; i >>= 1)                         // color green
		{
			timer_buf[pos++] = (led->green & i) ? WS2812_HI_TIME : WS2812_LO_TIME;
		}

		for (i = 0x80; i != 0; i >>= 1)                         // color red
		{
			timer_buf[pos++] = (led->red & i) ? WS2812_HI_TIME : WS2812_LO_TIME;
		}

		for (i = 0x80; i != 0; i >>= 1)                         // color blue
		{
			timer_buf[pos++] = (led->blue & i) ? WS2812_HI_TIME : WS2812_LO_TIME;
		}

		led++;
	}

	for (n = 0; n < WS2812_PAUSE_LEN; n++)                      // Pause (2 * 24 * 1.25us = 60us)
	{
		timer_buf[pos++] = 0;
	}
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * init GPIO
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_init_io (void)
{
	GPIO_InitTypeDef gpio;

    // Clock Enable
    RCC_AHB1PeriphClockCmd(WS2812_CH1_CLOCK, ENABLE);

    gpio.GPIO_Pin     = WS2812_CH1_PIN;                         // set pin to digital output
    gpio.GPIO_Mode    = GPIO_Mode_AF;
    gpio.GPIO_Speed   = GPIO_Speed_100MHz;
#if 1
    gpio.GPIO_OType   = GPIO_OType_PP;                          // PushPull
    gpio.GPIO_PuPd    = GPIO_PuPd_NOPULL;
#else
    gpio.GPIO_OType   = GPIO_OType_OD;                          // Open Drain, needs extern PullUp
    gpio.GPIO_PuPd    = GPIO_PuPd_NOPULL;
#endif

    GPIO_Init(WS2812_CH1_PORT, &gpio);

    WS2812_CH1_PORT->BSRRH = WS2812_CH1_PIN;				    // set pin to Low

    GPIO_PinAFConfig(WS2812_CH1_PORT, WS2812_CH1_SOURCE, WS2812_TIM_AF);
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * init timer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_init_timer (void)
{
	TIM_TimeBaseInitTypeDef tb;
	TIM_OCInitTypeDef       toc;

	RCC_APB1PeriphClockCmd(WS2812_TIM_CLOCK, ENABLE); 			// clock enable (TIM)
	RCC_AHB1PeriphClockCmd(WS2812_DMA_CLOCK, ENABLE);			// clock Enable (DMA)

	// Timer init
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
    TIM_OC1PreloadConfig (WS2812_TIM, TIM_OCPreload_Enable);

	TIM_ARRPreloadConfig (WS2812_TIM, ENABLE);                  // timer enable
}


/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * init NVIC
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
static void
ws2812_init_nvic (void)
{
	NVIC_InitTypeDef nvic;

    TIM_DMACmd(WS2812_TIM, WS2812_TIM_DMA_TRG1, ENABLE);

    nvic.NVIC_IRQChannel                    = WS2812_DMA_CH1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority  = 0;
    nvic.NVIC_IRQChannelSubPriority         = 0;
    nvic.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&nvic);
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * ISR DMA (will be called, when all data has been transferred)
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
WS2812_DMA_CH1_ISR (void)
{
	if (DMA_GetITStatus(WS2812_DMA_CH1_STREAM, WS2812_DMA_CH1_IRQ_FLAG))			// check transfer complete interrupt flag
	{
		DMA_ClearITPendingBit(WS2812_DMA_CH1_STREAM, WS2812_DMA_CH1_IRQ_FLAG);		// reset flag
		TIM_Cmd(WS2812_TIM, DISABLE);												// disable timer
		DMA_Cmd(WS2812_DMA_CH1_STREAM, DISABLE);									// disable DMA
		ws2812_dma_status = 0;														// set status to ready
	}
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * init ws2812
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_init (void)
{
	uint32_t n;

	ws2812_dma_status	= 0;

	for (n = 0; n < WS2812_TIMER_BUF_LEN; n++)
	{
		timer_buf[n] = 0;
	}

    for (n = 0; n < WS2812_LEDS; n++)
	{
        rgb_buf[n].red      = 0;
        rgb_buf[n].green    = 0;
        rgb_buf[n].blue     = 0;
    }

	ws2812_init_io ();
	volatile long x;
	for (x = 0; x < 100; x++) { ; }
	ws2812_init_timer ();
	for (x = 0; x < 100; x++) { ; }
	ws2812_init_nvic ();
	for (x = 0; x < 100; x++) { ; }
	ws2812_dma_init ();
	for (x = 0; x < 100; x++) { ; }
	ws2812_clear_all ();
	for (x = 0; x < 100; x++) { ; }
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * refresh buffer
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_refresh (void)
{
	while (ws2812_dma_status != 0)
	{
		;													    // wait until DMA transfer is ready
	}

	ws2812_setup_timer_buf ();
    ws2812_dma_start();
}

/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * set one RGB value
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_set_led (uint_fast16_t n, WS2812_RGB * rgb, uint_fast8_t refresh)
{
	if (n < WS2812_LEDS)
	{
        rgb_buf[n].red      = rgb->red;
        rgb_buf[n].green    = rgb->green;
        rgb_buf[n].blue     = rgb->blue;

        if (refresh)
		{
			ws2812_refresh ();
		}
	}
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------
 * set all LEDs to RGB value
 *-----------------------------------------------------------------------------------------------------------------------------------------------
 */
void
ws2812_set_all_leds (WS2812_RGB * rgb, uint_fast8_t refresh)
{
	uint_fast16_t n;

	for (n = 0; n < WS2812_LEDS; n++)
	{
        rgb_buf[n].red      = rgb->red;
        rgb_buf[n].green    = rgb->green;
        rgb_buf[n].blue     = rgb->blue;
	}

	if (refresh)
	{
		ws2812_refresh ();
	}
}
