/* tigertree.h
 * Copyright (C) 2001 Bitzi (aka Bitcollider) Inc. and Gordon Mohr
 * Released into the public domain by same; permission is explicitly
 * granted to copy, modify, and use freely.
 *
 * THE WORK IS PROVIDED "AS IS," AND COMES WITH ABSOLUTELY NO WARRANTY,
 * EXPRESS OR IMPLIED, TO THE EXTENT PERMITTED BY APPLICABLE LAW,
 * INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * (PD) 2001 The Bitzi Corporation
 * Please see file COPYING or http://bitzi.com/publicdomain 
 * for more info.
 *
 * $Id: TigerTree2.h,v 1.1 2004/04/06 04:45:39 swabby Exp $
 */
#pragma once

#include "tiger.h"

/* tiger hash result size, in bytes */
#define TIGERSIZE 24

/* size of each block independently tiger-hashed, not counting leaf 0x00 prefix */
#define BLOCKSIZE 1024

/* size of input to each non-leaf hash-tree node, not counting node 0x01 prefix */
#define NODESIZE (TIGERSIZE*2)

/* Byte size of lowest recorded node in tree */
#define TREE_RESOLUTION 128 * 1024

/* default size of interim values stack, in TIGERSIZE
 * blocks. If this overflows (as it will for input
 * longer than 2^64 in size), havoc may ensue. */
#define STACKSIZE TIGERSIZE*56

typedef struct tt2_context {
  word64 count;                   /* total blocks processed */
  byte   leaf[1+BLOCKSIZE]; /* leaf in progress */
  byte*  block;            /* leaf data */
  byte   node[1+NODESIZE]; /* node scratch space */
  int    index;                      /* index into block */
  byte*  top;             /* top (next empty) stack slot */
  byte   nodes[STACKSIZE]; /* stack of interim node values */

  byte* tree;
  uint64   treeSize;
  uint64   treeDepth;
  uint64   baseNodes;
  uint64   fileSize;

  tt2_context()
  {
	  tree = NULL;
  }

} TT2_CONTEXT;

void tt2_init(TT2_CONTEXT *ctx);
void tt2_update(TT2_CONTEXT *ctx, byte *buffer, uint64 len);
void tt2_digest(TT2_CONTEXT *ctx, byte *hash);
void tt2_copy(TT2_CONTEXT *dest, TT2_CONTEXT *src);
void tt2_initTree(TT2_CONTEXT *ctx, uint64 size);
static void tt2_block(TT2_CONTEXT *ctx);
static void tt2_compose(TT2_CONTEXT *ctx);
