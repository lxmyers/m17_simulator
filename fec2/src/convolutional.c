#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fec2.h"


fec convolutional_punctured_create(fec_scheme _fs)
{
    fec _fec = (fec) malloc(sizeof(struct fec_s));

    _fec->scheme = _fs;
    _fec->rate = get_convolutional_rate(_fec);

    switch (_fec->scheme) {

        case CONV_25_P45_60:   init_convolutional_v25_p45_60(_fec);    break;
        case CONV_25_P33_40:   init_convolutional_v25_p33_40(_fec);    break;
    }

    // convolutional-specific decoding
    _fec->num_dec_bytes = 0;
    _fec->enc_bits = NULL;
    _fec->vp = NULL;

    return _fec;
}

void convolutional_punctured_destroy(fec _fec)
{
    // delete viterbi decoder
    if (_fec->vp != NULL)
        _fec->delete_viterbi(_fec->vp);

    if (_fec->enc_bits != NULL)
        free(_fec->enc_bits);

    free(_fec);
}

void convolutional_punctured_encode(fec _fec, unsigned int _dec_msg_len, unsigned char *_msg_dec, unsigned char *_msg_enc)
{
    unsigned int sr=0;  // Convolutional shift register
    unsigned int n=0;   // Output bit counter
    unsigned int p=0;   // puncturing matrix column index

    unsigned char bit;
    unsigned char byte_in;
    unsigned char byte_out=0;

    // Loop through bytes in _msg_dec
    for (int i = 0; i < _dec_msg_len; i++) {
        byte_in = _msg_dec[i];

        // Loop through bits in byte.
        for (int j = 0; j < 8; j++)
        {
            // Shift bit starting with most significant
            bit = (byte_in >> (7-j)) & 0x01;
            sr = (sr << 1) | bit;

            // Compute parity bits for each polynomial
            for (int r = 0; r < _fec->R; r++) {
                // Check each parity bit against Puncturing Matrix
                if (_fec->puncturing_matrix[r*(_fec->P)+p]) {
                	
                	// Not Punctured, output bit.
                    byte_out = (byte_out<<1) | parity(sr & _fec->poly[r]);
                    _msg_enc[n/8] = byte_out;

                    n++;
                }
            }
            // Update Puncturing Matrix "column" index.
            p = (p+1) % _fec->P;
        }
    }

    // Add tail bits
    for (int i = 0; i <_fec->K-1; i++) {

        // Shift Register: push zeros.
        sr = (sr << 1);

        // Compute parity bits for each polynomial.
        for (int r = 0; r < _fec->R; r++) {
        	// Check each parity bit against Puncturing Matrix.
            if (_fec->puncturing_matrix[r*(_fec->P)+p]) {
                byte_out = (byte_out<<1) | parity(sr & _fec->poly[r]);
                _msg_enc[n/8] = byte_out;
                n++;
            }
        }
        // Update Puncturing Matrix "column" index.
        p = (p+1) % _fec->P;
    }

    // Pad output to ensure even number of bytes.
    while (n%8) {
        // Shift zeros.
        byte_out <<= 1;
        _msg_enc[n/8] = byte_out;
        n++;
    }
}


void convolutional_punctured_decode(fec _fec, unsigned int _dec_msg_len, unsigned char *_msg_enc, unsigned char *_msg_dec)
{
    // re-allocate resources if necessary
    convolutional_punctured_setlength(_fec, _dec_msg_len);

    // unpack bytes, adding erasures at punctured indices
    unsigned int num_dec_bits = _fec->num_dec_bytes * 8 + _fec->K - 1;
    unsigned int num_enc_bits = num_dec_bits * _fec->R;
    unsigned int i,r;
    unsigned int n=0;   // input byte index
    unsigned int k=0;   // intput bit index (0<=k<8)
    unsigned int p=0;   // puncturing matrix column index
    unsigned char bit;
    unsigned char byte_in = _msg_enc[n];
    for (i=0; i<num_enc_bits; i+=_fec->R) {
        //
        for (r=0; r<_fec->R; r++) {
            if (_fec->puncturing_matrix[r*(_fec->P)+p]) {
                // push bit from input
                bit = (byte_in >> (7-k)) & 0x01;
                _fec->enc_bits[i+r] = bit ? 255 : 0;
                k++;
                if (k==8) {
                    k = 0;
                    n++;
                    byte_in = _msg_enc[n];
                }
            } else {
                // push erasure
                _fec->enc_bits[i+r] = 127;
            }
        }
        p = (p+1) % _fec->P;
    }

    // run decoder
    _fec->init_viterbi(_fec->vp,0);
    // TODO : check to see if this shouldn't be num_enc_bits (punctured)
    _fec->update_viterbi_blk(_fec->vp, _fec->enc_bits, 8*_fec->num_dec_bytes+_fec->K-1);
    _fec->chainback_viterbi(_fec->vp, _msg_dec, 8*_fec->num_dec_bytes, 0);
}


