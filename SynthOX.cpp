#include "SynthOX.h"
#include <tuple>
#include <algorithm>
#include <assert.h>
#include <cmath>

namespace SynthOX
{

	//-----------------------------------------------------------------------------
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

	//-----------------------------------------------------------------------------
	void FloatClear(float * Dest, long len) { std::memset(Dest, 0, len*sizeof(float)); }

	//-----------------------------------------------------
	float GetNoteFreq(int NoteCode) { return 440.f * std::powf(1.059463f, float(NoteCode-69)); }

	//-----------------------------------------------------
	float Distortion(float _Gain, float _Sample)
	{
	//	float absx = (_Sample<0.0f) ? -_Sample: _Sample;
	//	_Sample = _Sample*(absx + _Gain)/(_Sample*_Sample + (_Gain-1.0f)*absx + 1.0f);

		_Sample *= 1.0f + _Gain;
		_Sample = 1.5f*_Sample - 0.5f*_Sample*_Sample*_Sample;

		/*
		while(_Sample > 1.0f || _Sample < -1.0f)
		{
			if(_Sample > 0.0f)
				_Sample = 2.0f - _Sample;
			else
				_Sample = -2.0f - _Sample;
		}
		*/

	/*
	#define DIST_LIMIT (0.65f)
		if(_Sample > 1.0f)
		{
			while(_Sample > 1.0f || _Sample < DIST_LIMIT)
			{
				if(_Sample > 1.0f)
					_Sample = 2.0f - _Sample;
				if(_Sample < DIST_LIMIT)
					_Sample = DIST_LIMIT + DIST_LIMIT - _Sample;
			}
		}
		else if(_Sample < -1.0f)
		{
			while(_Sample < -1.0f || _Sample > -DIST_LIMIT)
			{
				if(_Sample < -1.0f)
					_Sample = -2.0f - _Sample;
				if(_Sample > -DIST_LIMIT)
					_Sample = -DIST_LIMIT - DIST_LIMIT - _Sample;
			}
		}
	*/
		return _Sample;
	}

	//-----------------------------------------------------
	void Synth::Render(unsigned int SamplesToRender)
	{
		assert(SamplesToRender <= m_OutBuf.m_Data.size());
		assert(m_SourceTab.size() > 0);

		// clear out buffers
		for(auto & Source : m_SourceTab)
			Source->GetDest().Clear(SamplesToRender);

		// render source buffers en reverse
		for(int i = int(m_SourceTab.size()) - 1; i >= 0; i--)
			m_SourceTab[i]->Render(SamplesToRender);
	}

	//-----------------------------------------------------
	void Synth::PopOutputVal(float & OutLeft, float & OutRight)
	{
		std::tie(OutLeft, OutRight) = m_OutBuf.m_Data[m_OutBuf.m_WriteCursor];
		OutLeft = std::clamp(OutLeft, 0.f, 1.f);
		OutRight = std::clamp(OutRight, 0.f, 1.f);
		m_OutBuf.m_WriteCursor = (m_OutBuf.m_WriteCursor + 1) % m_OutBuf.m_Data.size(); 
	}

	//-----------------------------------------------------
	void Synth::NoteOn(int _Channel, int _KeyId, float _Velocity)
	{
		for(auto & Source : m_SourceTab)
			Source->NoteOn(_Channel, _KeyId, _Velocity);
	}

	//-----------------------------------------------------
	void Synth::NoteOff(int _Channel, int _KeyId)
	{
		for(auto & Source : m_SourceTab)
			Source->NoteOff(_Channel, _KeyId);
	}

};
