/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * SHA-256 hash implementation
  * From http://bradconte.com/sha256_c.html
  */

#pragma once

// Signed variables are for wimps
#define uchar unsigned char // 8-bit byte
#define uint unsigned long // 32-bit word

typedef struct {
   uchar data[64];
   uint datalen;
   uint bitlen[2];
   uint state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, uchar data[], uint len);
void sha256_final(SHA256_CTX *ctx, uchar hash[]);
