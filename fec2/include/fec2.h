/* Encoder/Decoder Library for M17 Protocol */

#ifndef __FEC2_H__
#define __FEC2_H__

/****

		Global Variables

****/


/* Viterbi (1/2 Rate, K=5) M17 Protocol Polnomials */
#define	V25POLYA	0x13 // 19
#define	V25POLYB	0x1D // 29


/****

		Golay(24,12) half-rate forward error-correction code - Source: liquid-dsp

****/

/* 
 * Returns length of data if encoded with Golay(24,12) based on decoded message length (number of bytes). 
 */
unsigned int fec_golay_get_enc_msg_len(unsigned int _dec_msg_len);

/* 
 * Encodes a block of data using Golay(24,12) encoder.
 * 
 * _dec_msg_len    :   decoded message length (number of bytes)
 * _msg_dec        :   decoded message [size: 1 x _dec_msg_len]
 * _msg_enc        :   encoded message [size: 1 x 2*_dec_msg_len]
 * 
 * Encoded Message will be padded if required.
 */
void fec_golay2412_encode(unsigned int _dec_msg_len, unsigned char *_msg_dec, unsigned char *_msg_enc);

/* 
 * Decodes a block of Golay(24,12) encoded data.
 * 
 * _dec_msg_len    :   decoded message length (number of bytes)
 * _msg_enc        :   encoded message [size: 1 x 2*_dec_msg_len]
 * _msg_dec        :   decoded message [size: 1 x _dec_msg_len]
 */
void fec_golay2412_decode(unsigned int _dec_msg_len, unsigned char *_msg_enc, unsigned char *_msg_dec);



/****

		CRC16 Error-detecting code - Source: avr-libc

****/

/* 
 * Returns 16 bit CRC value of data.
 * 
 * _msg            :   message
 * _msg_len        :   data length (number of bytes)
 */
unsigned short crc16_m17(unsigned char *_msg, unsigned int _msg_len);



/****

		Interleaver - Source: liquid-dsp

****/

typedef struct interleaver_s * interleaver;

/* 
 * Create interleaver of length _n.
 * 
 * _n              :   input/output bytes
 */
interleaver interleaver_create(unsigned int _n);

/* 
 * Destroy interleaver object.
 */
void interleaver_destroy(interleaver _q);


/*
 * Print interleaver object internals.
 */
void interleaver_print(interleaver _q);

/* 
 * Set depth (number of internal iterations).
 * 
 * _q              :   interleaver object
 * _depth          :   depth object
 */
void interleaver_set_depth(interleaver _q, unsigned int _depth);

/* 
 * Execute forward interleaver (encoder).
 * 
 * _q              :   interleaver object
 * _msg_dec        :   decoded (un-interleaved) message
 * _msg_enc        :   encoded (interleaved) message
 * 
 * Encoded Message will be padded if required.
 */
void interleaver_encode(interleaver _q, unsigned char * _msg_dec, unsigned char * _msg_enc);

/* 
 * Execute forward interleaver (encoder) on soft bits.
 * 
 * _q              :   interleaver object
 * _msg_dec        :   decoded (un-interleaved) message
 * _msg_enc        :   encoded (interleaved) message
 * 
 * Encoded Message will be padded if required.
 */
void interleaver_encode_soft(interleaver _q, unsigned char * _msg_dec, unsigned char * _msg_enc);

/* 
 * Execute reverse interleaver (decoder).
 * 
 * _q              :   interleaver object
 * _msg_enc        :   encoded (interleaved) message
 * _msg_dec        :   decoded (un-interleaved) message
 * 
 * Encoded Message will be padded if required.
 */
void interleaver_decode(interleaver _q, unsigned char * _msg_enc, unsigned char * _msg_dec);

/* 
 * Execute reverse interleaver (decoder) on soft bits.
 * 
 * _q              :   interleaver object
 * _msg_enc        :   encoded (interleaved) message
 * _msg_dec        :   decoded (un-interleaved) message
 * 
 * Encoded Message will be padded if required.
 */
