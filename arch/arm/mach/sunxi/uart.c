/**
 * @file        uart.c
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        18 March, 2018
 * @brief       Sunxi Allwiner H3 Uart Driver file
*/


/* Includes ----------------------------------------------- */
#include <uart.h>
#include <devices.h>
#include <kvspace.h>


/* Private types ------------------------------------------ */

typedef struct
{
	volatile uint32_t data;	/* 00 - Rx/Tx data */
	volatile uint32_t ier;	/* 04 - interrupt enables */
	volatile uint32_t iir;	/* 08 - interrupt ID / FIFO control */
	volatile uint32_t lcr;	/* 0c - line control */
	volatile uint32_t mcr;	/* 10 - modem control */
	volatile uint32_t lsr;	/* 14 - line status */
	volatile uint32_t msr;	/* 18 - modem status */
}h3_uart_t;


/* Private constants -------------------------------------- */

#define SUNXI_UART0		(0x01C28000)
#define SUNXI_UART1		(0x01C28400)
#define SUNXI_UART2		(0x01C28800)
#define SUNXI_UART3		(0x01C28C00)

/* bits in the lsr */
#define RX_READY	0x01
#define TX_READY	0x40
#define TX_EMPTY	0x80

/* bits in the ier */
#define IE_RDA		0x01	/* Rx data available */
#define IE_TXE		0x02	/* Tx register empty */
#define IE_LS		0x04	/* Line status */
#define IE_MS		0x08	/* Modem status */

#define LCR_DATA_5	0x00	/* 5 data bits */
#define LCR_DATA_6	0x01	/* 6 data bits */
#define LCR_DATA_7	0x02	/* 7 data bits */
#define LCR_DATA_8	0x03	/* 8 data bits */

#define LCR_STOP	0x04	/* extra (2) stop bits, else: 1 */
#define LCR_PEN		0x08	/* parity enable */

#define LCR_EVEN	0x10	/* even parity */
#define LCR_STICK	0x20	/* stick parity */
#define LCR_BREAK	0x40

#define LCR_DLAB	0x80	/* divisor latch access bit */

// Configurations
#define BAUD_115200    (0xD) /* 24 * 1000 * 1000 / 16 / 115200 = 13 */
#define NO_PARITY      (0)
#define ONE_STOP_BIT   (0)
#define DAT_LEN_8_BITS (3)
#define LC_8_N_1       (NO_PARITY << 3 | ONE_STOP_BIT << 2 | DAT_LEN_8_BITS)

/* Private macros ----------------------------------------- */

#define  ASCII_BS   0x08     /* Backspace */
#define  ASCII_SP   0x20     /* Space */
#define  ASCII_DEL  0x7F     /* Delete */

/* Private variables -------------------------------------- */

static h3_uart_t *uart;
static dev_t     *uartDevice;


/* Private function prototypes ---------------------------- */



/* Private functions -------------------------------------- */

/**
 * UartOpen Implementation (See header arch/include/uart.h file for description)
*/
int32_t UartOpen()
{
	uartDevice = DeviceGet((paddr_t)SUNXI_UART0, 1024);

	if(uartDevice == NULL)
	{
//		return E_ERROR;
	}

	// Each UART only has 1Kb memory but the minimum amount of memory we can map is 4Kb
	// so we will map 4k
	uart = (h3_uart_t *)VirtualSpaceIomap((paddr_t)SUNXI_UART0, 4096);

	/* Disable uart interrupts*/
	uart->ier = 0;
	/* select dll dlh */
	uart->lcr = LCR_DLAB;
	/* set baudrate */
	uart->ier = 0;				// DLH
	uart->data = BAUD_115200;	// LSB
	/* set line control */
	uart->lcr = LC_8_N_1;

	return E_OK;
}

/**
 * UartClose Implementation (See header arch/include/uart.h file for description)
*/
int32_t UartClose()
{
	// Unmap UART
	VirtualSpaceUnmmap((vaddr_t)uart);

	// Free the UART to allow it to be used by user processes
	DeviceFree(uartDevice);

	return E_OK;
}

/**
 * getc Implementation (See header arch/include/uart.h file for description)
*/
int32_t getc()
{
	while(!(uart->lsr & RX_READY))
    {

    }

    return (int32_t)uart->data;
}

/**
 * gets Implementation (See header arch/include/uart.h file for description)
*/
char *gets(char *str)
{
    char *buffer = str;

    while(TRUE)
    {
        *buffer = (char)getc();

        if(((*buffer == ASCII_BS) || (*buffer == ASCII_DEL)) && (buffer != str))
        {
            // Go back one character
            buffer--;
            // Delete last character from cmd
            putc(ASCII_BS);
			putc(ASCII_SP);
			putc(ASCII_BS);
            // Get new character
            continue;
        }

        if(*buffer == '\r')
        {
            // Move cursor to next line
            putc('\n');
            putc('\r');

            if(buffer == str)
            {
                // We still didn't read a string so continue
                continue;
            }

            // End of input string
            *buffer = '\0';
            // Return pointer to string beginning
            return str;
        }

        // Echo received character
        putc(*buffer);
        // Move to next buffer position
        buffer++;
    }
}

/**
 * putc Implementation (See header arch/include/uart.h file for description)
*/
void putc(char c)
{
	while (!(uart->lsr & TX_READY))
	{

	}

	uart->data = c;
}

/**
 * puts Implementation (See header arch/include/uart.h file for description)
*/
void puts(const char *s)
{
	while (*s)
	{
		if (*s == '\n')
		{
			putc('\r');
		}
		putc(*s++);
	}
}
