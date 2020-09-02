//
// M17 TX Simulator.
//
// Reads raw audio file (8000Hz, 16bit MONO) 40ms at time, encodes with codec2
//
// M17 Packets are printed in the terminal. 
//

// Kernal Headers
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h> // Required for usleep(). Part of SIMULATE_REALTIME.


// M17 Headers
#include "codec2/src/codec2.h"
#include "fec2.h"
#include "crypt2.h"



// Debug
#define SIMULATE_REALTIME 0 // Add realtime delay to after the "Capture" of audio.

#define DEBUG_SETUP_FRAME 0
#define DEBUG_SUB_FRAME 0


// M17 Type Definitions
#define M17_PACKETMODE 0
#define M17_STREAMMODE 1
#define M17_VOICE 2
#define M17_DATA 1
#define M17_VOICEDATA 3
#define M17_ENC_AES 1
#define M17_ENC_SCRAMBLE 2
#define M17_SCRAMBLE_8 0
#define M17_SCRAMBLE_16 1
#define M17_SCRAMBLE_24 2
#define M17_NONE 0


struct m17_type_bf {
    unsigned int PacketStream:1;
    unsigned int DataType:2;
    unsigned int EncryptionType:2;
    unsigned int EncryptionSubType:2;
    unsigned int Reserved:9;
};

// M17 Sync Burst
const uint16_t m17_sync0 = 0x3243;
const uint16_t m17_sync1 = 0x3423;  // !! OUT OF SPEC !! Used on subframes for idetification by the decoder.

/*****

M17 Global Variables

*****/

// Radio ID
// TODO - Get Callsign from Settings File .
char *msg_src = "CYRUS";

// Recipient ID
// TODO - Get Destination Callsign/Group from "Current Channel" in Settings File.
char *msg_dst = "GROUP01";

// Random AES256 Key
int8_t aes_key[32] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                       0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 };

// Per Call (transmission) Variables

// Type
struct m17_type_bf m17_type;

// Nonce
char m17_nonce[14];

// Link Setup Frame (Link Information CHannel) - Contains Destination Address, Source Address, Type, NONCE. Tail not included in Serial transmission.
unsigned char m17_lich[30] = "0";

// Frame number - Starts from 0 and increments every frame. Used as a counter by CTR block cipher. Allows for a little over 43 minute transmissions.
uint16_t m17_fn = 0;



/*****

Internal Funtions

*****/

/* 
 * Encodes 9 character callsign into Base40 and returns 6 character array.
 * 
 * _dec_callsign   :   decoded callsign (9 Bytes)
 * _enc_callsign   :   encoded callsign (6 Bytes)
 * 
 * Accepted characters - "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.".
 */
void base40_callsign_encode(const char *_dec_callsign, char *_enc_callsign)
{
    // Get first 9 characters from _dec_callsign.
    char dec_callsign[9];
    memcpy(dec_callsign, _dec_callsign, 9);
    
    // Encode upto 9 characters (A-Z, 0-9, -, /, .) into Base40. Ignore invalid c
    uint64_t encoded_base40 = 0;

    for (const char *p = (dec_callsign + strlen(dec_callsign) - 1); p >= dec_callsign; p-- ) {
        
        encoded_base40 *= 40;
        
        if (*p >= 'A' && *p <= 'Z') // 1-26
            encoded_base40 += *p - 'A' + 1;
        
        else if (*p >= '0' && *p <= '9') // 27-36
            encoded_base40 += *p - '0' + 27;
        
        else if (*p == '-') // 37
            encoded_base40 += 37;
        
        else if (*p == '/') // 38
            encoded_base40 += 38;
        
        else if (*p == '.') // 39
            encoded_base40 += 39;
        
        else;
    }

    // Encode Base40 into a 6 byte char.
    char enc_callsign[6];

    for (int i = 0; i < 6; ++i)
    {
        enc_callsign[i] = (uint64_t)((encoded_base40 >> (40 - (8*i))) & 0xFF);
    }

    memcpy(_enc_callsign, enc_callsign, 6);
}

