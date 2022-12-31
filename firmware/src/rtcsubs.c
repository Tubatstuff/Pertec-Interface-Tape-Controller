#include <stdint.h>
#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/pwr.h>

#include "license.h"

#include "comm.h"
#include "rtcsubs.h"

//*	Routines for accessing the STM32F4 real-time clock.
//	---------------------------------------------------
//
//	Used for dating files and logging.
//

static struct _rtc_date_time_struct
{
  uint16_t Day;          // Day of month
  uint16_t Month;        // Month of year
  uint16_t Year;         // Year
  uint16_t Seconds;      // Seconds in minute
  uint16_t Minutes;      // Minutes in hour
  uint16_t Hours;        // Hours in day
  uint16_t PM;           // nonzero if PM
} RTCDateTime;

#define RTC_INITIALIZED 0x32f2		// flag in backup reg 0

static void GetRTCDateTime( void);	// read time and date into structure
static void SetRTCDateTime( int Which);	// write time and date to RTC

#define SET_DATE 1			// bit to set date
#define SET_TIME 2			// bit to set time


//* Initialize Real Time Clock.
//  ----------------------------
//
//  Enables the RTC, or initializes it if needed.
//

void InitializeRTC( void)
{    

  rcc_periph_clock_enable( RCC_PWR); // Enable the PWR clock
  rcc_periph_clock_enable( RCC_RTC); // Enable the RTC clock

  if (RTC_BKPXR(0) != RTC_INITIALIZED)
  { // if the RTC hasn't been configured
    ConfigRTC();       // configure the RTC
    Uprintf( "\nTime and date not set.\n");
    SetRTCTime();      // set the time and date
  }
  else 
  { // RTC configured
    pwr_disable_backup_domain_write_protect();  // enable access to RTC
    rtc_wait_for_synchro();         // wait for APB to synchronize
    rtc_clear_wakeup_flag();        // clear alarm
    rtc_disable_wakeup_timer_interrupt(); // clear interupt
    pwr_enable_backup_domain_write_protect();	
  } // if RTC configured
  return;   
} // InitializeRTC

//*  Configure the RTC.
//   ------------------
//

void ConfigRTC( void)
{

  uint32_t 
   uwAsynchPrediv,
   uwSynchPrediv;

  rcc_periph_clock_enable( RCC_PWR); // Enable the PWR clock
  pwr_disable_backup_domain_write_protect();  // enable access to PWR regs
  rcc_osc_on( RCC_LSE);
  rcc_wait_for_osc_ready(RCC_LSE);    // enable and wait for LSE oscillator

//  Select LSE for the RTC oscillator

  RCC_BDCR = (RCC_BDCR & ~(RCC_BDCR_RTCSEL_MASK << RCC_BDCR_RTCSEL_SHIFT)) |
             (RCC_BDCR_RTCSEL_LSE << RCC_BDCR_RTCSEL_SHIFT);
  RCC_BDCR |= RCC_BDCR_RTCEN;           // enable the RTC running
  uwSynchPrediv = 0xFF;
  uwAsynchPrediv = 0x7F;      // synchronize to 32.768 KHz
  rtc_unlock();               // unlock RTC
  RTC_ISR |= RTC_ISR_INIT;
  while ((RTC_ISR & RTC_ISR_INITF) == 0);   // enter init mode[Ma1!
  
  rtc_set_prescaler( uwSynchPrediv, uwAsynchPrediv);    // set prescale and divide
  RTC_CR &= ~RTC_CR_FMT;      // clear 12 hour format
  RTC_ISR &= ~(RTC_ISR_INIT); // exit init mode     
  rtc_wait_for_synchro();         // wait for APB to synchronize
  pwr_enable_backup_domain_write_protect();	//  disable PWR write register
  return;  
} // ConfigRTC

//  GetRTCDateTime - Get RTC Time and Date.
//  ---------------------------------------
//
//    Local routine - gets time and date into
//    a structure.
//

