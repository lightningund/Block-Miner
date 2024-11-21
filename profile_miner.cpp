#include <iostream>

#include "kernel.cuh"

int main() {
	Block test_block{
		.minedBy = "Prof!",
		.messages = {"Keep it", "simple.", "Veni", "vidi", "vici"},
		.nonce = "663135608617883",
		.height = 0,
		.timestamp = 1730910874,
		.hash = "75977fa09516d028befa0695e16c93be20271b66630236d38718e35700000000"
	};

	Finder finder{test_block};
	finder.set_last_hash("");
	finder.find_nonce();

	return 0;
}