/* 
 * Decodes 6 character Base40 callsign and returns 9 character Callsign.
 * 
 * _enc_callsign   :   encoded callsign (6 Bytes)
 * _dec_callsign   :   decoded callsign (9 Bytes)
 */
void base40_callsign_decode(char *_enc_callsign, char *_dec_callsign)
{
	// Get first 6 characters from _enc_callsign.
    char enc_callsign[6];
    memcpy(enc_callsign, _enc_callsign, 6);

    uint64_t decoded_base40 = 0;

    for (int i = 0; i < 6; ++i)
    {
        decoded_base40 += (((uint64_t) enc_callsign[i] & 0xFF) << (40 - (8*i)));
    }

    char dec_callsign[9];
    memset(&dec_callsign[0], 0, sizeof(dec_callsign));

    if (decoded_base40 >= 262144000000000) // If base40 is larger than or equal to 40^9, return 0;
    { 
        *dec_callsign = 0;
    }
    else
    {
	    char *p = dec_callsign;
	    
	    for (; decoded_base40 > 0; p++) {
	        *p = "xABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."[decoded_base40 % 40];
	        decoded_base40 /= 40;
	    }

	    *p = 0;
	}

	memcpy(_dec_callsign, dec_callsign, 9);
}

// Converts Bitfields to Unsigned Short.
uint16_t get_m17_type()
{
    uint16_t _type;

    _type = m17_type.PacketStream << 15;
    _type |= m17_type.DataType << 13;
    _type |= m17_type.EncryptionType << 11;
    _type |= m17_type.EncryptionSubType << 9;
    _type |= m17_type.Reserved;

    return _type;

}

//DEBUG - Prints array as 1 Byte HEX
void print_char_hex(char *_in, int _len)
{
	for (int i = 0; i<_len; i++)
	{
	    printf("%02X", _in[i] & 0xFF);

	    if (i < (_len - 1)) printf("-");
	}
}

/* 
 * Decodes 6 character Base40 callsign and returns 9 character Callsign.
 * 
 * _enc_callsign   :   encoded callsign (6 Bytes)
 * _dec_callsign   :   decoded callsign (9 Bytes)
 */