static void GetRTCDateTime( void)
{

  uint32_t td;
  
  td = RTC_TR;        // get whole time register

//  Dig out the time.

  RTCDateTime.Hours =   (((td >> RTC_TR_HT_SHIFT) & RTC_TR_HT_MASK) * 10) +
                        ((td >> RTC_TR_HU_SHIFT) & RTC_TR_HU_MASK);
  RTCDateTime.Minutes = (((td >> RTC_TR_MNT_SHIFT) & RTC_TR_MNT_MASK) * 10) +
                        ((td >> RTC_TR_MNU_SHIFT) & RTC_TR_MNU_MASK);
  RTCDateTime.Seconds = (((td >> RTC_TR_ST_SHIFT) & RTC_TR_ST_MASK) * 10) +
                        ((td >> RTC_TR_SU_SHIFT) & RTC_TR_SU_MASK);
  RTCDateTime.PM = (td & RTC_TR_PM) != 0;   // PM flag

  td = RTC_DR;        // get whole date register 
  RTCDateTime.Year  =   (((td >> RTC_DR_YT_SHIFT) & RTC_DR_YT_MASK) * 10) +
                        ((td >> RTC_DR_YU_SHIFT) & RTC_DR_YU_MASK);
  RTCDateTime.Month   = (((td >> RTC_DR_MT_SHIFT) & 1) * 10) +
                        ((td >> RTC_DR_MU_SHIFT) & RTC_DR_MU_MASK);
  RTCDateTime.Day =     (((td >> RTC_DR_DT_SHIFT) & RTC_DR_DT_MASK) * 10) +
                        ((td >> RTC_DR_DU_SHIFT) & RTC_DR_DU_MASK);

  return;
} // GetRTCDateTime

//  SetRTCDateTime - Local routine to set the RTC date and time registers
//  ----------------------------------------------------------------------
//
//  Local routine; takes input in RTCDateTime struct.
//

static void SetRTCDateTime( int Which)
{

  uint32_t
    timeval, dateval;
 
  timeval = 
    ((uint32_t)(RTCDateTime.Hours / 10) << RTC_TR_HT_SHIFT) |
    ((uint32_t)(RTCDateTime.Hours % 10) << RTC_TR_HU_SHIFT) |
    ((uint32_t)(RTCDateTime.Minutes / 10) << RTC_TR_MNT_SHIFT) |
    ((uint32_t)(RTCDateTime.Minutes % 10) << RTC_TR_MNU_SHIFT) |
    ((uint32_t)(RTCDateTime.Seconds / 10) << RTC_TR_ST_SHIFT) |
    ((uint32_t)(RTCDateTime.Seconds % 10) << RTC_TR_SU_SHIFT);      // set time

  dateval = 
    ((uint32_t)(RTCDateTime.Year / 10) << RTC_DR_YT_SHIFT) |
    ((uint32_t)(RTCDateTime.Year % 10) << RTC_DR_YU_SHIFT) |
    ((uint32_t)(RTCDateTime.Month / 10) << RTC_DR_MT_SHIFT) |
    ((uint32_t)(RTCDateTime.Month % 10) << RTC_DR_MU_SHIFT) |
    ((uint32_t)(RTCDateTime.Day / 10) << RTC_DR_DT_SHIFT) |
    ((uint32_t)(RTCDateTime.Day % 10) << RTC_DR_DU_SHIFT);          // set date
    
  pwr_disable_backup_domain_write_protect();
  rtc_unlock();
  RTC_ISR |= RTC_ISR_INIT;
  while (!(RTC_ISR & RTC_ISR_INITF));	// enable RTC init mode
  if (Which & SET_TIME)
    RTC_TR = timeval;		// set time
  if (Which & SET_DATE)
    RTC_DR = dateval;		// set date

  RTC_ISR &= ~(RTC_ISR_RSF);
  while (RTC_ISR & RTC_ISR_RSF);	// synchronize shadow regs

  RTC_ISR &= ~RTC_ISR_INIT;
  while (RTC_ISR & RTC_ISR_INITF);	// disable init mode

  RTC_BKPXR(0) = RTC_INITIALIZED;      // say we've got it
  rtc_lock();
  pwr_enable_backup_domain_write_protect();

  return;
} // SetRTCDateTime


