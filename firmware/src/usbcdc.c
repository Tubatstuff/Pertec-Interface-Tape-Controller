#ifndef USE_UART

//*	USB CDC ACM code.
//	-----------------
//
//	This code implements a USB 2.0 full-speed CDC ACM interface.
//	On Linux systems, this will be assigned as /dev/ttyACMx, where 
//	x is (0,1...).   Windows systems may be different.
//
//	If the symbol USE_UART is defined at compilation, this code is
//	replaced by UART (serial) interface code.	
//


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/f4/nvic.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>
#include "comm.h"

#include "license.h"

#include "usbserial.h"

//  This is the pointer to the device that we'll be using.

static usbd_device 
  *Usbd_dev = NULL;

static int
  Usbd_registered = 0;      // set to 1 once we're registered

#define INPUT_QUEUE_SIZE 1024+64	// size of input queue
#define MAX_PACKET_SIZE 64		// largest packet to send

static char
  ReceiveBuffer[ 65],             	// Where characters come in
  InputQueue[ INPUT_QUEUE_SIZE];	// where we queue input up 

static volatile int
  InQIn,
  InQOut;				// input queue in/out

int _write( int Fd, char *What, int Count);


//  We define a CDC_ACM  device here.
//  ---------------------------------
//
//  This is a bit more versatile than the USB UART dongle-type.
//

static const struct usb_device_descriptor dev = {
  .bLength = USB_DT_DEVICE_SIZE,
  .bDescriptorType = USB_DT_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = USB_CLASS_CDC,
  .bDeviceSubClass = 0,
  .bDeviceProtocol = 0,
  .bMaxPacketSize0 = 64,      // standard for full-speed devices
  .idVendor = 0x0483,         // ST Microelectronics
  .idProduct = 0x5740,        // STM32F407--the easiest one we can use
  .bcdDevice = 0x0200,
  .iManufacturer = 1,
  .iProduct = 2,
  .iSerialNumber = 3,
  .bNumConfigurations = 1,
};

// This notification endpoint isn't implemented. According to CDC spec its
// optional, but its absence causes a NULL pointer dereference in Linux
// cdc_acm driver.

static const struct usb_endpoint_descriptor comm_endp[] = 
{
  {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x83,
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 16,
    .bInterval = 255,
  }
};

static const struct usb_endpoint_descriptor data_endp[] = 
{
  {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x01,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
  },
  {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x82,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
  }
};

