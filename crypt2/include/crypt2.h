/* Cryptographic Library for M17 Protocol */

#ifndef __CRYPT2_H__
#define __CRYPT2_H__


/****

		AES-CTR Encryption Library - Source: tiny-AES

****/

// AES256
#define AES_BLOCKLEN 16
#define AES_KEYLEN 32   // Key length in bytes
#define AES_keyExpSize 240

struct AES_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
  uint8_t Iv[AES_BLOCKLEN];
};

void aes256ctr_set_key(struct AES_ctx* ctx, const uint8_t* key);
void aes256ctr_xcrypt(struct AES_ctx* ctx, uint8_t* buf, const uint8_t* iv);


/****

		Random Number Generator

****/

void get_nonce(char *_nonce, unsigned int _nonce_len);
int get_m17_nonce(char *_nonce, const size_t _nonce_len);





#endif