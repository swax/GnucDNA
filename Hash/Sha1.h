
#pragma once


/* The SHA block size and message digest sizes, in bytes */
#define SHA_DATASIZE    64
#define SHA_DATALEN     16
#define SHA_DIGESTSIZE  20
#define SHA_DIGESTLEN    5


/* The structure for storing SHA info */
typedef struct sha_ctx {
  uint32 digest[SHA_DIGESTLEN];  /* Message digest */
  uint32 count_l, count_h;       /* 64-bit block count */
  byte   block[SHA_DATASIZE];    /* SHA data buffer */
  int    index;                  /* index into buffer */
} SHA_CTX;


void sha_init(struct sha_ctx *ctx);
void sha_update(struct sha_ctx *ctx, byte *buffer, uint32 len);
void sha_final(struct sha_ctx *ctx);
void sha_digest(struct sha_ctx *ctx, byte *s);
void sha_copy(struct sha_ctx *dest, struct sha_ctx *src);

CString Sha1toBase32(const byte *);
void	SubSha1toBase32(byte *out, const byte *in);


#if 1

#ifndef EXTRACT_UCHAR
#define EXTRACT_UCHAR(p)  (*(unsigned char *)(p))
#endif

#define STRING2INT(s) ((((((EXTRACT_UCHAR(s) << 8)    \
			 | EXTRACT_UCHAR(s+1)) << 8)  \
			 | EXTRACT_UCHAR(s+2)) << 8)  \
			 | EXTRACT_UCHAR(s+3))
#else
uint32 STRING2INT(word8 *s)
{
  uint32 r;
  int i;
  
  for (i = 0, r = 0; i < 4; i++, s++)
    r = (r << 8) | *s;
  return r;
}
#endif