static const struct 
{
  struct usb_cdc_header_descriptor header;
  struct usb_cdc_call_management_descriptor call_mgmt;
  struct usb_cdc_acm_descriptor acm;
  struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = 
{
  .header = 
  {
    .bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
    .bDescriptorType = CS_INTERFACE,
    .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
    .bcdCDC = 0x0110,
  },
  .call_mgmt = 
  {
    .bFunctionLength = 
    sizeof(struct usb_cdc_call_management_descriptor),
    .bDescriptorType = CS_INTERFACE,
    .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
    .bmCapabilities = 0,
    .bDataInterface = 1,
  },
  .acm = 
  {
    .bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
    .bDescriptorType = CS_INTERFACE,
    .bDescriptorSubtype = USB_CDC_TYPE_ACM,
    .bmCapabilities = 0,
  },
  .cdc_union = 
  {
    .bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
    .bDescriptorType = CS_INTERFACE,
    .bDescriptorSubtype = USB_CDC_TYPE_UNION,
    .bControlInterface = 0,
    .bSubordinateInterface0 = 1, 
  }
};

static const struct usb_interface_descriptor comm_iface[] = 
{
  {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_CDC,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
    .endpoint = comm_endp,
    .extra = &cdcacm_functional_descriptors,
    .extralen = sizeof(cdcacm_functional_descriptors)
  }
};

static const struct usb_interface_descriptor data_iface[] = 
{
  {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_DATA,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
    .endpoint = data_endp,
  }
};

static const struct usb_interface ifaces[] = 
{
  {
    .num_altsetting = 1,
    .altsetting = comm_iface,
  },
  {
    .num_altsetting = 1,
    .altsetting = data_iface,
  }
};

static const struct usb_config_descriptor config = 
{
  .bLength = USB_DT_CONFIGURATION_SIZE,
  .bDescriptorType = USB_DT_CONFIGURATION,
  .wTotalLength = 0,
  .bNumInterfaces = 2,
  .bConfigurationValue = 1,
  .iConfiguration = 0,
  .bmAttributes = 0x80,
  .bMaxPower = 0x32,
  .interface = ifaces,
};

static const char *usb_strings[] = 
{
  "Pertec Tape Controller",
  "USB command interface",
  "Dec2022",
};

uint8_t 
  usbd_control_buffer[128];  // Buffer to be used for control requests

//  Interrupt routines.  Just poll.

void usb_fs_wkup_isr(void) {
  usbd_poll(Usbd_dev);
} // usb_wakeup


//  Process CDC_ACM Control request.
//  --------------------------------
//
//    Mostly ignored.
//

static enum usbd_request_return_codes 
          cdcacm_control_request(usbd_device *usbd_dev,
				  struct usb_setup_data *req,
				  uint8_t **buf,
				  uint16_t *len,
				  void (**complete)(usbd_device *usbd_dev,
						    struct usb_setup_data *req))
{
  (void) complete;
  (void) buf;
  (void) usbd_dev;

  switch(req->bRequest) 
  {
    case USB_CDC_REQ_SET_CONTROL_LINE_STATE: 
    {
    
//  This Linux cdc_acm driver requires this to be implemented
//  even though it's optional in the CDC spec, and we don't
//  advertise it in the ACM functional descriptor.

      return 1;
    }
    
//  Ignore the "Set line coding" request.    
    
  case USB_CDC_REQ_SET_LINE_CODING: 
    if(*len < sizeof(struct usb_cdc_line_coding)) 
      return 0;
    else
      return 1;
    
  } // switch
  return 0;
} //  cdcacm_control_request

//  CDC_ACM Received data request.
//  ------------------------------
//
//  This is where we get a data packet from the host.
//

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
  int 
    len,
    i;

  (void)ep;

  len = usbd_ep_read_packet(usbd_dev, 0x01, ReceiveBuffer, 64);

  for ( i = 0; i < len; i++)
  {
    int iq;
    iq = InQIn+1;
    if ( iq >= INPUT_QUEUE_SIZE)
      iq = 0;
    if ( iq != InQOut)
    {  
      InputQueue[InQIn] = ReceiveBuffer[i];
      InQIn = iq;		// next in
    }
  } // for each received character
} // cdcacm_data_rx_cb


//  CDC ACM Set configuration.
//  --------------------------
//
//  We register our device.
//

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
  
  (void) wValue;
  
  usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_rx_cb);
  usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, NULL);
  usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

  usbd_register_control_callback(usbd_dev,
		USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		cdcacm_control_request);
	
	Usbd_registered = 1;        // say we've got it
} // cdcacm_set_config

//  External API entry points.

//  USInit - Initialize USB interface.
//  -----------------------------------
//
//  Must be called before any I/O is done.  Returns 0.
//

int USInit(  void)
{

//  The following line is assumed to be already set up.

//  rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_OTGHS);

  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
                        GPIO9 | GPIO11 | GPIO12);
  gpio_set_af(GPIOA, GPIO_AF10, GPIO9 | GPIO11 | GPIO12);


  Usbd_dev = usbd_init(&otgfs_usb_driver,
		                   &dev,
		                   &config,
		                   usb_strings,
		                   3,
		                   usbd_control_buffer,
                       sizeof(usbd_control_buffer));

  usbd_register_set_config_callback(Usbd_dev, cdcacm_set_config);

//  Enable USB interrupts.

  while( !Usbd_registered)
    usbd_poll(Usbd_dev);

  nvic_enable_irq(NVIC_USB_FS_WKUP_IRQ);

  return 0;
} // USInit

//*	USClear - Clear buffer contents.
//	--------------------------------
//
//	Removes any initialization messages from input buffer.
//

