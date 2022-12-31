// GPIO mode definitions.

#define _OUT_PULLUP_SET GPIO_MODE_OUTPUT,GPIO_PUPD_PULLUP,1
#define _OUT_PULLUP_CLR GPIO_MODE_OUTPUT,GPIO_PUPD_PULLUP,0
#define _IN_PULLUP     GPIO_MODE_INPUT,GPIO_PUPD_PULLUP,-1
#define _IN_NONE       GPIO_MODE_INPUT,GPIO_PUPD_NONE,-1
#define _AF_NONE       GPIO_MODE_AF,GPIO_PUPD_NONE,-1
#define _AF_PULLUP     GPIO_MODE_AF,GPIO_PUPD_PULLUP,-1

//  LED definitions.

#define LED1_GPIO      GPIOA    // LED 1 control
#define LED1_BIT       GPIO6
#define LED1_INIT      _OUT_PULLUP_SET

#define LED2_GPIO      GPIOA    // LED 2 control
#define LED2_BIT       GPIO7
#define LED2_INIT      _OUT_PULLUP_SET

#define LED_GPIO      GPIOA     // for the group of LEDs
#define LED_BIT       (LED1_BIT | LED2_BIT)
#define LED_INIT      _OUT_PULLUP_SET

//  USART definitions.

#define USART_RX_GPIO   GPIOA   // USART receive
#define USART_RX_BIT    GPIO10
#define USART_RX_INIT   _AF_NONE

#define USART_TX_GPIO   GPIOA   // USART transmit
#define USART_TX_BIT    GPIO9
#define USART_TX_INIT   _AF_NONE

#define USART_GPIO      GPIOA     // for the group of LEDs
#define USART_BIT       (USART_RX_BIT | USART_TX_BIT)
#define USART_INIT      _AF_NONE

//  SDIO definitions.

#define SDIO_CMD_GPIO   GPIOD     // SDIO command
#define SDIO_CMD_BIT    GPIO2
#define SDIO_CMD_INIT   _AF_PULLUP

#define SDIO_SCK_GPIO   GPIOC     // SDIO clock
#define SDIO_SCK_BIT    GPIO12
#define SDIO_SCK_INIT   _AF_NONE

#define SDIO_D0_GPIO    GPIOC     // SDIO Data 0
#define SDIO_D0_BIT     GPIO8
#define SDIO_D0_INIT    _AF_NONE

#define SDIO_D1_GPIO    GPIOC     // SDIO Data 1
#define SDIO_D1_BIT     GPIO9
#define SDIO_D1_INIT    _AF_NONE

#define SDIO_D2_GPIO    GPIOC     // SDIO Data 2
#define SDIO_D2_BIT     GPIO10
#define SDIO_D2_INIT    _AF_NONE

#define SDIO_D3_GPIO    GPIOC     // SDIO Data 3
#define SDIO_D3_BIT     GPIO11
#define SDIO_D3_INIT    _AF_NONE

#define SDIO_DATA_GPIO  GPIOC     // SDIO Data group
#define SDIO_DATA_BIT   (SDIO_D0_BIT | SDIO_D1_BIT | SDIO_D2_BIT | SDIO_D3_BIT)  
#define SDIO_DATA_INIT  _AF_NONE

//	Definitions for our Pertec interface board.

// Data register--bidirectional; default to input.

#define PDATA_GPIO	GPIOE		// Data register
#define PDATA_BIT	0xff		// 8 bits
#define PDATA_INIT	_IN_NONE	// set to input initially

// Master control register--active low.

#define PCTRL_GPIO	GPIOD		// Pertec control register

#define PCTRL_ENA	GPIO1		// Enable command output
#define PCTRL_DDIR	GPIO5		// Data direction bit	
#define PCTRL_LBUF	GPIO4		// Load buffer bit
#define PCTRL_SSEL	GPIO3		// Status register select (GPIO2 is
                                        // used by SDIO
#define PCTRL_CSEL0	GPIO7		// Select command register 0
#define PCTRL_CSEL1	GPIO6		// Select command register 1
#define PCTRL_TACK	GPIO0		// Transfer acknowledge

#define PCTRL_BIT (PCTRL_DDIR | PCTRL_LBUF | \
        PCTRL_SSEL | PCTRL_CSEL0 | PCTRL_CSEL1 | PCTRL_TACK | PCTRL_ENA)
#define PCTRL_INIT _OUT_PULLUP_SET	// always output, initially high

// Command register - 2 sets

#define PCMD_GPIO	GPIOC		// Pertec command register
#define PCMD_BIT	0xff		// 8 bits
#define PCMD_INIT	_OUT_PULLUP_SET	// see definitions below

// Status registers - 2 sets

#define PSTAT_GPIO	GPIOE		// Pertec status register  
#define PSTAT_BIT	0xff00		// 8 bits (high order)
#define PSTAT_INIT	_IN_NONE	// always input

//  Initialization macros.

#define GPIO_INIT(x) SetupGPIO( x##_GPIO, x##_BIT, x##_INIT)

//  Pin manipulation macros.

#define G_CLEAR(x) gpio_clear( x##_GPIO, x##_BIT)
#define G_SET(x) gpio_set( x##_GPIO, x##_BIT)
#define G_TOGGLE(x) gpio_toggle( x##_GPIO, x##_BIT)
#define G_GETPIN(x) gpio_get( x##_GPIO, x##_BIT)

//  Input-output mode change.

#define G_INPUT(x) gpio_mode_setup(x##_GPIO, GPIO_MODE_INPUT, GPIO_PUPD_NONE, x##_BIT)
#define G_OUTPUT(x) gpio_mode_setup(x##_GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, x##_BIT)                   