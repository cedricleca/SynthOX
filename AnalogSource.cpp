#include "SynthOX.h"
#include <algorithm>
#include <cmath>

namespace SynthOX
{

	//-----------------------------------------------------
	AnalogSource::AnalogSource(StereoSoundBuf * Dest, int Channel, AnalogSourceData * Data) : 
		SoundSource(Dest, Channel),
		m_Data(Data)
	{
		m_Data->m_PolyphonyMode = PolyphonyMode::Poly;
		m_Data->m_PortamentoTime = 0.5f;
	}

	void AnalogSource::OnBound(Synth * Synth)
	{ 
		SoundSource::OnBound(Synth); 

		for(int i = 0; i < AnalogsourceOscillatorNr; i++)
		{
			for(int j = 0; j < int(LFODest::Max); j++)
				m_OscillatorTab[i].m_LFOTab[j].m_Data = &m_Data->m_OscillatorTab[i].m_LFOTab[j];

			m_OscillatorTab[i].m_LFOTab[int(LFODest::Tune)].m_ZeroCentered = true;
		}
	}

	//-----------------------------------------------------
	void AnalogSource::NoteOn(int KeyId, float Velocity)
	{	
		for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
			if(m_NoteTab[k].m_Code == KeyId && m_NoteTab[k].m_NoteOn)
				return;

		auto LFONoteOn = [this]() 
		{
			for(int i = 0; i < AnalogsourceOscillatorNr; i++)
				for(int j = 0; j < int(LFODest::Max); j++)
					m_OscillatorTab[i].m_LFOTab[j].NoteOn();
		};

		if(m_Data->m_PolyphonyMode == PolyphonyMode::Portamento)
		{
			if(m_NoteTab[0].m_NoteOn)
			{
				m_PortamentoCurFreq = GetNoteFreq(m_NoteTab[0].m_Code);
				m_PortamentoStep = (GetNoteFreq(KeyId) - m_PortamentoCurFreq) / m_Data->m_PortamentoTime;
				m_NoteTab[0].NoteOff();
			}
			else
			{
				m_PortamentoCurFreq = GetNoteFreq(KeyId);
				m_PortamentoStep = 0.0f;
			}

			m_NoteTab[0].NoteOn(KeyId, Velocity);
			LFONoteOn();
		}
		else
		{
			for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
			{
				if(!m_NoteTab[k].m_NoteOn)
				{
					m_NoteTab[k].NoteOn(KeyId, Velocity);
					LFONoteOn();
					break;
				}
			}
		}
	}

	//-----------------------------------------------------
	void AnalogSource::NoteOff(int KeyId)
	{
		for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
		{
			if(m_NoteTab[k].m_Code == KeyId)
			{
				m_NoteTab[k].NoteOff();
				break;
			}
		}
	}

	//-----------------------------------------------------
	float AnalogSource::GetADSRValue(Note & Note, float DTime)
	{
		if(Note.m_NoteOn)
		{
			if(Note.m_Time > m_Data->m_ADSR_Attack + m_Data->m_ADSR_Decay)
			{
				Note.m_SustainTime += DTime;
				return m_Data->m_ADSR_Sustain;
			}
			else
			{
				if(Note.m_Time > m_Data->m_ADSR_Attack && m_Data->m_ADSR_Decay > 0.0f)
					return 1.0f + ((Note.m_Time - m_Data->m_ADSR_Attack) / m_Data->m_ADSR_Decay) * (m_Data->m_ADSR_Sustain - 1.0f);
				else if(m_Data->m_ADSR_Attack > 0.0f)
					return (Note.m_Time / m_Data->m_ADSR_Attack);
				else
					return 0.0f;
			}
		}
		else
		{
			if(Note.m_Time - Note.m_SustainTime < m_Data->m_ADSR_Release && m_Data->m_ADSR_Release > 0.0f)
				return (1.0f - ((Note.m_Time - Note.m_SustainTime) / m_Data->m_ADSR_Release)) * m_Data->m_ADSR_Sustain;
			else
			{
				Note.m_Died = true;
				return 0.0f;
			}
		}
	}

	//-----------------------------------------------------
	void AnalogSource::RenderScope()
	{
		m_ScopeDest.m_WriteCursor = 0;
		RenderToDest(PlaybackFreq, &m_ScopeDest);
	}