void interleaver_decode_soft(interleaver _q, unsigned char * _msg_enc, unsigned char * _msg_dec);




/****

		Viterbi Convolution Encode/Decoder - Source: libfec

****/

/*
 * Create a new instance of a Viterbi decoder.
 */
void *create_viterbi(int len);
//void *create_viterbi27(int len);

/*
 * Set polynomials.
 */
void set_viterbi_polynomial(int polys[2]);
//void set_viterbi27_polynomial(int polys[2]);

/*
 * Initialize Viterbi decoder for start of new frame.
 */
int init_viterbi(void *vp, int starting_state);
//int init_viterbi27(void *vp, int starting_state);

/*
 * Update decoder with a block of demodulated symbols.
 */
int update_viterbi_blk(void *vp, unsigned char sym[],int npairs);
//int update_viterbi27_blk(void *vp, unsigned char sym[],int npairs);

/*
 * Viterbi chainback.
 */
int chainback_viterbi(void *vp, unsigned char *data,unsigned int nbits,unsigned int endstate);
//int chainback_viterbi27(void *vp, unsigned char *data,unsigned int nbits,unsigned int endstate);

/*
 * Delete instance of a Viterbi decoder.
 */
void delete_viterbi(void *vp);
//void delete_viterbi27(void *vp);

/* 
 * Create 256-entry odd-parity lookup table. Only needed for non-ia32 machines.
 */
void partab_init();

static inline int parityb(unsigned char x){

	extern unsigned char Partab[256];
	extern int P_init;

	if(!P_init) {
		partab_init();
	}

	return Partab[x];
}

static inline int parity(int x){

	/* Fold down to one byte */
	x ^= (x >> 16);
	x ^= (x >> 8);

	return parityb(x);
}



/****

		Punctured Convolution Encode/Decoder - Source: liquid-dsp

****/

typedef enum {
    // M17 Punctured Codes
    CONV_25_P45_60,     
    CONV_25_P33_40
} fec_scheme;


// fec : basic object
struct fec_s {
    // common
    fec_scheme scheme;
    float rate;

    // lengths: convolutional
    unsigned int num_dec_bytes;
    unsigned int num_enc_bytes;

    // convolutional : internal memory structure
    unsigned char * enc_bits;
    void * vp;      // decoder object
    int * poly;     // polynomial
    unsigned int R; // primitive rate, inverted (e.g. R=3 for 1/3)
    unsigned int K; // constraint length
    unsigned int P; // puncturing rate (e.g. p=3 for 3/4)
    int * puncturing_matrix;

    // viterbi decoder function pointers
    void*(*create_viterbi)(int);
    int  (*init_viterbi)(void*,int);
    int  (*update_viterbi_blk)(void*,unsigned char*,int);
    int  (*chainback_viterbi)(void*,unsigned char*,unsigned int,unsigned int);
    void (*delete_viterbi)(void*);

};

// fec object (pointer to fec structure)
typedef struct fec_s * fec;

float get_convolutional_rate(fec _fec);
unsigned int get_convolutional_msg_len(fec _fec, unsigned int _dec_msg_len);


// punctured convolutional codes
fec convolutional_punctured_create(fec_scheme _fs);
void convolutional_punctured_destroy(fec _fec);
void convolutional_punctured_encode(fec _fec, unsigned int _dec_msg_len, unsigned char *_msg_dec, unsigned char *_msg_enc);
void convolutional_punctured_decode(fec _fec, unsigned int _dec_msg_len, unsigned char *_msg_enc, unsigned char *_msg_dec);
void convolutional_punctured_setlength(fec _fec, unsigned int _dec_msg_len);

/* 
 * Internal initialization methods. Sets r, K, viterbi methods, and puncturing matrix.
 */
void init_convolutional_v25(fec _fec);
void init_convolutional_v25_p45_60(fec _fec);
void init_convolutional_v25_p33_40(fec _fec);

// Convolutional Puncturing Matrices       [R x P]
extern int conv25p45_60_matrix[60];     // [2 x 30]
extern int conv25p33_40_matrix[40];     // [2 x 20]

#endif