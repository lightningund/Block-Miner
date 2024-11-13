/*
 * sha256.cuh CUDA Implementation of SHA256 Hashing
 *
 * Date: 12 June 2019
 * Revision: 1
 *
 * Based on the public domain Reference Implementation in C, by
 * Brad Conte, original code here:
 *
 * https://github.com/B-Con/crypto-algorithms
 *
 * This file is released into the Public Domain.
 */

#pragma once
#include "config.h"

#define SHA256_BLOCK_SIZE 32 // SHA256 outputs a 32 byte digest

struct HashContext {
	BYTE data[64];
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];

	__device__
	HashContext();

	__device__
	void update(const BYTE incoming[], size_t len);

	__device__
	void digest(BYTE hash[]);

private:
	__device__
	__forceinline__
	void transform();
};

__global__
void kernel_sha256_hash(const BYTE* indata, WORD inlen, BYTE* outdata, WORD n_batch);

void mcm_cuda_sha256_hash_batch(const BYTE* in, WORD inlen, BYTE* out, WORD n_batch);
