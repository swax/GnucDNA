#include "stdafx.h"
#include "ed2k_md4.h"

unsigned int EDSEG_SIZE = 1024*9500; // 9728000

#define S11 3
#define S12 7
#define S13 11
#define S14 19
#define S21 3
#define S22 5
#define S23 9
#define S24 13
#define S31 3
#define S32 9
#define S33 11
#define S34 15

static void MD4Transform(w32 [4], const unsigned char [64]);
static void Encode(unsigned char *, w32 *, unsigned int);
static void Decode(w32 *, const unsigned char *, unsigned int);

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define FF(a, b, c, d, x, s) { \
    (a) += F ((b), (c), (d)) + (x); \
    (a) = ROTATE_LEFT ((a), (s)); \
  }
#define GG(a, b, c, d, x, s) { \
    (a) += G ((b), (c), (d)) + (x) + (w32)0x5a827999; \
    (a) = ROTATE_LEFT ((a), (s)); \
  }
#define HH(a, b, c, d, x, s) { \
    (a) += H ((b), (c), (d)) + (x) + (w32)0x6ed9eba1; \
    (a) = ROTATE_LEFT ((a), (s)); \
  }

void MD4Init(MD4_CTX *context)                                       
{
  context->count[0] = context->count[1] = 0;
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}

void MD4Update(MD4_CTX *context, const unsigned char *input, unsigned int inputLen)                     
{
  unsigned int i, index, partLen;

  index = (unsigned int)((context->count[0] >> 3) & 0x3F);

  if ((context->count[0] += ((w32)inputLen << 3))
      < ((w32)inputLen << 3))
    context->count[1]++;
  context->count[1] += ((w32)inputLen >> 29);

  partLen = 64 - index;

  if (inputLen >= partLen) 
  {
    memcpy(&context->buffer[index], input, partLen);
    MD4Transform (context->state, context->buffer);

    for (i = partLen; i + 63 < inputLen; i += 64)
      MD4Transform (context->state, &input[i]);

    index = 0;
  }
  else
    i = 0;

  memcpy(&context->buffer[index], &input[i], inputLen-i);
}

void MD4Final(unsigned char digest[16],MD4_CTX *context)
{
  unsigned char bits[8];
  unsigned int index, padLen;

  Encode (bits, context->count, 8);
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD4Update (context, PADDING, padLen);
  MD4Update (context, bits, 8);
  Encode (digest, context->state, 16);
  memset(context, 0, sizeof (*context));

}

static void MD4Transform(w32 state[4], const unsigned char block[64])
{
  w32 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode (x, block, 64); 

  FF (a, b, c, d, x[ 0], S11);  
  FF (d, a, b, c, x[ 1], S12);  
  FF (c, d, a, b, x[ 2], S13);  
  FF (b, c, d, a, x[ 3], S14);  
  FF (a, b, c, d, x[ 4], S11); 
  FF (d, a, b, c, x[ 5], S12); 
  FF (c, d, a, b, x[ 6], S13);
  FF (b, c, d, a, x[ 7], S14);
  FF (a, b, c, d, x[ 8], S11);
  FF (d, a, b, c, x[ 9], S12); 
  FF (c, d, a, b, x[10], S13); 
  FF (b, c, d, a, x[11], S14);
  FF (a, b, c, d, x[12], S11); 
  FF (d, a, b, c, x[13], S12); 
  FF (c, d, a, b, x[14], S13); 
  FF (b, c, d, a, x[15], S14); 

  GG (a, b, c, d, x[ 0], S21); 
  GG (d, a, b, c, x[ 4], S22); 
  GG (c, d, a, b, x[ 8], S23); 
  GG (b, c, d, a, x[12], S24); 
  GG (a, b, c, d, x[ 1], S21); 
  GG (d, a, b, c, x[ 5], S22); 
  GG (c, d, a, b, x[ 9], S23); 
  GG (b, c, d, a, x[13], S24); 
  GG (a, b, c, d, x[ 2], S21); 
  GG (d, a, b, c, x[ 6], S22); 
  GG (c, d, a, b, x[10], S23); 
  GG (b, c, d, a, x[14], S24); 
  GG (a, b, c, d, x[ 3], S21);
  GG (d, a, b, c, x[ 7], S22); 
  GG (c, d, a, b, x[11], S23);
  GG (b, c, d, a, x[15], S24); 

  HH (a, b, c, d, x[ 0], S31); 
  HH (d, a, b, c, x[ 8], S32); 
  HH (c, d, a, b, x[ 4], S33); 
  HH (b, c, d, a, x[12], S34); 
  HH (a, b, c, d, x[ 2], S31); 
  HH (d, a, b, c, x[10], S32); 
  HH (c, d, a, b, x[ 6], S33); 
  HH (b, c, d, a, x[14], S34); 
  HH (a, b, c, d, x[ 1], S31); 
  HH (d, a, b, c, x[ 9], S32); 
  HH (c, d, a, b, x[ 5], S33); 
  HH (b, c, d, a, x[13], S34); 
  HH (a, b, c, d, x[ 3], S31); 
  HH (d, a, b, c, x[11], S32); 
  HH (c, d, a, b, x[ 7], S33); 
  HH (b, c, d, a, x[15], S34); 

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;

  memset(x, 0, sizeof(x));
}

static void Encode(unsigned char *output,w32 *input,unsigned int len)
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
    output[j] = (unsigned char)(input[i] & 0xff);
    output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
    output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
    output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
  }
}

static void Decode(w32 *output, const unsigned char *input,unsigned int len)
{
	unsigned int i, j; 

  for (i = 0, j = 0; j < len; i++, j += 4)
    output[i] = ((w32)input[j]) | (((w32)input[j+1]) << 8) |
      (((w32)input[j+2]) << 16) | (((w32)input[j+3]) << 24);
}

void ED2KInit(ED2K_CTX *context)                                         
{
	context->nextPos = 0;
	MD4Init(&(context->seg_ctx));
	MD4Init(&(context->top_ctx));
}

void ED2KUpdate(ED2K_CTX *context, const unsigned char *input, unsigned int inputLen)                      
{
	unsigned int firstLen;
	unsigned char innerDigest[16];

	if(inputLen==0) return;

	if((context->nextPos > 0) && ((context->nextPos % EDSEG_SIZE)==0) )
	{

		MD4Final(innerDigest,&(context->seg_ctx));
		MD4Update(&(context->top_ctx),innerDigest,16);
		MD4Init(&(context->seg_ctx));
	}

	if((context->nextPos/EDSEG_SIZE)==(context->nextPos+inputLen)/EDSEG_SIZE)
	{
		MD4Update(&(context->seg_ctx),input,inputLen);
		context->nextPos += inputLen;
		return;
	}
	
	firstLen = EDSEG_SIZE-(context->nextPos % EDSEG_SIZE);
	MD4Update(&(context->seg_ctx),input,firstLen);
	context->nextPos += firstLen;

	ED2KUpdate(context,input+firstLen,inputLen-firstLen);
}

void ED2KFinal(ED2K_CTX *context, unsigned char digest[16])
{
	unsigned char innerDigest[16];

    if(context->nextPos <= EDSEG_SIZE) 
	{

		MD4Final(digest,&(context->seg_ctx));
		return;
	}

	MD4Final(innerDigest,&(context->seg_ctx));
	MD4Update(&(context->top_ctx),innerDigest,16);
    MD4Final(digest,&(context->top_ctx));
}


