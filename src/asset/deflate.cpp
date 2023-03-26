#include "pch.h"
#include "deflate.h"


struct bit_stream
{
	uint8* data;
	uint64 size;
	uint64 readOffset;

	uint32 bitCount;
	uint32 bitBuffer;


	template <typename T>
	T* consume(uint32 count = 1)
	{
		T* result = (T*)(data + readOffset);
		readOffset += sizeof(T) * count;
		ASSERT(readOffset <= size);
		return result;
	}

	uint32 peekBits(uint32 bitCount)
	{
		ASSERT(bitCount <= 32);

		uint32 result = 0;

		while ((this->bitCount < bitCount))
		{
			uint32 byte = *consume<uint8>();
			this->bitBuffer |= (byte << this->bitCount);
			this->bitCount += 8;
		}

		result = this->bitBuffer & ((1 << bitCount) - 1);

		return result;
	}

	void discardBits(uint32 bitCount)
	{
		this->bitCount -= bitCount;
		this->bitBuffer >>= bitCount;
	}

	uint32 consumeBits(uint32 bitCount)
	{
		uint32 result = peekBits(bitCount);
		discardBits(bitCount);
		return result;
	}

	void flushByte()
	{
		uint32 flushCount = (bitCount % 8);
		consumeBits(flushCount);
	}

	uint64 bytesRemaining()
	{
		return size - readOffset;
	}
};

static uint32 reverseBits(uint32 value, uint32 bitCount)
{
	uint32 result = 0;

	for (uint32 i = 0; i <= (bitCount / 2); ++i)
	{
		uint32 inv = (bitCount - (i + 1));
		result |= ((value >> i) & 0x1) << inv;
		result |= ((value >> inv) & 0x1) << i;
	}

	return result;
}



struct huffman_entry
{
	uint16 symbol;
	uint16 codeLength;
};

struct huffman_table
{
	uint32 maxCodeLengthInBits;
	std::vector<huffman_entry> entries;

	void initialize(uint32 maxCodeLengthInBits, uint32* symbolLengths, uint32 numSymbols, uint32 symbolOffset = 0)
	{
		ASSERT(maxCodeLengthInBits <= 16);

		this->maxCodeLengthInBits = maxCodeLengthInBits;
		uint32 entryCount = (1 << maxCodeLengthInBits);
		this->entries.resize(entryCount);


		uint32 blCount[16] = {};
		for (uint32 i = 0; i < numSymbols; ++i)
		{
			uint32 count = symbolLengths[i];
			ASSERT(count < arraysize(blCount));
			++blCount[count];
		}

		uint32 nextCode[16];
		nextCode[0] = 0;
		blCount[0] = 0;
		for (uint32 bits = 1; bits < 16; ++bits)
		{
			nextCode[bits] = ((nextCode[bits - 1] + blCount[bits - 1]) << 1);
		}

		for (uint32 n = 0; n < numSymbols; ++n)
		{
			uint32 len = symbolLengths[n];
			if (len)
			{
				uint32 code = nextCode[len]++;

				uint32 numGarbageBits = maxCodeLengthInBits - len;
				uint32 numEntries = (1 << numGarbageBits);

				for (uint32 i = 0; i < numEntries; ++i)
				{
					uint32 base = (code << numGarbageBits) | i;
					uint32 index = reverseBits(base, maxCodeLengthInBits);

					huffman_entry& entry = entries[index];

					uint32 symbol = n + symbolOffset;
					entry.codeLength = (uint16)len;
					entry.symbol = (uint16)symbol;
				}
			}
		}
	}

	uint32 decode(bit_stream& stream)
	{
		uint32 index = stream.peekBits(maxCodeLengthInBits);
		huffman_entry entry = entries[index];
		stream.discardBits(entry.codeLength);
		return entry.symbol;
	}
};