void m17_transmit_setup_frame()
{
	// Variables
    uint16_t type = get_m17_type();
	char enc_dst_callsign[6];
	char enc_src_callsign[6];

	// Encode Callsigns
	base40_callsign_encode(msg_dst, enc_dst_callsign);
	base40_callsign_encode(msg_src, enc_src_callsign);

	// Clear and Generate Nonce
    memset(&m17_nonce[0], 0, sizeof(m17_nonce));
	get_m17_nonce(m17_nonce, sizeof(m17_nonce));

#if DEBUG_SETUP_FRAME
    printf("Nonce: ");
    print_char_hex(m17_nonce, sizeof(m17_nonce));
    printf("\n");
#endif

    // Clear LICH
	memset(&m17_lich[0], 0, sizeof(m17_lich));

	// Assemble LICH without CRC
    memcpy(m17_lich, enc_dst_callsign, 6);
    memcpy(m17_lich + 6, enc_src_callsign, 6);
    m17_lich[12] = (uint64_t)((type >> 8) & 0xFF);
    m17_lich[13] = (uint64_t)((type) & 0xFF);
    memcpy(m17_lich + 14, m17_nonce, 14);

#if DEBUG_SETUP_FRAME
    printf ("LICH w/o CRC: ");
    print_char_hex(m17_lich, 30);
    printf("\n");
#endif

    // Get CRC for LICH
    uint16_t lich_crc;
    lich_crc = crc16_m17(m17_lich, 28);

#if DEBUG_SETUP_FRAME
    printf ("LICH CRC: %04X\n", lich_crc);
#endif

    // Add CRC to LICH
    m17_lich[28] = (uint64_t)((lich_crc >> 8) & 0xFF);
    m17_lich[29] = (uint64_t)((lich_crc) & 0xFF);

#if DEBUG_SETUP_FRAME
    printf("LICH Type 1:  ");
	print_char_hex(m17_lich, sizeof(m17_lich));
	printf(" (Length: %ld)\n", sizeof(m17_lich));
#endif

    // Encode LICH - Viterbi R=1/2 K=5 Punctured 45/60
    fec q1 = convolutional_punctured_create(CONV_25_P45_60);            // Create fec object

    unsigned int n_enc = get_convolutional_msg_len(q1,sizeof(m17_lich));
    unsigned char encoded_lich[n_enc];
    
    convolutional_punctured_encode(q1,sizeof(m17_lich),m17_lich,encoded_lich);	// Encode message. Return 41 Bytes. Padded to 46 Bytes after interleaving

#if DEBUG_SETUP_FRAME
    printf("LICH Type 3:  ");
	print_char_hex(encoded_lich, n_enc);
	printf(" (Length: %d)\n", n_enc);
#endif
	
    // Interleave LICH
    interleaver q2 = interleaver_create(n_enc);                         // Create interleaver object

    unsigned char interleaved_lich[n_enc];
    interleaver_encode(q2,encoded_lich,interleaved_lich);
    
#if DEBUG_SETUP_FRAME
    printf("LICH Type 4:  ");
	print_char_hex(interleaved_lich, n_enc);
	printf("\n");


	// Test De-interleaving and Decoding
    unsigned char deinterleaved_lich[n_enc];
    interleaver_decode(q2,interleaved_lich,deinterleaved_lich);

    unsigned char msg_dec[sizeof(m17_lich)];
    convolutional_punctured_decode(q1,sizeof(m17_lich),deinterleaved_lich,msg_dec);

    printf("LICH Type 1:  ");
	print_char_hex(msg_dec, sizeof(m17_lich));
	printf(" (Decoded)\n");
#endif
    
    // Clean up
    convolutional_punctured_destroy(q1);                                // Destroy fec object
    interleaver_destroy(q2);                                            // Destroy interleaver object

    // Create Setup Frame
    unsigned char setup_frame[48];
    memset(&setup_frame[0], 0, sizeof(setup_frame));

    // Add SYNC and Type 4 Payload
    setup_frame[0] = (uint64_t)((m17_sync0 >> 8) & 0xFF);
    setup_frame[1] = (uint64_t)((m17_sync0) & 0xFF);
    memcpy(setup_frame + 2, interleaved_lich, n_enc);

    // "Transmit" LICH
    printf ("Setup Frame:    ");
    print_char_hex(setup_frame, 48);
	printf("\n");

}


