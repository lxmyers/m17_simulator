/*
 * Copyright (c) 2007 - 2015 Joseph Gaeddert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//
// Golay(24,12) half-rate forward error-correction code
//
// References:
//  [Lin:2004] Lin, Shu and Costello, Daniel L. Jr., "Error Control
//      Coding," Prentice Hall, New Jersey, 2nd edition, 2004.
//

#include <stdio.h>
#include <stdlib.h>

#include "fec2.h"


// number of ones in a byte
const unsigned char c_ones[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};


// count the number of ones in an integer
unsigned int count_ones(unsigned int _x) {

    return ( c_ones[(_x) & 0xff] + c_ones[((_x)>>8) & 0xff] );
}

// P matrix [12 x 12]
unsigned int golay2412_P[12] = {
    0x08ed, 0x01db, 0x03b5, 0x0769,
    0x0ed1, 0x0da3, 0x0b47, 0x068f,
    0x0d1d, 0x0a3b, 0x0477, 0x0ffe};

// generator matrix transposed [24 x 12]
unsigned int golay2412_Gt[24] = {
    0x08ed, 0x01db, 0x03b5, 0x0769, 0x0ed1, 0x0da3, 0x0b47, 0x068f, 
    0x0d1d, 0x0a3b, 0x0477, 0x0ffe, 0x0800, 0x0400, 0x0200, 0x0100, 
    0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001};

// parity check matrix [12 x 24]
unsigned int golay2412_H[12] = {
    0x008008ed, 0x004001db, 0x002003b5, 0x00100769,
    0x00080ed1, 0x00040da3, 0x00020b47, 0x0001068f,
    0x00008d1d, 0x00004a3b, 0x00002477, 0x00001ffe};

/* 
 * Returns length of data if encoded with Golay(24,12).
 * 
 * _dec_msg_len    :   decoded message length (number of bytes)
 */
unsigned int fec_golay_get_enc_msg_len(unsigned int _dec_msg_len)
{
    unsigned int _m = 12;
    unsigned int _k = 24;

    // validate input
    if (_m == 0) {
        fprintf(stderr,"fec_block_get_enc_msg_len(), input block size cannot be zero\n");
        exit(1);
    } else if (_k < _m) {
        fprintf(stderr,"fec_block_get_enc_msg_len(), output block size cannot be smaller than input\n");
        exit(1);
    }

    unsigned int num_bits_in = _dec_msg_len*8;
    unsigned int num_blocks = num_bits_in / _m + (num_bits_in%_m ? 1 : 0);
    unsigned int num_bits_out = num_blocks * _k;
    unsigned int num_bytes_out = num_bits_out/8 + (num_bits_out%8 ? 1 : 0);

    return num_bytes_out;
}

// multiply input vector with parity check matrix, H
unsigned int golay2412_matrix_mul(unsigned int   _v, unsigned int * _A, unsigned int   _n)
{
    unsigned int x = 0;
    unsigned int i;
    for (i=0; i<_n; i++) {
        x <<= 1;

        // Exploits the fact that vectors are at
        // most 24 bits long; liquid_count_ones_mod2() assumes full
        // 32- or 64-bit integer
        unsigned int c = 0;
        unsigned int p = _A[i] & _v;
        c += c_ones[ p & 0xff ]; p >>= 8;
        c += c_ones[ p & 0xff ]; p >>= 8;
        c += c_ones[ p & 0xff ];

        x |= c & 0x0001;    // mod 2
    }
    return x;
}

unsigned int fec_golay2412_encode_symbol(unsigned int _sym_dec)
{
    // validate input
    if (_sym_dec >= (1<<12)) {
        printf("error, fec_golay2412_encode_symbol(), input symbol too large\n");
        exit(1);
    }

    // compute encoded/transmitted message: v = m*G
    return golay2412_matrix_mul(_sym_dec, golay2412_Gt, 24);
}

// search for p[i] such that w(v+p[i]) <= 2, return -1 on fail
int golay2412_parity_search(unsigned int _v)
{
    //assert( _v < (1<<12) );

    unsigned int i;
    for (i=0; i<12; i++) {

        // Faster by exploiting the fact that P has
        // only 12 bits of resolution
        unsigned int wj = 0;
        unsigned int p  = _v ^ golay2412_P[i];
        wj += c_ones[ (p     ) & 0xff ];
        wj += c_ones[ (p >> 8) & 0xff ];

        if (wj <= 2)
            return i;
    }

    // could not find p[i] to satisfy criteria
    return -1;
}

