/**
 * @file        uart.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        08 January, 2020
 * @brief       UART Driver Interface file
*/

#ifndef _UART_H_
#define _UART_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */



/* Exported constants ------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/**
 * @brief	Initialize the board specific UART that will be used by the
 * 			kernel during the system start up
 *
 * @param	No parameters
 *
 * @retval	Success
 */
int32_t UartOpen();

/**
 * @brief	Close the board specific UART, to release it for being user by
 * 			user land processes
 *
 * @param	No parameters
 *
 * @retval	Success
 */
int32_t UartClose();

/**
 * @brief    Get a character from the UART communication channel
 *
 * @param    No parameters
 *
 * @retval   Return the character that has been received from the UART
 */
int32_t getc();

/**
 * @brief    Reads characters from the UART communication channel and stores
 *           them into str until a newline character is reached.
 *
 * @param    str - Pointer to a block of memory where the string read is copied
 *
 * @retval   Returns str
 */
char *gets(char *str);

/**
 * @brief    Send a character to the UART communication channel
 *
 * @param    c - Character to be sent by the UART
 *
 * @retval   No return value
 */
void putc(char c);

/**
 * @brief    Send a string of character to the UART communication channel
 *
 * @param    s - String to be sent by the UART
 *
 * @retval   No return value
 */
void puts(const char *s);

#ifdef __cplusplus
    }
#endif

#endif
