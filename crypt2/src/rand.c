/****
		Random Number Generator. !!! TEMPORARY / Not Portable !!!
****/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>		// Not Portable
#include <sys/random.h>	// Not Portable

#include "crypt2.h"


// Get Random Bytes.
int _randombytes_linux_getrandom(void * const _buf, const size_t _size)
{
    int readnb;

    do {
        readnb = getrandom(_buf, _size, 0);
    } while (readnb < 0);

    return (readnb == (int) _size) - 1;
}


// Generate M17, 112 Bit (14 Byte) Nonce. Includes 32 Bit Timestamp and 80 Bit Random Nonce
int get_m17_nonce(char *_nonce, const size_t _nonce_len)
{
	// Specific Nonce size required.
	if (_nonce_len == 14)
	{
		// Get Current UNIX Time. Subtract Seconds from 1970 to 2020.
		uint32_t seconds;
	   	seconds = (time(NULL) - 1577836800);

	   	// Add Timestamp to start of Nonce.
	   	for (int i=0; i<4; i++)
	   	{
	   		_nonce[i] = (uint32_t)((seconds >> (24 - (8*i))) & 0xFF);
	   	}

	   	// Get remaining random Nonce including CTR_HIGH.
	   	char randombytes[10];
	   	memset(&randombytes[0], 0, sizeof(randombytes));

	   	int ret = _randombytes_linux_getrandom(randombytes, sizeof(randombytes));

	   	memcpy(_nonce + 4, randombytes, 10);

	    return 0;
	}
	else
		return -1;

}