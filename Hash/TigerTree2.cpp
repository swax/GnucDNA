/* (PD) 2003 The Bitzi Corporation
 *
 * Copyright (C) 2001 Bitzi (aka Bitcollider) Inc. & Gordon Mohr
 * Released into the public domain by same; permission is explicitly
 * granted to copy, modify, and use freely.
 *
 * THE WORK IS PROVIDED "AS IS," AND COMES WITH ABSOLUTELY NO WARRANTY,
 * EXPRESS OR IMPLIED, TO THE EXTENT PERMITTED BY APPLICABLE LAW,
 * INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Please see file COPYING or http://bitzi.com/publicdomain 
 * for more info.
 *
 * tigertree.c - Implementation of the TigerTree algorithm
 *
 * Patterned after sha.c by A.M. Kuchling and others.
 *
 * To use:
 *    (1) allocate a TT_CONTEXT in your own code;
 *    (2) tt_init(ttctx);
 *    (3) tt_update(ttctx, buffer, length); as many times as necessary
 *    (4) tt_digest(ttctx,resultptr);
 *
 * NOTE: The TigerTree hash value cannot be calculated using a
 * constant amount of memory; rather, the memory required grows
 * with the (binary log of the) size of input. (Roughly, one more 
 * interim value must be remembered for each doubling of the 
 * input size.) This code reserves a counter and stack for input 
 * up to about 2^72 bytes in length. PASSING IN LONGER INPUT WILL 
 * LEAD TO A BUFFER OVERRUN AND UNDEFINED RESULTS. Of course,
 * that would be over 4.7 trillion gigabytes of data, so problems
 * are unlikely in practice anytime soon. :)
 *
 * Requires the tiger() function as defined in the reference
 * implementation provided by the creators of the Tiger
 * algorithm. See
 *
 *    http://www.cs.technion.ac.il/~biham/Reports/Tiger/
 *
 * $Id: TigerTree2.cpp,v 1.1 2004/04/06 04:45:39 swabby Exp $
 *
 */

#include "StdAfx.h"
#include "tigertree2.h"


/* Initialize the tigertree context */
void tt2_init(TT2_CONTEXT *ctx)
{
	ctx->count = 0;
	ctx->leaf[0] = 0; // flag for leaf  calculation -- never changed
	ctx->node[0] = 1; // flag for inner node calculation -- never changed
	ctx->block = ctx->leaf + 1 ; // working area for blocks
	ctx->index = 0;   // partial block pointer/block length
	ctx->top = ctx->nodes;

	if(ctx->tree)
	{
		delete [] ctx->tree;
		ctx->tree = NULL;
	}

	ctx->fileSize = 0;
	ctx->treeSize = 0;
	ctx->treeDepth = 0;
	ctx->baseNodes = 0;
}

void tt2_update(TT2_CONTEXT *ctx, byte *buffer, uint64 len)
{
	if (ctx->index)
	{ 
		/* Try to fill partial block */
		unsigned left = BLOCKSIZE - ctx->index;
		if (len < left)
		{
			memmove(ctx->block + ctx->index, buffer, len);
			ctx->index += len;
			return; /* Finished */
		}
		else
		{
			memmove(ctx->block + ctx->index, buffer, left);
			ctx->index = BLOCKSIZE;
			tt2_block(ctx);
			buffer += left;
			len -= left;
		}
	}

	while (len >= BLOCKSIZE)
	{
		memmove(ctx->block, buffer, BLOCKSIZE);
		ctx->index = BLOCKSIZE;
		tt2_block(ctx);
		buffer += BLOCKSIZE;
		len -= BLOCKSIZE;
	}

	if ((ctx->index = len))     /* This assignment is intended */
	{
		/* Buffer leftovers */
		memmove(ctx->block, buffer, len);
	}
}

