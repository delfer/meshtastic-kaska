#ifndef _TINY_AES_H_
#define _TINY_AES_H_

#include <stddef.h>
#include <stdint.h>

#define AES_BLOCKLEN 16 // Block length in bytes - AES is 128b block only

// Принудительно настраиваем на AES-128
#define AES128 1
#undef AES256

#define AES_KEYLEN 16
#define AES_keyExpSize 176

struct AES_ctx {
    uint8_t RoundKey[AES_keyExpSize];
    uint8_t Iv[AES_BLOCKLEN];
};

void AES_init_ctx(struct AES_ctx *ctx, const uint8_t *key);
void AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t *key, const uint8_t *iv);
void AES_ctx_set_iv(struct AES_ctx *ctx, const uint8_t *iv);

void AES_CTR_xcrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, size_t length);

// Экспортируем Cipher для кастомной реализации CTR в Meshtastic
typedef uint8_t state_t[4][4];
void Cipher(state_t *state, const uint8_t *RoundKey);

#endif // _TINY_AES_H_
