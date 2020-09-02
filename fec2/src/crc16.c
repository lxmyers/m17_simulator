/****
		CRC16 Error-detecting code.
****/

#include <stdio.h>
#include <stdlib.h>

#include "fec2.h"

/****
        Non-Reflected CRC16 Algorithm.
****/

// width=16 poly=0xAC9A init=0xFFFF refin=false refout=false xorout=0x0000 check=0x9630 residue=? name="CRC-16/M17"
unsigned short crc16_m17(unsigned char *_msg, unsigned int _msg_len)
{
    unsigned short crc = 0xFFFF; // Initial Value

    while (_msg_len--)
    {
        crc = crc ^ (*_msg++ << 8);
        for (int i=0; i<8; i++)
        {
           if (crc & 0x8000)
               crc = (crc << 1) ^ 0xAC9A; // CRC-16 Poly
           else
               crc <<= 1;
        }
    }

    crc ^= 0x0000; // Final XOR

    return crc;
}

/****
        Reflected CRC16 Algorithm.
****/

// width=16 poly=0xAC9A init=0xFFFF refin=true refout=true xorout=? check=? residue=? name="CRC-16/M17 Reflected"
unsigned short crc16_m17r(unsigned char *_msg, unsigned int _msg_len)
{
    unsigned short crc = 0xFFFF; // Initial Value

    while (_msg_len--)
    {
        crc ^= *_msg++;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0x5935 : crc >> 1; //0x5935 is reflection of CRC-16 Poly 0xAC9A
    }

    crc ^= 0x0000; // Final XOR

    return crc;
}