static void tt2_block(TT2_CONTEXT *ctx)
{
	tiger((uint64*)ctx->leaf,(uint64)ctx->index+1,(uint64*)ctx->top);

	ctx->top += TIGERSIZE;
	++ctx->count;
	uint64 b = ctx->count;


	int    bSize   = BLOCKSIZE;      // Size of a node in current layer
	uint64 bTotal  = ctx->baseNodes; // Total nodes in current layer
	uint64 treePos = ctx->treeSize;  // Position of layer start in serialized tree


	// while evenly divisible by 2... // more iterations, more layers
	while(b == ((b >> 1)<<1) || b == bTotal) 
	{ 
		// Either really small file, or we're done
		if(b == 1 && b == bTotal)
		{
			memcpy(ctx->tree, ctx->nodes, TIGERSIZE);
			break;
		}
		
		// Not even means node needs to be moved up tree, not composed
		if( b == (b >> 1) << 1 )
		{
			tt2_compose(ctx);		
			b = b >> 1;
		}
		else
			b = (b >> 1) + 1;


		// Add to tiger tree
		bSize *= 2;
		bTotal = bTotal / 2 + (bTotal % 2);	
		
		if(bSize >= TREE_RESOLUTION)
		{
			uint64 treeOff =  (b-1) * 24;
			treePos -= bTotal * 24;

			memcpy(ctx->tree + treePos + treeOff, ctx->top - TIGERSIZE, TIGERSIZE);
		}	
	}
}

static void tt2_compose(TT2_CONTEXT *ctx) 
{
	byte *node = ctx->top - NODESIZE;
	memmove((ctx->node)+1,node,NODESIZE); // copy to scratch area
	tiger((uint64*)(ctx->node),(uint64)(NODESIZE+1),(uint64*)(ctx->top)); // combine two nodes

	memmove(node,ctx->top,TIGERSIZE);           // move up result
	ctx->top -= TIGERSIZE;                      // update top ptr
}

// no need to call this directly; tt_digest calls it for you
static void tt2_final(TT2_CONTEXT *ctx)
{
	// do last partial block, unless index is 1 (empty leaf)
	// AND we're past the first block
	if((ctx->index>0)||(ctx->top==ctx->nodes))
		tt2_block(ctx);
}

void tt2_initTree(TT2_CONTEXT *ctx, uint64 size)
{
	ctx->fileSize  = size;
	ctx->baseNodes = ctx->fileSize / BLOCKSIZE;

	if(ctx->fileSize % BLOCKSIZE > 0)
		ctx->baseNodes++;

	ctx->treeSize  = TIGERSIZE;
	ctx->treeDepth = 1;
	
	uint64 nodeCount  = ctx->baseNodes;
	int    nodeSize   = BLOCKSIZE;

	while(nodeCount > 1)
	{
		if( nodeSize >= TREE_RESOLUTION)
		{
			ctx->treeSize += nodeCount * TIGERSIZE;
			ctx->treeDepth++;
		}
		
		nodeCount = nodeCount / 2 + (nodeCount % 2);
		nodeSize *= 2;
	}

	ctx->tree = new byte[ctx->treeSize];
	memset(ctx->tree, 0, ctx->treeSize);
}

void tt2_digest(TT2_CONTEXT *ctx, byte *s)
{
	tt2_final(ctx);
	
	while( (ctx->top-TIGERSIZE) > ctx->nodes ) 
	{
		tt2_compose(ctx);
	}
	
	memmove(s,ctx->nodes,TIGERSIZE);
}

// this code untested; use at own risk
void tt2_copy(TT2_CONTEXT *dest, TT2_CONTEXT *src)
{
	int i;

	dest->count = src->count;
	for(i=0; i < BLOCKSIZE; i++)
		dest->block[i] = src->block[i];
	
	dest->index = src->index;
	for(i=0; i < STACKSIZE; i++)
		dest->nodes[i] = src->nodes[i];

	dest->top = src->top;
}