unsigned int fec_golay2412_decode_symbol(unsigned int _sym_enc)
{
    // validate input
    if (_sym_enc >= (1<<24)) {
        fprintf(stderr,"error, fec_golay2412_decode_symbol(), input symbol too large\n");
        exit(1);
    }

    // state variables
    unsigned int s=0;       // syndrome vector
    unsigned int e_hat=0;   // estimated error vector
    unsigned int v_hat=0;   // estimated transmitted message
    unsigned int m_hat=0;   // estimated original message
    
    // compute syndrome vector, s = r*H^T = ( H*r^T )^T
    s = golay2412_matrix_mul(_sym_enc, golay2412_H, 12);

    // compute weight of s (12 bits)
    unsigned int ws = count_ones(s);

    // step 2:
    e_hat = 0;
    if (ws <= 3) {

        // set e_hat = [s 0(12)]
        e_hat = (s << 12) & 0xfff000;
    } else {
        // step 3: search for p[i] s.t. w(s+p[i]) <= 2

        int s_index = golay2412_parity_search(s);

        if (s_index >= 0) {
            // vector found!

            // NOTE : uj = 1 << (12-j-1)
            e_hat = ((s ^ golay2412_P[s_index]) << 12) | (1 << (11-s_index));
        } else {
            // step 4: compute s*P
            unsigned int sP = golay2412_matrix_mul(s, golay2412_P, 12);


            // compute weight of sP (12 bits)
            unsigned int wsP = count_ones(sP);


            if (wsP == 2 || wsP == 3) {
                // step 5: set e = [0, s*P]

                e_hat = sP;
            } else {

                // step 6: search for p[i] s.t. w(s*P + p[i]) == 2...
                int sP_index = golay2412_parity_search(sP);

                if (sP_index >= 0) {
                    // vector found!

                    // NOTE : uj = 1 << (12-j-1)
                    //      [      uj << 1 2    ] [    sP + p[j]    ]
                    e_hat = (1 << (23-sP_index)) | (sP ^ golay2412_P[sP_index]);
                }
                else {
                    // step 7: decoding error
                    printf("  **** decoding error\n");
                }
            }
        }
    }

    // step 8: compute estimated transmitted message: v_hat = r + e_hat
    v_hat = _sym_enc ^ e_hat;
    
    // compute estimated original message: (last 12 bits of encoded message)
    m_hat = v_hat & 0x0fff;

    return m_hat;
}

/* 
 * Encodes a block of data using Golay(24,12) encoder.
 * 
 * _dec_msg_len    :   decoded message length (number of bytes)
 * _msg_dec        :   decoded message [size: 1 x _dec_msg_len]
 * _msg_enc        :   encoded message [size: 1 x 2*_dec_msg_len]
 * 
 * Encoded Message will be padded if required.
 */
void fec_golay2412_encode(unsigned int _dec_msg_len, unsigned char *_msg_dec, unsigned char *_msg_enc)
{
    unsigned int i=0;           // decoded byte counter
    unsigned int j=0;           // encoded byte counter
    unsigned int s0, s1, s2;    // three 8-bit symbols
    unsigned int m0, m1;        // two 12-bit symbols (uncoded)
    unsigned int v0, v1;        // two 24-bit symbols (encoded)

    // determine remainder of input length / 3
    unsigned int r = _dec_msg_len % 3;

    for (i=0; i<_dec_msg_len-r; i+=3) {
        
        // strip three input bytes (two uncoded symbols)
        s0 = _msg_dec[i+0];
        s1 = _msg_dec[i+1];
        s2 = _msg_dec[i+2];

        // pack into two 12-bit symbols
        m0 = ((s0 << 4) & 0x0ff0) | ((s1 >> 4) & 0x000f);
        m1 = ((s1 << 8) & 0x0f00) | ((s2     ) & 0x00ff);

        // encode each 12-bit symbol into a 24-bit symbol
        v0 = fec_golay2412_encode_symbol(m0);
        v1 = fec_golay2412_encode_symbol(m1);

        // unpack two 24-bit symbols into six 8-bit bytes
        // retaining order of bits in output
        _msg_enc[j+0] = (v0 >> 16) & 0xff;
        _msg_enc[j+1] = (v0 >>  8) & 0xff;
        _msg_enc[j+2] = (v0      ) & 0xff;
        _msg_enc[j+3] = (v1 >> 16) & 0xff;
        _msg_enc[j+4] = (v1 >>  8) & 0xff;
        _msg_enc[j+5] = (v1      ) & 0xff;

        j += 6;
    }

    // if input length isn't divisible by 3, encode last 1 or two bytes
    for (i=_dec_msg_len-r; i<_dec_msg_len; i++) {
        // strip last input symbol
        s0 = _msg_dec[i];

        // extend as 12-bit symbol
        m0 = s0;

        // encode into 24-bit symbol
        v0 = fec_golay2412_encode_symbol(m0);

        // unpack one 24-bit symbol into three 8-bit bytes, and
        // append to output array
        _msg_enc[j+0] = ( v0 >> 16 ) & 0xff;
        _msg_enc[j+1] = ( v0 >>  8 ) & 0xff;
        _msg_enc[j+2] = ( v0       ) & 0xff;

        j += 3;
    }

    //assert( j == fec_golay_get_enc_msg_len(_dec_msg_len) );
    //assert( i == _dec_msg_len);
}

