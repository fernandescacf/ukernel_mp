/**
 * @file        pl011_uart.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        08 January, 2020
 * @brief       VE Pl011 Driver
*/


/* Includes ----------------------------------------------- */
#include <uart.h>
#include <devices.h>
#include <kvspace.h>


/* Private types ------------------------------------------ */

typedef struct
{
   volatile uint32_t data;                // UART Data Register
   volatile uint32_t status_error;        // UART Receive Status Register/Error Clear Register
   const uint32_t reserved1[4];            // Reserved: 4(0x4) bytes
   volatile uint32_t flag;                // UART Flag Register
   const uint32_t reserved2[1];            // Reserved: 1(0x1) bytes
   volatile uint32_t lp_counter;          // UART Low-power Counter Register
   volatile uint32_t integer_br;          // UART Integer Baud Rate Register
   volatile uint32_t fractional_br;       // UART Fractional Baud Rate Register
   volatile uint32_t line_control;        // UART Line Control Register
   volatile uint32_t control;             // UART Control Register
   volatile uint32_t isr_fifo_level_sel;  // UART Interrupt FIFO level Select Register
   volatile uint32_t isr_mask;            // UART Interrupt Mask Set/Clear Register
   volatile uint32_t raw_isr_status;      // UART Raw Interrupt Status Register
   volatile uint32_t masked_isr_status;   // UART Masked Interrupt Status Register
   volatile uint32_t isr_clear;           // UART Interrupt Clear Register
   volatile uint32_t DMA_control;         // UART DMA control Register
} pl011_uart;


/* Private constants -------------------------------------- */

/* UART Base Address (PL011) */

#if defined(ARM_FVP)
    #define UART_0      (0x1C090000)
    #define UART_1      (0x1C0A0000)
    #define UART_2      (0x1C0B0000)
    #define UART_3      (0x1C0C0000)
#elif defined(QEMU)
    #define UART_0      (0x10009000)
    #define UART_1      (0x1000A000)
    #define UART_2      (0x1000B000)
    #define UART_3      (0x1000C000)
#endif

#define UART_CLK 		(24000000)
#define UART_BAUD_RATE	(115200)

/*UART Control Register*/
#define UART_CR_UARTEN      (1 << 0)      // UART enable
#define UART_CR_LBE         (1 << 7)      // Loop back enable
#define UART_CR_TXE         (1 << 8)      // Transmit enable
#define UART_CR_RXE         (1 << 9)      // Receive enable

/*UART Line Control Register*/
#define UART_LCR_FEN        (1 << 4)      // Enable FIFOs
#define UART_LCR_WLEN_8     (0b11 << 5)   // Word length 8 bits

/*UART Flag Register*/
#define UART_FR_BUSY        (1 << 3)      // UART busy. If this bit is set to 1, the UART is busy transmitting data.
#define UART_FR_RXFE        (1 << 4)      // If the FIFO is disabled, this bit is set when the receive holding register is empty.
                                          // If the FIFO is enabled, the RXFE bit is set when the receive FIFO is empty.
#define UART_FR_TXFF        (1 << 5)      // If the FIFO is disabled, this bit is set when the transmit holding register is full.
                                          // If the FIFO is enabled, the TXFF bit is set when the transmit FIFO is full.

/*UART Interrupt FIFO Level Select Register*/
#define UART_IFLS_RXIFLSEL_1_2  (0b010 << 3)   // Receive interrupt FIFO level -> 1/2 full

/*UART Interrupt Clear Register*/
#define UART_ICR_FEIC       (1 << 7)       //
#define UART_ICR_PEIC       (1 << 8)       //
#define UART_ICR_BEIC       (1 << 9)       //
#define UART_ICR_OEIC       (1 << 10)      //

/* Private macros ----------------------------------------- */

#define  ASCII_BS   0x08     /* Backspace */
#define  ASCII_SP   0x20     /* Space */
#define  ASCII_DEL  0x7F     /* Delete */


/* Private variables -------------------------------------- */

static pl011_uart * uart;
static dev_t      * uartDevice;


/* Private function prototypes ---------------------------- */

/**
 * @brief	Routine to disable the UART
 *
 * @param	No parameters
 *
 * @retval	None
 */
static void UartDisable();

/**
 * @brief	Routine to enable the UART
 *
 * @param	No parameters
 *
 * @retval	None
 */
static void UartEnable();

/**
 * @brief	Routine to set the UART Baudrate
 *
 * @param	No parameters
 *
 * @retval	None
 */
static void UartSetBaudrate();

/* Private functions -------------------------------------- */

/**
 * UartDisable Implementation (See header file for description)
*/
void UartDisable()
{
	/* Clear control configuration */
    uart->control = 0x00000000;
}

/**
 * UartEnable Implementation (See header file for description)
*/
void UartEnable()
{
	/* Enable RX */
    uart->control = (UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE);
}

/**
 * UartSetBaudrate Implementation (See header file for description)
*/
void UartSetBaudrate()
{
    uint32_t temp;
    uint32_t ibrd;
    uint32_t mod;
    uint32_t fbrd;

    uint32_t baud_rate =  UART_BAUD_RATE;

    /*
     * Set baud rate
     *
     * IBRD = UART_CLK / (16 * BAUD_RATE)
     * FBRD = ROUND((64 * MOD(UART_CLK,(16 * BAUD_RATE))) / (16 * BAUD_RATE))
     */
    temp = 16 * baud_rate;
    ibrd = UART_CLK / temp;
    mod = UART_CLK % temp;
    fbrd = (4 * mod) / baud_rate;

    /* Set the values of the baudrate divisors */
    uart->integer_br = ibrd;
    uart->fractional_br = fbrd;
}

/**
 * UartOpen Implementation (See header arch/include/uart.h file for description)
*/
int32_t UartOpen()
{
	uartDevice = DeviceGet((paddr_t)UART_0, 4096);

	if(uartDevice == NULL)
	{
		return E_ERROR;
	}

	uart = (pl011_uart *)VirtualSpaceIomap((paddr_t)UART_0, 4096);

    int lcrh_reg;

    /* First, disable everything */
    uart->control = 0x0;

    /* Disable the FIFOs */
    lcrh_reg = uart->line_control;
    lcrh_reg &= ~UART_LCR_FEN;
    uart->line_control = lcrh_reg;

    // Set Baudrate
    UartSetBaudrate();

    /* Set the UART to be 8 bits, 1 stop bit and no parity
     * FIFOs enable
     */
    uart->line_control = (UART_LCR_WLEN_8 | UART_LCR_FEN);

    /* Enable the UART, enable TX and enable loop back*/
    uart->control = (UART_CR_UARTEN | UART_CR_TXE | UART_CR_LBE);

    uart->data = 0x0;

    while(uart->flag & UART_FR_BUSY);

    /* Enable RX */
    UartEnable();

    /* Clear interrupts */
    uart->isr_clear = (UART_ICR_OEIC | UART_ICR_BEIC | UART_ICR_PEIC | UART_ICR_FEIC);

    return E_OK;
}

/**
 * UartClose Implementation (See header arch/include/uart.h file for description)
*/
int32_t UartClose()
{
	// Disable UART
	UartDisable();

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
    uint32_t data = 0;

    //wait until there is data in FIFO
    while(uart->flag & UART_FR_RXFE)
    {

    }

    data = uart->data;

    return data;
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
    //wait until txFIFO is not full
    while(uart->flag & UART_FR_TXFF)
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
    	if(*s == '\n')
    	{
    		putc(*s++);
    		putc('\r');
    	}
    	else{
    		putc(*s++);
    	}
    }
}
