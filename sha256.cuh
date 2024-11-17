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
	static constexpr size_t DIGEST_SIZE = 32;

	__host__ __device__
	HashContext();

	__host__ __device__
	void update(const BYTE incoming[], size_t len);

	__host__ __device__
	void update(const char incoming[], size_t len);

	__host__ __device__
	void update(size_t offset);

	__host__ __device__
	void digest(BYTE hash[]);

	__host__ __device__
	bool test(size_t difficulty);

private:
	BYTE data[64];
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];

	__host__ __device__
	__forceinline__
	void transform();
};
