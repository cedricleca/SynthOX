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

		for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
		{
			for(int i = 0; i < AnalogsourceOscillatorNr; i++)
			{
				for(int j = 0; j < int(LFODest::Max); j++)
					m_OscillatorTab[k][i].m_LFOTab[j].m_Data = &m_Data->m_OscillatorTab[i].m_LFOTab[j];

				m_OscillatorTab[k][i].m_LFOTab[int(LFODest::Tune)].m_ZeroCentered = true;
			}
		}
	}

	//-----------------------------------------------------
	void AnalogSource::NoteOn(int KeyId, float Velocity)
	{	
		for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
			if(m_NoteTab[k].m_Code == KeyId && m_NoteTab[k].m_NoteOn)
				return;

		auto LFONoteOn = [this](int PolyphonyIdx) 
		{
			for(int i = 0; i < AnalogsourceOscillatorNr; i++)
				for(int j = 0; j < int(LFODest::Max); j++)
					m_OscillatorTab[PolyphonyIdx][i].m_LFOTab[j].NoteOn();
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
			LFONoteOn(0);
		}
		else
		{
			for(int k = 0; k < AnalogsourcePolyphonyNoteNr; k++)
			{
				if(!m_NoteTab[k].m_NoteOn)
				{
					m_NoteTab[k].NoteOn(KeyId, Velocity);
					LFONoteOn(k);
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
			const float ReleaseTime = Note.m_Time - (m_Data->m_ADSR_Attack + m_Data->m_ADSR_Decay + Note.m_SustainTime);
			if(m_Data->m_ADSR_Release > 0.0f && ReleaseTime < m_Data->m_ADSR_Release*5.f && m_Data->m_ADSR_Release > 0.0f)
			{
				return (1.0f - (ReleaseTime / (m_Data->m_ADSR_Release*5.f))) * m_Data->m_ADSR_Sustain;
			}
			else
			{
				Note.m_Died = true;
				return 0.0f;
			}
		}
	}

	inline float Transfer(float x, float Alpha)
	{
		return x < .5f ? .5f - .5f * std::powf(1.f - 2.f*x, Alpha) : .5f + .5f * std::powf(2.f*x - 1.f, Alpha);
	}

//-----------------------------------------------------
	std::vector<float> AnalogSource::RenderScope(int OscIdx, unsigned int NbSamples)
	{
		std::vector<float> Ret;
		Ret.reserve(NbSamples);

		auto & Oscillator = m_OscillatorTab[0][OscIdx];

		float Morph	= Oscillator.m_LFOTab[int(LFODest::Morph)].m_Data->m_BaseValue;
		Morph = std::clamp(Morph, 0.f, 1.f);

		const float Alpha = .4f + .6f * Morph;
		const float C = std::powf(Alpha, 10.f) * 30.f;
        const float Flatness = std::powf(Oscillator.m_LFOTab[int(LFODest::Squish)].m_Data->m_BaseValue, 3.f) * 8.f;

		const float step = 1.f / NbSamples;
		for(unsigned int i = 0; i < NbSamples; i++)
		{
			const float Decat = std::ceilf(1.f + 1.f / (std::powf(Oscillator.m_LFOTab[int(LFODest::Decat)].m_Data->m_BaseValue, 3.f) + .001f));
			const float Cursor = (std::floor(step * (i+1) * Decat) / Decat) + .5f / Decat;
			auto Val = [Flatness, C](float c) -> float { return 1.f - Transfer(std::powf(c * 2.f, C), Flatness); };
			const float val = Cursor < .5f ? Val(Cursor) : -Val(1.f - Cursor);
			Ret.push_back(Distortion(Oscillator.m_LFOTab[int(LFODest::Distort)].m_Data->m_BaseValue, val));
		}

		return Ret;
	}
	
//-----------------------------------------------------
	void AnalogSource::Render(long SampleNr)
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

		long Cursor = m_Dest->m_WriteCursor;
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
				int BaseNote;
				float NoteFreq;
				switch(m_Data->m_PolyphonyMode)
				{
				case 
					PolyphonyMode::Arpeggio:
					BaseNote = m_NoteTab[m_ArpeggioIdx>>1].m_Code;
					break;

				case PolyphonyMode::Portamento:	
				/*
					BaseNote = m_NoteTab[0].m_Code;
					{
						float Newfreq = m_PortamentoCurFreq + m_PortamentoStep * Dtime;
						if(m_PortamentoCurFreq > NoteFreq)
							m_PortamentoCurFreq = (Newfreq < NoteFreq ? NoteFreq : Newfreq);
						else if(m_PortamentoCurFreq < NoteFreq)
							m_PortamentoCurFreq = (Newfreq > NoteFreq ? NoteFreq : Newfreq);

					}
					NoteFreq = m_PortamentoCurFreq;
					*/
					break;

				default:
					BaseNote = m_NoteTab[k].m_Code;
					break;
				}

				// update Oscillators
				for(int j = 0; j < AnalogsourceOscillatorNr; j++)
				{
					auto & Oscillator = m_OscillatorTab[k][j];
					NoteFreq = GetNoteFreq(BaseNote + m_Data->m_OscillatorTab[j].m_NoteOffset);

					auto LFOVal = [&Oscillator, Note](LFODest LFODest) -> float { return Oscillator.m_LFOTab[int(LFODest)].GetUpdatedValue(Note.m_Time); };

					const float Volume		= std::max(LFOVal(LFODest::Volume ), 0.f);
					const float Morph		= LFOVal(LFODest::Morph  );
					const float Squish		= LFOVal(LFODest::Squish );
					const float DistortGain	= LFOVal(LFODest::Distort);
					const float StepShift	= LFOVal(LFODest::Tune   );
					float Decat				= LFOVal(LFODest::Decat  );

					for(int o = 0; o < m_Data->m_OscillatorTab[j].m_OctaveOffset; o++)	NoteFreq *= 2.0f;
					for(int o = 0; o > m_Data->m_OscillatorTab[j].m_OctaveOffset; o--)	NoteFreq *= 0.5f;
			
					const float Alpha = .4f + .6f * std::clamp(Morph, 0.f, 1.f);
					const float C = std::powf(Alpha, 10.f) * 30.f;
					const float Flatness = Squish*Squish*Squish * 8.f;

					Decat = std::ceilf(1.f + (1.f / (Decat*Decat*Decat + .001f)));
					const float Cursor = Decat > 1000.f ? Oscillator.m_Cursor : (std::floor(Oscillator.m_Cursor * Decat) / Decat) + .5f / Decat;
					auto Val = [Flatness, C](float c) -> float { return 1.f - Transfer(std::powf(c * 2.f, C), Flatness); };
					float val = Cursor < .5f ? Val(Cursor) : -Val(1.f - Cursor);
					val = Distortion(DistortGain, val) * Volume;

					switch(m_Data->m_OscillatorTab[j].m_ModulationType)
					{
					case ModulationType::Mix:	NoteOutput += val;	break;
					case ModulationType::Mul:	NoteOutput *= std::lerp(1.f, val, Oscillator.m_LFOTab[int(LFODest::Volume)].m_Data->m_BaseValue);	break;
					case ModulationType::Ring:	NoteOutput *= 1.0f - 0.5f*(val+Volume);	break; // ???
					}

					// avance le curseur de lecture de l'oscillateur
					Oscillator.m_Cursor += std::max(NoteFreq + StepShift, 0.f) / PlaybackFreq;		
					Oscillator.m_Cursor -= std::floorf(Oscillator.m_Cursor);
				}

				Output += NoteOutput * ADSRMultiplier * .5f;
			}

			Output = std::clamp(Output, -1.f, 1.f);
			m_Dest->m_Data[Cursor] = { Output * m_Data->m_LeftVolume, Output * m_Data->m_RightVolume };
			Cursor = (Cursor + 1) % PlaybackFreq;
		}
	}

};