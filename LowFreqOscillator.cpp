#include "SynthOX.h"

#include <cmath>

namespace SynthOX
{

	//-----------------------------------------------------
	void LFOTransients::Init(LFOData * Data)
	{	
		m_Data = Data;
		SetOscillator(m_Data->m_WF);
	}

	//-----------------------------------------------------
	void LFOTransients::SetOscillator(WaveType Wave)
	{
		m_Data->m_WF = Wave;
	}

	//-----------------------------------------------------
	float LFOTransients::GetValue(float NoteTime, bool ZeroCentered)
	{
		if(NoteTime > m_Data->m_Delay)
		{
			float val = m_CurVal * m_Data->m_Magnitude;

			NoteTime -= m_Data->m_Delay;
			if(NoteTime < m_Data->m_Attack)
				val *= NoteTime / m_Data->m_Attack;

			return val * m_Data->m_BaseValue + (ZeroCentered ? 0.0f : m_Data->m_BaseValue);
		}

		if(m_Data->m_Delay > 0.0f)
		{
			float val = m_Data->m_BaseValue * NoteTime / m_Data->m_Delay;
			return ZeroCentered ? m_Data->m_BaseValue - val : val;
		}

		return 0.0f;
	}

	//-----------------------------------------------------
	void LFOTransients::Update(float FrameTime)
	{
		float Dum;
		m_Cursor = std::modf(m_Cursor + FrameTime, &Dum);
		m_CurVal = GetWaveformValue(m_Data->m_WF, m_Cursor);
	}

	//-----------------------------------------------------
	void LFOTransients::NoteOn()
	{
		if(m_Data->m_NoteSync)
			m_Cursor = 0.f;
	}

};