
#pragma once

typedef unsigned int w32;

typedef struct 
{
  w32 state[4];                                   
  w32 count[2];         
  unsigned char buffer[64];                        
} MD4_CTX;

void MD4Init(MD4_CTX * );
void MD4Update(MD4_CTX *, const unsigned char *, unsigned int);
void MD4Final(unsigned char [16], MD4_CTX *);

typedef struct 
{
  MD4_CTX seg_ctx;    
  MD4_CTX top_ctx;   
  unsigned long nextPos;
} ED2K_CTX;

void ED2KInit(ED2K_CTX * );
void ED2KUpdate(ED2K_CTX *, const unsigned char *, unsigned int);
void ED2KFinal(ED2K_CTX *, unsigned char [16] );
