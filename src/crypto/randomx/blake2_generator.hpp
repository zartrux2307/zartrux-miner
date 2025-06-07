

#pragma once

#include <cstdint>

namespace randomx {

	class Blake2Generator {
	public:
		Blake2Generator(const void* seed, size_t seedSize, int nonce = 0);
		uint8_t getByte();
		uint32_t getUInt32();
	private:
		void checkData(const size_t);

		uint8_t data[64];
		size_t dataIndex;
	};
}