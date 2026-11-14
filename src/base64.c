#include "base64.h"

//base64 character array table
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//takes length of source file --return--> no: of char req to encode src->base64

size_t base64_encode_size(size_t src_len){
	return ((src_len + 2) / 3) * 4;
	//as 3bytes [24bits] are splitted into 4 int of 6bits [every int gives a char]
}


//takse source file+its length , dst buffer,req no:of char --> encode src intto base64 and put into dst buffer 

size_t base64_encode(const uint8_t *src, size_t src_len , char *dst, size_t dst_cap){
	//dst buffer we will make and allocate in memory when we run & code protocols
    //if dst buffer have less mem/size then needs to be stored then do nothing
    if(dst_cap <= require){
    	return 0;
    }

    //if you have enough space 
    size_t i = 0; //counter for taking out bytes in 3 groups
    size_t o = 0; //counter for dst buffer index

    //process 3 bytes --> 4 characters
    while(i + 2 < src_len){
    	//32bit triplet for 3 consecutive bytes
    	//24 bit accumulator
    	//slide each 6bit group [ith] to bottom right by bit manipulation
    	uint32_t triple = ( (uint32_t)src[i] << 16 | (uint32_t)src[i+1] << 12 | (uint32_t)src[i+2] << 6);
        dst[o++] = b64_table[(triple >> 18) & 0x3F];
        dst[o++] = b64_table[(triple >> 12) & 0x3F];
        dst[o++] = b64_table[(triple >> 6) & 0x3F];
        dst[o++] = b64_table[triple & 0x3F];

        i += 3; 
    }

    //process remaining bytes group that is not a multiple of 3
    // if only 1 remaining -> add \n null terminator
    //if 2bytes remaining --then--> add =
    if(i < src_len){
		size_t rem = src_len - i; //how many bytes are remaining
		uint32_t triple = (uint32_t)src[i] << 16;
		//send it right down middle
		if(rem == 2){
			triple |= (uint32_t)src[i+1] << 8;  
		}

		dst[o++] = b64_table[(triple >> 18) & 0x3F];
		dst[o++] = b64_table[(triple >> 12) & 0x3F];

		if(rem == 2){
			dst[o++] = b64_table[(triple >> 6) & 0x3F];
			dst[o++] = '=';	}
		else {
			dst[o++] = '=';
			dst[o++] = '=';
		}
    }

    dst[o] = "\0"; //null terminator
    return o; //character accumulated in dst buffer



}