void USClear( void)
{

  InQIn = 0;		
  InQOut = 0;		// empty the input buffer
} // USClear

//  USGetchar - Get a character from input.
//  ---------------------------------------
//
//    Not the most efficient.  We ignore any buffer overflow.
//

int USGetchar( void)
{

  char
    retChar;

  if ( !Usbd_dev)
    USInit();             // if not initialized, do it.
  while (InQIn == InQOut)
  {
    usbd_poll( Usbd_dev);    	// wait for input
  } // get a character if there's none
  
//  At this point we know that there's something in the buffer.  
  
  retChar = InputQueue[ InQOut++];	// get the character
  if ( InQOut >= INPUT_QUEUE_SIZE)
    InQOut = 0;				// wrap around to the start
  return (unsigned char) retChar;
} // USGetchar

//* USCharReady - See if a character is waiting.
//  --------------------------------------------
//
//  Return 0 if no character; 1 otherwise.
//

int USCharReady( void)
{
  if ( !Usbd_dev)
    USInit();             // if not initialized, do it.

  usbd_poll(Usbd_dev);    // poll
  return ( InQIn == InQOut) ? 0 : 1;
} // USCharReady

//* USPuts - Put a character string to output.
//  ------------------------------------------
//
//  Uses packet-mode transfer, so is somewhat faster.
//

void USPuts( char *What)
{
 
  char
    localBuf[64];
  char 
    *p;
  int
    i;

   
  if ( !Usbd_dev)
    USInit();             // if not initialized, do it.

  while (*What)
  {
    p = localBuf;
    for (  i = 0; i < 62; i++)
    {
      if ( *What == 0)
        break;
      if ( *What == '\n')
      {
        *p++ = '\r';        // make sure of cr-lf  
        i++;
      } // convert LF to CR-LF  
      *p++ = *What++;
    } // for
    if ( i)
    {  // if we have anything to write
      while( usbd_ep_write_packet(Usbd_dev, 0x82, localBuf, i) == 0);
    }
    usbd_poll(Usbd_dev);    // poll
  } // until we've reached the end.
  return;
} // USPuts


//* USPutchar - Put a single character to output.
//  ---------------------------------------------
//
//    If you've got a string, use USPuts(); it's faster.
//

int USPutchar( char What)
{

  const char *crlf="\r\n";

  if ( !Usbd_dev)
    USInit();             // if not initialized, do it.
  if ( What == '\n')
  {
    while( usbd_ep_write_packet(Usbd_dev, 0x82, crlf, 2) == 0);
  }
  else  
  {
    while( usbd_ep_write_packet(Usbd_dev, 0x82, &What, 1) == 0);
  }
  usbd_poll(Usbd_dev);    // poll
  return What;
} // USPutchar

//* USWritechar - Write a single character without "cooking"
//  --------------------------------------------------------
//
//    Just passes the data through.
//

int USWritechar( char What)
{

  if ( !Usbd_dev)
    USInit();             // if not initialized, do it.

  while( usbd_ep_write_packet(Usbd_dev, 0x82, &What, 1) == 0);
  usbd_poll(Usbd_dev);    // poll
  return What;
} // USWritechar


//*  USWriteBlock - Write a block of characters without "cooking"
//   ------------------------------------------------------------
//
//	This is a bit more efficient than single-character writes.
//
//	Always returns zero.
//

int USWriteBlock( uint8_t *What, int Count)
{

  int 
    thisPass;

  while ( Count != 0)
  {
      thisPass = ( Count >= MAX_PACKET_SIZE) ? MAX_PACKET_SIZE-1 : Count;
      
      while( usbd_ep_write_packet(Usbd_dev, 0x82, What, thisPass) == 0)
        usbd_poll(Usbd_dev);    // poll
      What += thisPass;		// advance buffer
      Count -= thisPass;
  }
  return 0;

} //  USWriteBlock

int _write( int Fd, char *What, int Count)
{

  int written = Count;
  
  (void) Fd;
  
  while ( Count > 0)
  {
    USPutchar( *What++);
    Count--;
  }
  return written;
} // _write


#endif