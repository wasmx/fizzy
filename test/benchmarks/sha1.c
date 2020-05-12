// From https://github.com/rhash/rhash

/* sha1.c - an implementation of Secure Hash Algorithm 1 (SHA1)
 * based on RFC 3174.
 *
 * Copyright (c) 2008, Aleksey Kravchenko <rhash.admin@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE  INCLUDING ALL IMPLIED WARRANTIES OF  MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT,  OR CONSEQUENTIAL DAMAGES  OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,  NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF  OR IN CONNECTION  WITH THE USE  OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <inttypes.h>
#include <string.h>

#define WASM_EXPORT __attribute__((visibility("default")))

/* ROTL/ROTR macros rotate a 32/64-bit word left/right by n bits */
#define ROTL32(dword, n) ((dword) << (n) ^ ((dword) >> (32 - (n))))
#define ROTR32(dword, n) ((dword) >> (n) ^ ((dword) << (32 - (n))))

#define IS_ALIGNED_32(p) (0 == (3 & ((const char*)(p) - (const char*)0)))

#if __BYTE_ORDER != __LITTLE_ENDIAN
#warning Only little endian supported.
#endif

#define be2me_32(x) __builtin_bswap32(x)
#define be32_copy(to, index, from, length) rhash_swap_copy_str_to_u32((to), (index), (from), (length))

void rhash_swap_copy_str_to_u32(void* to, int index, const void* from, size_t length)
{
    /* if all pointers and length are 32-bits aligned */
    if ( 0 == (( (int)((char*)to - (char*)0) | ((char*)from - (char*)0) | index | length ) & 3) ) {
        /* copy memory as 32-bit words */
        const uint32_t* src = (const uint32_t*)from;
        const uint32_t* end = (const uint32_t*)((const char*)src + length);
        uint32_t* dst = (uint32_t*)((char*)to + index);
        for (; src < end; dst++, src++)
            *dst = __builtin_bswap32(*src);
    } else {
        const char* src = (const char*)from;
        for (length += index; (size_t)index < length; index++)
            ((char*)to)[index ^ 3] = *(src++);
    }
}

#define sha1_block_size 64
#define sha1_hash_size  20

/* algorithm context */
typedef struct sha1_ctx
{
        unsigned char message[sha1_block_size]; /* 512-bit buffer for leftovers */
        uint64_t length;   /* number of processed bytes */
        unsigned hash[5];  /* 160-bit algorithm internal hashing state */
} sha1_ctx;

/**
 * Initialize context before calculaing hash.
 *
 * @param ctx context to initialize
 */
static void rhash_sha1_init(sha1_ctx* ctx)
{
	ctx->length = 0;

	/* initialize algorithm state */
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->hash[4] = 0xc3d2e1f0;
}

/**
 * The core transformation. Process a 512-bit block.
 * The function has been taken from RFC 3174 with little changes.
 *
 * @param hash algorithm state
 * @param block the message block to process
 */
static void rhash_sha1_process_block(unsigned* hash, const unsigned* block)
{
	int           t;                 /* Loop counter */
	uint32_t      temp;              /* Temporary word value */
	uint32_t      W[80];             /* Word sequence */
	uint32_t      A, B, C, D, E;     /* Word buffers */

	/* initialize the first 16 words in the array W */
	for (t = 0; t < 16; t++) {
		/* note: it is much faster to apply be2me here, then using be32_copy */
		W[t] = be2me_32(block[t]);
	}

	/* initialize the rest */
	for (t = 16; t < 80; t++) {
		W[t] = ROTL32(W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16], 1);
	}

	A = hash[0];
	B = hash[1];
	C = hash[2];
	D = hash[3];
	E = hash[4];

	for (t = 0; t < 20; t++) {
		/* the following is faster than ((B & C) | ((~B) & D)) */
		temp =  ROTL32(A, 5) + (((C ^ D) & B) ^ D)
			+ E + W[t] + 0x5A827999;
		E = D;
		D = C;
		C = ROTL32(B, 30);
		B = A;
		A = temp;
	}

	for (t = 20; t < 40; t++) {
		temp = ROTL32(A, 5) + (B ^ C ^ D) + E + W[t] + 0x6ED9EBA1;
		E = D;
		D = C;
		C = ROTL32(B, 30);
		B = A;
		A = temp;
	}

	for (t = 40; t < 60; t++) {
		temp = ROTL32(A, 5) + ((B & C) | (B & D) | (C & D))
			+ E + W[t] + 0x8F1BBCDC;
		E = D;
		D = C;
		C = ROTL32(B, 30);
		B = A;
		A = temp;
	}

	for (t = 60; t < 80; t++) {
		temp = ROTL32(A, 5) + (B ^ C ^ D) + E + W[t] + 0xCA62C1D6;
		E = D;
		D = C;
		C = ROTL32(B, 30);
		B = A;
		A = temp;
	}

	hash[0] += A;
	hash[1] += B;
	hash[2] += C;
	hash[3] += D;
	hash[4] += E;
}