void convolutional_punctured_setlength(fec _fec, unsigned int _dec_msg_len)
{
    // re-allocate resources as necessary
    unsigned int num_dec_bytes = _dec_msg_len;

    // return if length has not changed
    if (num_dec_bytes == _fec->num_dec_bytes)
        return;

    // reset number of framebits
    _fec->num_dec_bytes = num_dec_bytes;
    _fec->num_enc_bytes = get_convolutional_msg_len(_fec, _dec_msg_len);

    // puncturing: need to expand to full length (decoder
    //             injects erasures at punctured values)
    unsigned int num_dec_bits = 8*_fec->num_dec_bytes;
    unsigned int n = num_dec_bits + _fec->K - 1;
    unsigned int num_enc_bits = n*(_fec->R);

    // delete old decoder if necessary
    if (_fec->vp != NULL)
        _fec->delete_viterbi(_fec->vp);

    // re-create / re-allocate memory buffers
    _fec->vp = _fec->create_viterbi(8*_fec->num_dec_bytes);
    _fec->enc_bits = (unsigned char*) realloc(_fec->enc_bits,
                                            num_enc_bits*sizeof(unsigned char));

}

// get the theoretical rate of a particular forward error-
// correction scheme (object-independent method)
float get_convolutional_rate(fec _fec)
{
    switch (_fec->scheme) {
        case CONV_25_P45_60:    return 45./60.;
        case CONV_25_P33_40:    return 33./40.;
    }
}

// compute encoded message length for convolutional codes
//  _fec			:	fec instance
//  _dec_msg_len    :   decoded message length
unsigned int get_convolutional_msg_len(fec _fec, unsigned int _dec_msg_len)
{
	// Convert Bytes to Bits
    unsigned int num_bits_in = _dec_msg_len*8;
    // Add Tail Bits.
    unsigned int n = num_bits_in + _fec->K - 1;
    // !! TEST CODE !!
    unsigned int num_bits_out = (n * _fec->R) * get_convolutional_rate(_fec);
    // Convert Bits to Bytes and Pad if needed.
    unsigned int num_bytes_out = num_bits_out/8 + (num_bits_out%8 ? 1 : 0);

    return num_bytes_out;
}


/* 
 * Internal initialization methods (sets r, K, viterbi methods, and puncturing matrix.
 */

int convolutional_v25_poly[2]  = {V25POLYA, V25POLYB};


void init_convolutional_v25(fec _fec)
{
	/* !! Matching Rate and Contraint required in viterbi.c !! */
    _fec->R=2;
    _fec->K=5;
    _fec->poly = convolutional_v25_poly;
    _fec->create_viterbi = create_viterbi;
    _fec->init_viterbi = init_viterbi;
    _fec->update_viterbi_blk = update_viterbi_blk;
    _fec->chainback_viterbi = chainback_viterbi;
    _fec->delete_viterbi = delete_viterbi;
}

void init_convolutional_v25_p45_60(fec _fec)
{
    init_convolutional_v25(_fec);

    _fec->P = 30;
    _fec->puncturing_matrix = conv25p45_60_matrix;
}

void init_convolutional_v25_p33_40(fec _fec)
{
    init_convolutional_v25(_fec);

    _fec->P = 20;
    _fec->puncturing_matrix = conv25p33_40_matrix;
}