/* 
 * Decodes a block of Golay(24,12) encoded data.
 * 
 * _dec_msg_len    :   decoded message length (number of bytes)
 * _msg_enc        :   encoded message [size: 1 x 2*_dec_msg_len]
 * _msg_dec        :   decoded message [size: 1 x _dec_msg_len]
 */
void fec_golay2412_decode(unsigned int _dec_msg_len, unsigned char *_msg_enc, unsigned char *_msg_dec)
{
    unsigned int i=0;                       // decoded byte counter
    unsigned int j=0;                       // encoded byte counter
    unsigned int r0, r1, r2, r3, r4, r5;    // six 8-bit bytes
    unsigned int v0, v1;                    // two 24-bit encoded symbols
    unsigned int m0_hat, m1_hat;            // two 12-bit decoded symbols
    
    // determine remainder of input length / 3
    unsigned int r = _dec_msg_len % 3;

    for (i=0; i<_dec_msg_len-r; i+=3) {
        // strip six input bytes (two encoded symbols)
        r0 = _msg_enc[j+0];
        r1 = _msg_enc[j+1];
        r2 = _msg_enc[j+2];
        r3 = _msg_enc[j+3];
        r4 = _msg_enc[j+4];
        r5 = _msg_enc[j+5];

        // pack six 8-bit symbols into two 24-bit symbols
        v0 = ((r0 << 16) & 0xff0000) | ((r1 <<  8) & 0x00ff00) | ((r2     ) & 0x0000ff);
        v1 = ((r3 << 16) & 0xff0000) | ((r4 <<  8) & 0x00ff00) | ((r5 << 0) & 0x0000ff);

        // decode each symbol into a 12-bit symbol
        m0_hat = fec_golay2412_decode_symbol(v0);
        m1_hat = fec_golay2412_decode_symbol(v1);

        // unpack two 12-bit symbols into three 8-bit bytes
        _msg_dec[i+0] = ((m0_hat >> 4) & 0xff);
        _msg_dec[i+1] = ((m0_hat << 4) & 0xf0) | ((m1_hat >> 8) & 0x0f);
        _msg_dec[i+2] = ((m1_hat     ) & 0xff);

        j += 6;
    }

    // if input length isn't divisible by 3, decode last 1 or two bytes
    for (i=_dec_msg_len-r; i<_dec_msg_len; i++) {
        // strip last input symbol (three bytes)
        r0 = _msg_enc[j+0];
        r1 = _msg_enc[j+1];
        r2 = _msg_enc[j+2];

        // pack three 8-bit symbols into one 24-bit symbol
        v0 = ((r0 << 16) & 0xff0000) | ((r1 <<  8) & 0x00ff00) | ((r2     ) & 0x0000ff);

        // decode into a 12-bit symbol
        m0_hat = fec_golay2412_decode_symbol(v0);

        // retain last 8 bits of 12-bit symbol
        _msg_dec[i] = m0_hat & 0xff;

        j += 3;
    }

    //assert( j== fec_golay_get_enc_msg_len(_dec_msg_len) );
    //assert( i == _dec_msg_len);
}