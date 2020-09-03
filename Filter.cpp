#include "SynthOX.h"

namespace SynthOX
{

	void EchoFilterSource<>::Render(long _SampleNr)
	{
		long & wc = m_Dest->m_WriteCursor;

		for(int i = 0; i < _SampleNr; i++)
		{
			float resoSource0 = 0.0f;
			float resoSource1 = 0.0f;
			float fd = 1.0f;
			long step = (m_ResoDelayLen/(m_ResoSteps+1)) + 1;
			for(int j = 0; j >= -m_ResoDelayLen; j-=step)
			{
				size_t csr = m_SrcWaveForm.m_WriteCursor + j;
				if(csr < 0)
					csr += m_SrcWaveForm.m_Data.size();

				resoSource0 += m_SrcWaveForm.m_Data[csr].first * fd;
				resoSource1 += m_SrcWaveForm.m_Data[csr].second * fd;
				fd *= m_ResoFeedback;
			}

			const float tSL = m_DelayWaveForm.m_Data[m_DelayWaveForm.m_WriteCursor].first;
			m_S0 = 0.3f*tSL + 0.7f*m_S0;
			m_S1 = 0.1f*tSL + 0.2f*m_S0 + 0.7f*m_S1;
			m_DelayWaveForm.m_Data[m_DelayWaveForm.m_WriteCursor].first = m_Feedback*m_S1 + resoSource0;

			const float tSR = m_DelayWaveForm.m_Data[m_DelayWaveForm.m_WriteCursor].second;
			m_S0 = 0.3f*tSR + 0.7f*m_S0;
			m_S1 = 0.1f*tSR + 0.2f*m_S0 + 0.7f*m_S1;
			m_DelayWaveForm.m_Data[m_DelayWaveForm.m_WriteCursor].second = m_Feedback*m_S1 + resoSource1;

			m_Dest->m_Data[wc].first += m_DelayWaveForm.m_Data[m_DelayWaveForm.m_WriteCursor].first;
			m_Dest->m_Data[wc].second += m_DelayWaveForm.m_Data[m_DelayWaveForm.m_WriteCursor].second;

			m_DelayWaveForm.m_WriteCursor = (m_DelayWaveForm.m_WriteCursor + 1) % m_DelayWaveForm.m_Data.size();
			if(m_DelayWaveForm.m_WriteCursor >= m_DelayLen)
				m_DelayWaveForm.m_WriteCursor = 0;

			m_SrcWaveForm.m_WriteCursor = (m_SrcWaveForm.m_WriteCursor + 1) % m_SrcWaveForm.m_Data.size();
			wc = (wc + 1) % m_Dest->m_Data.size();
		}
	}

};