/**
 * Calculate message hash.
 * Can be called repeatedly with chunks of the message to be hashed.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param msg message chunk
 * @param size length of the message chunk
 */
static void rhash_sha1_update(sha1_ctx* ctx, const unsigned char* msg, size_t size)
{
	unsigned index = (unsigned)ctx->length & 63;
	ctx->length += size;

	/* fill partial block */
	if (index) {
		unsigned left = sha1_block_size - index;
		__builtin_memcpy(ctx->message + index, msg, (size < left ? size : left));
		if (size < left) return;

		/* process partial block */
		rhash_sha1_process_block(ctx->hash, (unsigned*)ctx->message);
		msg  += left;
		size -= left;
	}
	while (size >= sha1_block_size) {
		unsigned* aligned_message_block;
		if (IS_ALIGNED_32(msg)) {
			/* the most common case is processing of an already aligned message
			without copying it */
			aligned_message_block = (unsigned*)msg;
		} else {
			__builtin_memcpy(ctx->message, msg, sha1_block_size);
			aligned_message_block = (unsigned*)ctx->message;
		}

		rhash_sha1_process_block(ctx->hash, aligned_message_block);
		msg  += sha1_block_size;
		size -= sha1_block_size;
	}
	if (size) {
		/* save leftovers */
		__builtin_memcpy(ctx->message, msg, size);
	}
}

/**
 * Store calculated hash into the given array.
 *
 * @param ctx the algorithm context containing current hashing state
 * @param result calculated hash in binary form
 */
static void rhash_sha1_final(sha1_ctx* ctx, unsigned char* result)
{
	unsigned  index = (unsigned)ctx->length & 63;
	unsigned* msg32 = (unsigned*)ctx->message;

	/* pad message and run for last block */
	ctx->message[index++] = 0x80;
	while ((index & 3) != 0) {
		ctx->message[index++] = 0;
	}
	index >>= 2;

	/* if no room left in the message to store 64-bit message length */
	if (index > 14) {
		/* then fill the rest with zeros and process it */
		while (index < 16) {
			msg32[index++] = 0;
		}
		rhash_sha1_process_block(ctx->hash, msg32);
		index = 0;
	}
	while (index < 14) {
		msg32[index++] = 0;
	}
	msg32[14] = be2me_32( (unsigned)(ctx->length >> 29) );
	msg32[15] = be2me_32( (unsigned)(ctx->length << 3) );
	rhash_sha1_process_block(ctx->hash, msg32);

	if (result) be32_copy(result, 0, &ctx->hash, sha1_hash_size);
}

WASM_EXPORT unsigned sha1_bench(unsigned input_len, unsigned fill, unsigned rounds)
{
	unsigned char input[input_len];
	memset(input, (uint8_t)fill, sizeof(input));

	sha1_ctx ctx;
	rhash_sha1_init(&ctx);
	for (int i = 0; i < rounds; i++) {
		rhash_sha1_update(&ctx, input, sizeof(input));
	}

	unsigned char ret[20];
	rhash_sha1_final(&ctx, ret);

	return (ret[0] << 24) | (ret[1] << 16) | (ret[2] << 8) | ret[3];
}