void m17_transmit_sub_frame(char *_payload)
{
	// Variables
	int lich_chunk_len = 6;
	unsigned char lich_chunk[6];
	int sub_frame_chunk_len = 20;

	// Get LICH Chunk based on Frame Number
    int fi = m17_fn % 5; 						// Frame Iteration. 0-4.
    memcpy(lich_chunk, m17_lich + (6*fi), 6);

    unsigned int enc_lich_chunk_len = fec_golay_get_enc_msg_len(lich_chunk_len); // Redundant? Fixed Values...
    unsigned char enc_lich_chunk[enc_lich_chunk_len];
    
    // Encode LICH Chunk
    fec_golay2412_encode(lich_chunk_len, lich_chunk, enc_lich_chunk);


#if DEBUG_SUB_FRAME
    printf ("LICH Chunk: ");
    print_char_hex(lich_chunk, lich_chunk_len);
	printf("\n");

	printf ("Encoded LICH Chunk: ");
    print_char_hex(enc_lich_chunk, enc_lich_chunk_len);
	printf("\n");

	unsigned char dec_lich_chunk[enc_lich_chunk_len];
	fec_golay2412_decode(lich_chunk_len, enc_lich_chunk, dec_lich_chunk);

	printf ("Decoded LICH Chunk: ");
    print_char_hex(dec_lich_chunk, lich_chunk_len);
	printf("\n");
#endif

	// Assemble LICH Chunk, Frame Number and Payload for CRC Calculation
	unsigned char crc_sub_frame_chunk[24];
	memcpy(crc_sub_frame_chunk, lich_chunk, 6);
    crc_sub_frame_chunk[6] = (uint64_t)((m17_fn >> 8) & 0xFF);
    crc_sub_frame_chunk[7] = (uint64_t)((m17_fn) & 0xFF);
    memcpy(crc_sub_frame_chunk + 8, _payload, 16);

#if DEBUG_SUB_FRAME
    printf ("CRC Sub Frame Chunk: ");
    print_char_hex(crc_sub_frame_chunk, 24);
	printf("\n");
#endif

	// Get CRC for crc_sub_frame_chunk
	uint16_t sub_frame_crc;
    sub_frame_crc = crc16_m17(crc_sub_frame_chunk, 24);

#if DEBUG_SUB_FRAME
    printf ("Sub Frame Chunk CRC: %04X\n", sub_frame_crc);
#endif

    // Assemble Frame Number, Payload and CRC
    unsigned char sub_frame_chunk[20];
    sub_frame_chunk[0] = (uint64_t)((m17_fn >> 8) & 0xFF);
    sub_frame_chunk[1] = (uint64_t)((m17_fn) & 0xFF);
    memcpy(sub_frame_chunk + 2, _payload, 16);
    sub_frame_chunk[18] = (uint64_t)((sub_frame_crc >> 8) & 0xFF);
    sub_frame_chunk[19] = (uint64_t)((sub_frame_crc) & 0xFF);

#if DEBUG_SUB_FRAME
    printf ("Sub Frame Chunk Type 1: ");
    print_char_hex(sub_frame_chunk, 20);
	printf("\n");
#endif

    // Encode LICH - Viterbi R=1/2 K=5 Punctured 33/40
    fec q1 = convolutional_punctured_create(CONV_25_P33_40);// Create fec object

    unsigned int n_enc = get_convolutional_msg_len(q1,sizeof(sub_frame_chunk));
    unsigned char encoded_sub_frame_chunk[n_enc];
    
    convolutional_punctured_encode(q1,sizeof(sub_frame_chunk),sub_frame_chunk,encoded_sub_frame_chunk); // Encode message. Return 32 Bytes. Padded to 34 Bytes after interleaving


#if DEBUG_SUB_FRAME
    printf("Sub Frame Chunk Type 3: ");
	print_char_hex(encoded_sub_frame_chunk, n_enc);
	printf(" (Length: %d)\n", n_enc);
#endif
	
    // Interleave Sub Frame Chunk
    interleaver q2 = interleaver_create(n_enc);				// Create interleaver object

    unsigned char interleaved_sub_frame_chunk[n_enc];
    interleaver_encode(q2,encoded_sub_frame_chunk,interleaved_sub_frame_chunk);
    
#if DEBUG_SUB_FRAME
    printf("Sub Frame Chunk Type 4: ");
	print_char_hex(interleaved_sub_frame_chunk, n_enc);
	printf("\n");


	// Test De-interleaving and Decoding
    unsigned char deinterleaved_sub_frame_chunk[n_enc];
    interleaver_decode(q2,interleaved_sub_frame_chunk,deinterleaved_sub_frame_chunk);

    unsigned char msg_dec[sizeof(sub_frame_chunk)];
    convolutional_punctured_decode(q1,sizeof(sub_frame_chunk),deinterleaved_sub_frame_chunk,msg_dec);

    printf("Sub Frame Chunk Type 1: ");
	print_char_hex(msg_dec, sizeof(sub_frame_chunk));
	printf(" (Decoded)\n");
#endif

    // Clean up
    convolutional_punctured_destroy(q1); 					// Destroy fec object
    interleaver_destroy(q2);								// Destroy interleaver object

	// Create Sub Frame - SYNC (2 Bytes), LICH Chunk (12 Bytes) and Payload (34 Bytes) 
    unsigned char sub_frame[48];
    memset(&sub_frame[0], 0, sizeof(sub_frame));

    // Add SYNC and Type 4 Payload
    sub_frame[0] = (uint64_t)((m17_sync1 >> 8) & 0xFF);
    sub_frame[1] = (uint64_t)((m17_sync1) & 0xFF);
    memcpy(sub_frame + 2, enc_lich_chunk, enc_lich_chunk_len);
    memcpy(sub_frame + 14, interleaved_sub_frame_chunk, n_enc);

	// "Transmit" Sub Frame
    printf ("Sub Frame (%02d): ", m17_fn);
    print_char_hex(sub_frame, 48);
	printf("\n");

	// Increment Frame Number
	m17_fn++;
}


