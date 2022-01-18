/**
 * @file        string.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        December 20, 2019
 * @brief       String Definition Header File
*/

#ifndef _STRING_H
#define _STRING_H


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported constants ------------------------------------- */



/* Exported types ----------------------------------------- */



/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */

/*
 * @brief   Sets the first n bytes of the block of memory pointed by s
 * 			to the specified value c (interpreted as an unsigned char)
 * @param   s:	Pointer to the block of memory to fill
 * 			c:	Value to be set. The value is passed as an int, but the
 * 				function fills the block of memory using the unsigned
 * 				char conversion of this value
 * 			n:	Number of bytes to be set to the value
 * @retval  s is returned
 */
void *memset(void *s, int c, uint32_t n);

/*
 * @brief   Copies the values of len bytes from the location pointed to
 * 			by src directly to the memory block pointed to by dst
 * @param   dst:	Pointer to the destination array where the content
 * 					is to be copied, type-casted to a pointer of type void*
 * 			src:	Pointer to the source of data to be copied, type-casted
 * 					to a pointer of type const void*
 * 			len:	Number of bytes to copy
 * @retval  dst  is returned
 */
void *memcpy(void *dst, const void *src, uint32_t len);

/*
 * @brief   Compares the first n bytes of the block of memory pointed by
 * 			s1 to the first n bytes pointed by s2, returning zero if
 * 			they all match or a value different from zero representing which
 * 			is greater if they do not
 * @param   s1:	Pointer to block of memory
 * 			s2:	Pointer to block of memory
 * 			n:	Number of bytes to compare
 * @retval  Return 0 if both memory blocks are equal
 */
int32_t memcmp(const void *s1, const void *s2, uint32_t n);

/*
 * @brief   Returns the length of the C string str
 * @param   str:	C string
 * @retval  The length of string
 */
uint32_t strlen(const void *str);

/*
 * @brief   Compares the C string s1 to the C string s2
 * @param   s1:	C string to be compared
 * 			s2:	C string to be compared
 * @retval  0 if both strings are equal
 */
int32_t strcmp(const void *s1, const void *s2);

#endif // _STRING_H