static huffman_entry lengthExtra[] =
{
	{3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, {8, 0}, {9, 0}, {10, 0}, {11, 1}, {13, 1}, {15, 1}, {17, 1}, {19, 2},
	{23, 2}, {27, 2}, {31, 2}, {35, 3}, {43, 3}, {51, 3}, {59, 3}, {67, 4}, {83, 4}, {99, 4}, {115, 4}, {131, 5},
	{163, 5}, {195, 5}, {227, 5}, {258, 0}
};

static huffman_entry distExtra[] =
{
	{1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 1}, {7, 1}, {9, 2}, {13, 2}, {17, 3}, {25, 3}, {33, 4}, {49, 4}, {65, 5},
	{97, 5}, {129, 6}, {193, 6}, {257, 7}, {385, 7}, {513, 8}, {769, 8}, {1025, 9}, {1537, 9}, {2049, 10}, {3073, 10},
	{4097, 11}, {6145, 11}, {8193, 12}, {12289, 12}, {16385, 13}, {24577, 13}
};


uint64 decompress(uint8* data, uint64 compressedSize, uint8* output)
{
	bit_stream stream = { data, compressedSize };

	uint32 zlibHeader0 = *stream.consume<uint8>();
	uint32 zlibHeader1 = *stream.consume<uint8>();
	uint32 counter = (((zlibHeader0 * 256 + zlibHeader1) % 31 != 0) || (zlibHeader1 & 32) || ((zlibHeader0 & 15) != 8));
	ASSERT(counter == 0);

	uint8* outputStart = output;


	uint32 BFINAL = 0;
	while (BFINAL == 0)
	{
		BFINAL = stream.consumeBits(1);
		uint32 BTYPE = stream.consumeBits(2);

		ASSERT(BTYPE != 3);

		if (BTYPE == 0)
		{
			// No compression.
			stream.flushByte();

			uint16 LEN = (uint16)stream.consumeBits(16);
			uint16 NLEN = (uint16)stream.consumeBits(16);
			ASSERT((uint16)LEN == (uint16)~NLEN);

			while (LEN)
			{
				uint16 useLEN = LEN;
				useLEN = (uint16)min((uint64)useLEN, stream.bytesRemaining());

				uint8* source = stream.consume<uint8>(useLEN);
				uint16 copyCount = useLEN;
				while (copyCount--)
				{
					*output++ = *source++;
				}

				LEN -= useLEN;
			}
		}
		else
		{
			uint32 litlen_dist[512];

			uint32 HLIT = 0;
			uint32 HDIST = 0;
			if (BTYPE == 2)
			{
				// Compressed with dynamic Huffman codes.
				HLIT = stream.consumeBits(5) + 257;
				HDIST = stream.consumeBits(5) + 1;
				uint32 HCLEN = stream.consumeBits(4) + 4;

				uint32 HCLENSwizzle[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
				ASSERT(HCLEN <= arraysize(HCLENSwizzle));
				uint32 HCLENTable[arraysize(HCLENSwizzle)] = {};

				for (uint32 i = 0; i < HCLEN; ++i)
				{
					HCLENTable[HCLENSwizzle[i]] = stream.consumeBits(3);
				}

				huffman_table dictTable;
				dictTable.initialize(7, HCLENTable, arraysize(HCLENSwizzle));

				uint32 outIndex = 0;
				while (outIndex < HLIT + HDIST)
				{
					uint32 len = dictTable.decode(stream);
					uint32 value = 0;
					uint32 repeat = 1;
					if (len < 16)
					{
						value = len;
					}
					else if (len == 16)
					{
						repeat = stream.consumeBits(2) + 3;
						ASSERT(outIndex > 0);
						value = litlen_dist[outIndex - 1];
					}
					else if (len == 17)
					{
						repeat = stream.consumeBits(3) + 3;
					}
					else if (len == 18)
					{
						repeat = stream.consumeBits(7) + 11;
					}

					for (uint32 r = 0; r < repeat; ++r)
					{
						litlen_dist[outIndex++] = value;
					}
				}
				ASSERT(outIndex == HLIT + HDIST);
			}
			else if (BTYPE == 1)
			{
				HLIT = 288;
				HDIST = 32;

				uint32 bitCounts[][2] = { {143, 8}, {255, 9}, {279, 7}, {287, 8}, {319, 5} };

				uint32 outIndex = 0;
				for (uint32 i = 0; i < arraysize(bitCounts); ++i)
				{
					uint32 lastValue = bitCounts[i][0];
					uint32 bitCount = bitCounts[i][1];
					while (outIndex <= lastValue)
					{
						litlen_dist[outIndex++] = bitCount;
					}
				}
			}

			huffman_table litLenTable;
			litLenTable.initialize(15, litlen_dist, HLIT);

			huffman_table distTable;
			distTable.initialize(15, litlen_dist + HLIT, HDIST);

			while (true)
			{
				uint32 litLen = litLenTable.decode(stream);
				if (litLen <= 255)
				{
					*output++ = litLen;
				}
				else if (litLen >= 257)
				{
					huffman_entry lenEntry = lengthExtra[litLen - 257];
					uint32 len = lenEntry.symbol;
					if (lenEntry.codeLength)
					{
						len += stream.consumeBits(lenEntry.codeLength);
					}

					uint32 distIndex = distTable.decode(stream);
					huffman_entry distEntry = distExtra[distIndex];
					uint32 dist = distEntry.symbol;
					if (distEntry.codeLength)
					{
						dist += stream.consumeBits(distEntry.codeLength);
					}

					uint8* input = output - dist;
					for (uint32 r = 0; r < len; ++r)
					{
						*output++ = *input++;
					}
				}
				else
				{
					break;
				}
			}
		}
	}

	return output - outputStart;
}