	std::pair<float, float> AnalogSource::PopScopeVal()
	{
		auto Ret = m_ScopeDest.m_Data[m_ScopeDest.m_WriteCursor];
		m_ScopeDest.m_WriteCursor = (m_ScopeDest.m_WriteCursor + 1) % PlaybackFreq; 
		return Ret;
	}
	
//-----------------------------------------------------
	void AnalogSource::RenderToDest(long SampleNr, StereoSoundBuf * AuxDest)
	{
		int nbActiveNotes = 0;
		for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
		{
			// arpeggio : count active notes
			if(m_NoteTab[k].m_NoteOn)
				nbActiveNotes++;
		}
		// next arpeggio step
		if(nbActiveNotes > 0)
		{
			m_ArpeggioIdx++;
			m_ArpeggioIdx %= (nbActiveNotes<<1);
		}

		static const float Dtime = 1.f / PlaybackFreq;

		StereoSoundBuf * Dest = AuxDest ? AuxDest : m_Dest;
		long Cursor = Dest->m_WriteCursor;
		const int PolyNoteNr = (m_Data->m_PolyphonyMode == PolyphonyMode::Poly ? AnalogsourcePolyphonyNoteNr : 1);
		// compute samples
		for(long i = 0; i < SampleNr; i++)
		{
			float Output = 0.f;
			for(int k = 0; k < PolyNoteNr; k++)
			{
				float NoteOutput = 0.0f;
				auto & Note = m_NoteTab[k];
				Note.m_Time += Dtime;
	
				// Get ADSR and Velocity
				const float ADSRMultiplier = GetADSRValue(Note, Dtime) * Note.m_Velocity;
				if(ADSRMultiplier == 0.0f)
					continue;

				// select the active note depending on arpeggio mode
				float NoteFreq;
				switch(m_Data->m_PolyphonyMode)
				{
				case 
					PolyphonyMode::Arpeggio:	
					NoteFreq = GetNoteFreq(m_NoteTab[m_ArpeggioIdx>>1].m_Code);
					break;

				case PolyphonyMode::Portamento:	
					NoteFreq = GetNoteFreq(m_NoteTab[0].m_Code);
					{
						float Newfreq = m_PortamentoCurFreq + m_PortamentoStep * Dtime;
						if(m_PortamentoCurFreq > NoteFreq)
							m_PortamentoCurFreq = (Newfreq < NoteFreq ? NoteFreq : Newfreq);
						else if(m_PortamentoCurFreq < NoteFreq)
							m_PortamentoCurFreq = (Newfreq > NoteFreq ? NoteFreq : Newfreq);

					}
					NoteFreq = m_PortamentoCurFreq;
					break;

				default:
					NoteFreq = GetNoteFreq(m_NoteTab[k].m_Code);
					break;
				}

				// update Oscillators
				for(int j = 0; j < AnalogsourceOscillatorNr; j++)
				{
					auto & Oscillator = m_OscillatorTab[j];

					Oscillator.m_Volume			= Oscillator.m_LFOTab[int(LFODest::Volume )].GetUpdatedValue(Note.m_Time);
					Oscillator.m_Morph			= Oscillator.m_LFOTab[int(LFODest::Morph  )].GetUpdatedValue(Note.m_Time);
					Oscillator.m_DistortGain	= Oscillator.m_LFOTab[int(LFODest::Distort)].GetUpdatedValue(Note.m_Time);
					Oscillator.m_StepShift		= Oscillator.m_LFOTab[int(LFODest::Tune   )].GetUpdatedValue(Note.m_Time);

					for(int o = 0; o < m_Data->m_OscillatorTab[j].m_OctaveOffset; o++)	NoteFreq *= 2.0f;
					for(int o = 0; o > m_Data->m_OscillatorTab[j].m_OctaveOffset; o--)	NoteFreq *= 0.5f;
			
					Oscillator.m_Step	= std::max(NoteFreq + Oscillator.m_StepShift, 0.f);
					Oscillator.m_Morph	= std::clamp(Oscillator.m_Morph, 0.f, 1.f);
					Oscillator.m_Volume	= std::max(Oscillator.m_Volume, 0.f);

					float val;
					const float alpha = .4f + .6f * Oscillator.m_Morph;
					const float C = std::powf(alpha, 10.f) * 30.f;
			        const float Flatness = 2.f; // [2., 8.]
					if(Oscillator.m_Cursor < .5f)
					{
						const float XX = std::powf(Oscillator.m_Cursor * 2.f, C);
						val = -.5f + .5f * (1.f - std::powf((2.f * XX - 1.f), Flatness));
					}
					else
					{
						const float XX = std::powf((Oscillator.m_Cursor - .5f) * 2.f, C);
						val = .5f * std::powf((2.f * XX - 1.f), Flatness);
					}

					val = Distortion(Oscillator.m_DistortGain, val);
					val *= Oscillator.m_Volume;

					switch(m_Data->m_OscillatorTab[j].m_ModulationType)
					{
					case ModulationType::Mix:	NoteOutput += val;	break;
					case ModulationType::Mul:	NoteOutput *= val;	break;
					case ModulationType::Ring:	NoteOutput *= 1.0f - 0.5f*(val+Oscillator.m_Volume);	break; // ???
					}

					// avance le curseur de lecture de l'oscillateur
					Oscillator.m_Cursor += Oscillator.m_Step / PlaybackFreq;		
					Oscillator.m_Cursor -= std::floorf(Oscillator.m_Cursor);
				}

				Output += NoteOutput * ADSRMultiplier;
			}

			Dest->m_Data[Cursor] = { Output * m_Data->m_LeftVolume, Output * m_Data->m_RightVolume };
			Cursor = (Cursor + 1) % PlaybackFreq;
		}
	}

};