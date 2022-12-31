//  Miscellaneous Utility Subroutines.
//  ----------------------------------

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/timer.h>

#include "license.h"

#include "comm.h"
#include "gpiodef.h"
#include "globals.h"
#include "miscsubs.h"

static void SetupGPIO( uint32_t Gpio, uint16_t Pin, 
                      uint8_t Mode, uint8_t Pull, int Initial);


//  InitGPIO - Initialize GPIOs.
//  ----------------------------
//
//    Handles GPIO initialization -- see pin definitions in gpiodef.h
//

void InitGPIO( void)
{

//  Enable all GPIO clocks.

  rcc_periph_clock_enable( RCC_GPIOA);
  rcc_periph_clock_enable( RCC_GPIOB);
  rcc_periph_clock_enable( RCC_GPIOC);
  rcc_periph_clock_enable( RCC_GPIOD);
  rcc_periph_clock_enable( RCC_GPIOE);
  rcc_periph_clock_enable( RCC_SYSCFG);
  rcc_periph_clock_enable( RCC_TIM2);
  rcc_periph_clock_enable( RCC_TIM1);

//  The initialization list.  

  GPIO_INIT( LED);		// two LEDs
  GPIO_INIT( USART);		// USART Rx, Tx
  GPIO_INIT( SDIO_DATA);	// SDIO data
  GPIO_INIT( SDIO_SCK);		// SDIO clock
  GPIO_INIT( SDIO_CMD);		// SDIO command

//  Pertec initialization.

  GPIO_INIT( PDATA);		// Pertec data register
  GPIO_INIT( PCTRL);		// Control register
  GPIO_INIT( PCMD);		// Command registers
  GPIO_INIT( PSTAT);		// Status registers

  return;
} // InitGPIO

//  SetupGPIO - Routine called by GPIO_INIT Macro.
//  ----------------------------------------------
//
//    

static void SetupGPIO( uint32_t Gpio, uint16_t Pin, 
                      uint8_t Mode, uint8_t Pull, int Initial)
{

  gpio_mode_setup( Gpio, Mode, Pull, Pin);
  if ( Initial == 0)
    gpio_clear( Gpio, Pin);
  else if ( Initial == 1)    
    gpio_set( Gpio, Pin);
  return;
} // SetupGPIO
 

//  ShowBuffer - Display buffer contents.
//  -------------------------------------
//
//

void ShowBuffer( uint8_t *Buf, int Buflen)
{

  int
    base,
    i, j;

  for ( i = 0; i < Buflen/16; i++)
  {
    base = 16*i;
    Uprintf( "%04x: ", base);
    for ( j = 0; j < 16; j++)
      Uprintf( "%02x ", Buf[j+base]);
    Uprintf("  ");
    for ( j = 0; j < 16; j++)
      Uprintf( "%c", isprint( Buf[j+base]) ? Buf[j+base] : '.');
    Uprintf( "\n");    
  } // for each line

} // ShowBuffer

//  Delay Setup.
//  ------------
//
//	Uses TIM6to provide variable delays.
//

void DelaySetup (void)
{

// Set up a microsecond free running timer 

  rcc_periph_clock_enable(RCC_TIM6);
  timer_set_prescaler(TIM6, rcc_apb1_frequency / 4000000 - 1);
  timer_set_period(TIM6, 0xffff);
  timer_one_shot_mode(TIM6);
} // DelaySetup

//  Delay for a specific number of half-microseconds.
//  -------------------------------------------------
//

void Delay(uint16_t Howmuch)
{

  TIM_ARR(TIM6) = Howmuch;
  TIM_EGR(TIM6) = TIM_EGR_UG;
  TIM_CR1(TIM6) |= TIM_CR1_CEN;	  //timer_enable_counter(TIM6)
  while (TIM_CR1(TIM6) & TIM_CR1_CEN); // stall
} // Delay


//  Sys_tick_handler - called every millisecond.
//  --------------------------------------------
//
//

void sys_tick_handler( void)
{
  Milliseconds++;

  if ( !(Milliseconds & 0x3ff))
  {  // Every 1024 milliseconds, blink LED
    G_TOGGLE( LED1);    // LED1 on/off 
  }
} // sys_tick_handler

//	SetupSysTick - Configure System Timer.
//	---------------------------------------
//
//	We interrupt every 1 msec.
//
  
void SetupSysTick( void)
{

  systick_set_reload(168000);
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_interrupt_enable();
  Milliseconds = 0;	            // clear tick counter
  systick_counter_enable();
} // SetupSysTick
