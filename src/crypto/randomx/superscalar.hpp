

#pragma once

#include <cstdint>
#include <vector>
#include "crypto/randomx/superscalar_program.hpp"
#include "crypto/randomx/blake2_generator.hpp"

namespace randomx {
	                                              //                  Intel Ivy Bridge reference
	enum class SuperscalarInstructionType {       //uOPs (decode)   execution ports         latency       code size
		ISUB_R = 0,                               //1               p015                    1               3 (sub)
		IXOR_R = 1,                               //1               p015                    1               3 (xor)
		IADD_RS = 2,                              //1               p01                     1               4 (lea)
		IMUL_R = 3,                               //1               p1                      3               4 (imul)
		IROR_C = 4,                               //1               p05                     1               4 (ror)
		IADD_C7 = 5,                              //1               p015                    1               7 (add)
		IXOR_C7 = 6,                              //1               p015                    1               7 (xor)
		IADD_C8 = 7,                              //1+0             p015                    1               7+1 (add+nop)
		IXOR_C8 = 8,                              //1+0             p015                    1               7+1 (xor+nop)
		IADD_C9 = 9,                              //1+0             p015                    1               7+2 (add+nop)
		IXOR_C9 = 10,                             //1+0             p015                    1               7+2 (xor+nop)
		IMULH_R = 11,                             //1+2+1           0+(p1,p5)+0             3               3+3+3 (mov+mul+mov)
		ISMULH_R = 12,                            //1+2+1           0+(p1,p5)+0             3               3+3+3 (mov+imul+mov)
		IMUL_RCP = 13,                            //1+1             p015+p1                 4              10+4   (mov+imul)

		COUNT = 14,
		INVALID = -1
	};

	void generateSuperscalar(SuperscalarProgram& prog, Blake2Generator& gen);
	void executeSuperscalar(uint64_t(&r)[8], SuperscalarProgram& prog);
}