//*	Display the current time of day.
//	--------------------------------
//	

void ShowRTCTime( void)
{

  GetRTCDateTime();

  Uprintf( "Time: %02d:%02d:%02d\n", 
    RTCDateTime.Hours,
    RTCDateTime.Minutes,
    RTCDateTime.Seconds);
  return;
} // ShowRTCTime

//*  Display the current date.
//   -------------------------
//
//	By default, we do MM/DD/YYYY
//

void ShowRTCDate(void)
{

  GetRTCDateTime();

  Uprintf( "Date: %02d/%02d/%4d\n", 
    RTCDateTime.Month,
    RTCDateTime.Day,
    2000 + RTCDateTime.Year);
  return;
}  // ShowRTCDate

//* SetRTCTime - Set the time of day.
//  ----------------------------------
//
//  Prompts for HHMMSS and MMDDYY
//

void SetRTCTime(void)
{

  char
    inBuf[10];
  int32_t
    inval;
  
  ShowRTCDate(); 
  while ( 1)
  { 

    int mm, dd, yy;
  
    Uprintf( "Enter current date, MMDDYY or return to skip: ");
    Ugets( inBuf, sizeof( inBuf)-1);
    Uprintf("\n");
    if (!inBuf[0])
      break;
     
    inval = atoi( inBuf);           // convert to int
    yy = inval % 100;               // low order 2 digits
    dd = (inval % 10000) / 100;     // middle 2 digits
    mm = inval / 10000;             // high order digits
    if ( ( yy > 50) ||
       (( mm <= 0) | (mm > 12)) ||
       ((dd <= 0) | (dd > 31)) )
    {
      Uprintf( "\n Date error, please try again.\n");  
      continue;
    }   
    Uprintf( "Setting %02d/%02d/%04d\n",mm,dd, yy+2000);
    RTCDateTime.Day = dd;
    RTCDateTime.Month = mm;
    RTCDateTime.Year = yy;
    SetRTCDateTime(SET_DATE);
    break;
  } // until we get it right

//  Now, set the time.

  ShowRTCTime();
  while ( 1)
  { 

    int hh, mm, ss;
  
    Uprintf( "Enter current time, HHMMSS or return to skip: ");
    Ugets( inBuf, sizeof( inBuf)-1);
    Uprintf("\n");
    if (!inBuf[0])
      break;            // skip setting time
    inval = atoi( inBuf);           // convert to int
    ss = inval % 100;               // low order 2 digits
    mm = (inval % 10000) / 100;     // middle 2 digits
    hh = inval / 10000;             // high order digits
    if ( ((ss < 0) || (ss > 59)) ||
         (( mm < 0) | (mm > 59)) ||
         ((hh < 0) | (hh > 23)) )
    {
      Uprintf( "\n Time error, please try again.\n");  
      continue;
    }
    RTCDateTime.Seconds = ss;
    RTCDateTime.Minutes = mm;
    RTCDateTime.Hours = hh;
    SetRTCDateTime( SET_TIME);
    break;
  } // until we get it right
  return;
} // SetRTCTime

//  GetRTCDOSTime - Get time in DOS format.
//  ---------------------------------------
//
//

uint32_t GetRTCDOSTime( void)
{

  uint32_t
    dosTime;

  GetRTCDateTime();       // read the date and time into RTCDateTime struct

  dosTime =
    ((RTCDateTime.Year + 20) << 25) |	// year is 1980-based, so add 20
    (RTCDateTime.Month << 21) |
    (RTCDateTime.Day  << 16) |
    (RTCDateTime.Hours << 11) |
    (RTCDateTime.Minutes << 5) |
    (RTCDateTime.Seconds >> 1);
  return dosTime;  
} // GetRTCDOSTime