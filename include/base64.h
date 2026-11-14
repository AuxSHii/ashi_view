#ifndef ASHI_BASE64_H
#define ASHI_BASE64_H

#include <stddef.h>
#include <stdint.h>

//return the exact number of characters required to encode src_len bytes in base64 [excluding null terminators]

size_t base64_encoded_size(size_t src_len);

/* encode source file to base64 
encodes src in a dst[destination] buffer, dst must have atleast base64_encoded_size(src_len) + 1 bytes
returns the number of characters written [excluding null terminators]
adds a trailing null terminator at the end
*/

size_t base64_encode(const uint8_t *src, size_t src_len , char *dst, size_t dst_cap);


#endif