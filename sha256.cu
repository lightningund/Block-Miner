/*
 * sha256.cu Implementation of SHA256 Hashing
 *
 * Date: 12 June 2019
 * Revision: 1
 * *
 * Based on the public domain Reference Implementation in C, by
 * Brad Conte, original code here:
 *
 * https://github.com/B-Con/crypto-algorithms
 *
 * This file is released into the Public Domain.
 */

/*************************** HEADER FILES ***************************/
#include <stdlib.h>
#include <memory.h>
#include "sha256.cuh"

/****************************** MACROS ******************************/
#ifndef ROTLEFT
#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#endif

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

/**************************** VARIABLES *****************************/
__constant__ WORD k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/*********************** FUNCTION DEFINITIONS ***********************/
__device__
__forceinline__
void HashContext::transform() {
	WORD a, b, c, d, e, f, g, h, t1, t2, m[64];

	for (int j = 0; j < 64; j += 4) {
		m[j / 4] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
	}
	for (int i = 16; i < 64; ++i) {
		m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
	}

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	f = state[5];
	g = state[6];
	h = state[7];

	for (int i = 0; i < 64; ++i) {
		t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
		t2 = EP0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

__device__
HashContext::HashContext() {
	datalen = 0;
	bitlen = 0;
	state[0] = 0x6a09e667;
	state[1] = 0xbb67ae85;
	state[2] = 0x3c6ef372;
	state[3] = 0xa54ff53a;
	state[4] = 0x510e527f;
	state[5] = 0x9b05688c;
	state[6] = 0x1f83d9ab;
	state[7] = 0x5be0cd19;
}

__device__
void HashContext::update(const BYTE incoming[], size_t len) {
	for (size_t i = 0; i < len; ++i) {
		data[datalen] = incoming[i];
		datalen++;
		if (datalen == 64) {
			transform();
			bitlen += 512;
			datalen = 0;
		}
	}
}

__device__
void HashContext::digest(BYTE hash[]) {
	WORD i = datalen;

	// Pad whatever data is left in the buffer.
	if (datalen < 56) {
		data[i++] = 0x80;
		while (i < 56) {
			data[i++] = 0x00;
		}
	} else {
		data[i++] = 0x80;
		while (i < 64) {
			data[i++] = 0x00;
		}
		transform();
		memset(data, 0, 56);
	}

	// Append to the padding the total message's length in bits and transform.
	bitlen += datalen * 8;
	data[63] = bitlen;
	data[62] = bitlen >> 8;
	data[61] = bitlen >> 16;
	data[60] = bitlen >> 24;
	data[59] = bitlen >> 32;
	data[58] = bitlen >> 40;
	data[57] = bitlen >> 48;
	data[56] = bitlen >> 56;
	transform();

	// Since this implementation uses little endian byte ordering and SHA uses big endian,
	// reverse all the bytes when copying the final state to the output hash.
	for (i = 0; i < 4; ++i) {
		hash[i]      = (state[0] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 4]  = (state[1] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 8]  = (state[2] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 12] = (state[3] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 16] = (state[4] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 20] = (state[5] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 24] = (state[6] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 28] = (state[7] >> (24 - i * 8)) & 0x000000ff;
	}
}

__global__
void kernel_sha256_hash(const BYTE* indata, WORD inlen, BYTE* outdata, WORD n_batch) {
	WORD thread = blockIdx.x * blockDim.x + threadIdx.x;
	if (thread >= n_batch) return;

	const BYTE* in = indata + thread * inlen;
	BYTE* out = outdata + thread * SHA256_BLOCK_SIZE;
	HashContext ctx{};
	ctx.update(in, inlen);
	ctx.digest(out);
}

void mcm_cuda_sha256_hash_batch(const BYTE* in, WORD inlen, BYTE* out, WORD n_batch) {
	BYTE *cuda_indata;
	BYTE *cuda_outdata;
	cudaMalloc(&cuda_indata, inlen * n_batch);
	cudaMalloc(&cuda_outdata, SHA256_BLOCK_SIZE * n_batch);
	cudaMemcpy(cuda_indata, in, inlen * n_batch, cudaMemcpyHostToDevice);

	WORD thread = 256;
	WORD block = (n_batch + thread - 1) / thread;

	kernel_sha256_hash<<<block, thread>>>(cuda_indata, inlen, cuda_outdata, n_batch);
	cudaMemcpy(out, cuda_outdata, SHA256_BLOCK_SIZE * n_batch, cudaMemcpyDeviceToHost);
	cudaDeviceSynchronize();
	cudaError_t error = cudaGetLastError();
	if (error != cudaSuccess) {
		printf("Error cuda sha256 hash: %s \n", cudaGetErrorString(error));
	}
	cudaFree(cuda_indata);
	cudaFree(cuda_outdata);
}
