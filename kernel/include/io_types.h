/**
 * @file        io_types.h
 * @author      Carlos Fernandes
 * @version     1.0
 * @date        24 April, 2020
 * @brief       IO Types Definition Header File
*/

#ifndef _IO_TYPES_H_
#define _IO_TYPES_H_

#ifdef __cplusplus
    extern "C" {
#endif


/* Includes ----------------------------------------------- */
#include <types.h>


/* Exported types ----------------------------------------- */

typedef struct
{
	int32_t type;
	int32_t code;
	size_t  sbytes;
	size_t  rbytes;
}io_hdr_t;


/* Exported constants ------------------------------------- */

// System Request Types
#define _IO_INFO		0x00	// only request supported by the system
#define _IO_READ		0x01
#define _IO_WRITE		0x02
#define _IO_OPEN		0x03
#define _IO_CLOSE		0x04
#define _IO_SHARE		0x05
#define _IO_IOCTL		0x06
#define _IO_SEEK		0x07
#define _IO_SPACE		0x08

// _IO_INFO reply types
#define INFO_NAMESPACE	0x1
#define INFO_SERVER	    0x2

#define INFO_NAMESPACE_LS	0
// codes[0x1...0x7FFFFFFF] -> used to specify namespace entries
#define INFO_BEST_MATCH		-1
// Info best match will return the best match (namespace or server)
// with the difference that the name will have also the remaining unresolved path
// NOTE: best match cannot be combined with namespace entries

/* Exported macros ---------------------------------------- */



/* Exported functions ------------------------------------- */


#ifdef __cplusplus
    }
#endif

#endif /* _IO_TYPES_H_ */
