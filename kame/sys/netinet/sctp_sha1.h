/*	$KAME: sctp_sha1.h,v 1.3 2002/06/09 16:29:55 itojun Exp $	*/

#ifndef __SCTP_SLA1_h__
#define __SCTP_SLA1_h__
/*
 * Copyright (c) 2001, 2002 Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Cisco Systems, Inc.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CISCO SYSTEMS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CISCO SYSTEMS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>

struct sha1_context {
	unsigned int A;
	unsigned int B;
	unsigned int C;
	unsigned int D;
	unsigned int E;
	unsigned int H0;
	unsigned int H1;
	unsigned int H2;
	unsigned int H3;
	unsigned int H4;
	unsigned int words[80];
	unsigned int TEMP;
	/* block I am collecting to process */
	char sha_block[64];
	/* collected so far */
	int how_many_in_block;
	unsigned int running_total;
};


#define F1(B,C,D) (((B & C) | ((~B) & D)))	/*  0  <= t <= 19 */
#define F2(B,C,D) (B ^ C ^ D)			/*  20 <= t <= 39 */
#define F3(B,C,D) ((B & C) | (B & D) | (C & D))	/*  40 <= t <= 59 */
#define F4(B,C,D) (B ^ C ^ D)			/* 600 <= t <= 79 */

/* circular shift */
#define CSHIFT(A,B) ((B << A) | (B >> (32-A)))

#define K1 0x5a827999       /* 0  <= t <= 19 */
#define K2 0x6ed9eba1       /* 20 <= t <= 39 */
#define K3 0x8f1bbcdc       /* 40 <= t <= 59 */
#define K4 0xca62c1d6       /* 60 <= t <= 79 */

#define H0INIT 0x67452301
#define H1INIT 0xefcdab89
#define H2INIT 0x98badcfe
#define H3INIT 0x10325476
#define H4INIT 0xc3d2e1f0

#ifdef _KERNEL

void SHA1_Init(struct sha1_context *ctx);
void SHA1_Process(struct sha1_context *ctx, unsigned char *ptr, int siz);
void SHA1_Final(struct sha1_context *ctx, unsigned char *digest);

#endif /* _KERNEL */
#endif
