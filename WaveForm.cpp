#include "SynthOX.h"

namespace SynthOX
{
	int gRand_x1 = 0x67452301;
	int gRand_x2 = 0xefcdab89;
	float GetWaveformValue(WaveType Type, float Cursor)
	{
		switch(Type)
		{
		case WaveType::Square:		return Cursor >= .5f ? -1.f : 1.f;
		case WaveType::Saw:			return 1.f - 2.f * Cursor;
		case WaveType::Triangle:	return Cursor < .5f ? 1.f - 4.f * Cursor : -1.f + 4.f * (Cursor - .5f);
		case WaveType::Sine:		return sinf(Cursor * 3.14159f*2.f);
		case WaveType::Rand:
			{
				gRand_x1 ^= gRand_x2;
				float Ret = float(gRand_x2);
				gRand_x2 += gRand_x1;
				return Ret;
			}
		}

		return 0.f;
	}
};