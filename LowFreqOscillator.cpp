#include "SynthOX.h"

#include <cmath>

namespace SynthOX
{
	//-----------------------------------------------------
	float LFOTransients::GetUpdatedValue(float NoteTime)
	{
		float Dum;
		m_Cursor = std::modf(m_Cursor + 1.f/PlaybackFreq, &Dum);

		if(NoteTime > m_Data->m_Delay)
		{
			float val = GetWaveformValue(m_Data->m_WF, m_Cursor) * m_Data->m_Magnitude;

			NoteTime -= m_Data->m_Delay;
			if(NoteTime < m_Data->m_Attack)
				val *= NoteTime / m_Data->m_Attack;

			return val * m_Data->m_BaseValue + (m_ZeroCentered ? 0.0f : m_Data->m_BaseValue);
		}

		if(m_Data->m_Delay > 0.0f)
		{
			float val = m_Data->m_BaseValue * NoteTime / m_Data->m_Delay;
			return m_ZeroCentered ? m_Data->m_BaseValue - val : val;
		}

		return 0.0f;
	}

	//-----------------------------------------------------
	void LFOTransients::NoteOn()
	{
		if(m_Data->m_NoteSync)
			m_Cursor = 0.f;
	}

};