void transmit_voice_stream()
{
    // Set Message Options
    m17_type.PacketStream = M17_STREAMMODE;
    m17_type.DataType = M17_VOICE;

    // "Transmit" Setup Frame
    m17_transmit_setup_frame();

    // Encryption Variables
    struct AES_ctx ctx;
    char iv[16];

    // Setup Encryption
    if (m17_type.EncryptionType == M17_ENC_AES)
    {
        printf("AES Encryption Required.\n");
        // Load Key & Nonce
        aes256ctr_set_key(&ctx, aes_key);
        memcpy(iv, m17_nonce, 14);
    }


    FILE *fptr;

    // Create Codec2 Object
    int mode = CODEC2_MODE_3200;
    void *codec2 = codec2_create(mode);
    codec2_set_natural_or_gray(codec2, 1); // Set Gray

    short *buf;
    unsigned char *bits;
    int nsam, nbit, nbyte;

    nsam = codec2_samples_per_frame(codec2);    // 160
    nbit = codec2_bits_per_frame(codec2);       // 64
    buf = (short*)malloc(nsam*sizeof(short));   // 320 Bytes
    nbyte = (nbit + 7) / 8;                     // 8

    bits = (unsigned char*)malloc(nbyte*sizeof(char));

    // Load file.
    if ( (fptr = fopen("raw/hts1a.raw","rb")) == NULL ) {

        printf("Error opening audio file.\n");
        exit(1);
    }

    char codec2_3200_payload[16];

    // Used to alternate writing to the first half or second half of codec2_3200_payload. codec2_encode (3200) returns 8 bytes at time.
    uint16_t write_count = 0;

    // Read File
    while(fread(buf, sizeof(short), nsam, fptr) == (size_t)nsam) {

        codec2_encode(codec2, bits, buf);

        if (!(write_count % 2))
        {
            memcpy(codec2_3200_payload, bits, 8);
        }
        else
        {
            memcpy(codec2_3200_payload + 8, bits, 8);

            // Delay thread for 40ms to simulate the realtime capturing of audio.
            if (SIMULATE_REALTIME) usleep(40000);

            if (m17_type.EncryptionType == M17_ENC_AES)
            {
                // Add current Frame Number to IV and encrypt Payload
                iv[14] = (uint64_t)((m17_fn >> 8) & 0xFF);
                iv[15] = (uint64_t)((m17_fn) & 0xFF);
                aes256ctr_xcrypt(&ctx, codec2_3200_payload, iv);
            }

            // "Transmit" Sub Frame
            m17_transmit_sub_frame(codec2_3200_payload);
        }

        write_count++;
    }

    codec2_destroy(codec2);


}


int main (void) {

	printf ("Program Started.\n");

    // Set Encryption Parameter for M17 Type.
    m17_type.EncryptionType = M17_ENC_AES;
    m17_type.EncryptionSubType = M17_NONE;
    m17_type.Reserved = M17_NONE;

    transmit_voice_stream();

	

	printf ("Program Finished.\n");
}

