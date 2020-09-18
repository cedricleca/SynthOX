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
	}

	void AnalogSource::OnBound(Synth * Synth)
	{ 
		SoundSource::OnBound(Synth); 

		for(auto & Note : m_NoteTab)
		{
			for(int i = 0; i < AnalogsourceOscillatorNr; i++)
			{
				for(int j = 0; j < int(LFODest::Max); j++)
					Note.m_OscillatorTab[i].m_LFOTab[j].m_Data = &m_Data->m_OscillatorTab[i].m_LFOTab[j];

				Note.m_OscillatorTab[i].m_LFOTab[int(LFODest::Tune)].m_ZeroCentered = true;
			}
		}
	}

	//-----------------------------------------------------
	void AnalogSource::NoteOn(int KeyId, float Velocity)
	{	
		auto LFONoteOn = [this](AnalogSourceNote & Note) 
		{
			for(auto & Osc : Note.m_OscillatorTab)
				for(auto & LFO : Osc.m_LFOTab)
					LFO.NoteOn();
		};

		if(m_Data->m_PolyphonyMode == PolyphonyMode::Portamento)
		{
			if(m_NoteTab[0].m_NoteOn)
			{
				m_PortamentoBaseNote = float(m_NoteTab[0].m_Code);
				m_PortamentoStep = (float(KeyId) - m_PortamentoBaseNote) / m_Data->m_PortamentoTime;
				m_NoteTab[0].NoteOff();
			}
			else
			{
				m_PortamentoBaseNote = float(KeyId);
				m_PortamentoStep = 0.0f;
			}

			m_NoteTab[0].NoteOn(KeyId, Velocity);
			LFONoteOn(m_NoteTab[0]);
		}
		else
		{
			bool bFound = false;
			for(auto & Note : m_NoteTab)
			{
				if(Note.m_Code == KeyId)
				{
					Note.m_AmpADSRValue = GetADSRValue(Note, 0.f);
					Note.NoteOn(KeyId, Velocity);
					LFONoteOn(Note);
					bFound = true;
					break;
				}
			}

			if(!bFound)
			{
				for(auto & Note : m_NoteTab)
				{
					if(Note.m_Died)
					{
						Note.m_AmpADSRValue = GetADSRValue(Note, 0.f);
						Note.NoteOn(KeyId, Velocity);
						LFONoteOn(Note);
						bFound = true;
						break;
					}
				}
			}

			if(!bFound)
			{
				for(auto & Note : m_NoteTab)
				{
					if(!Note.m_NoteOn)
					{
						Note.m_AmpADSRValue = GetADSRValue(Note, 0.f);
						Note.NoteOn(KeyId, Velocity);
						LFONoteOn(Note);
						bFound = true;
						break;
					}
				}
			}
		}
	}

	//-----------------------------------------------------
	void AnalogSource::NoteOff(int KeyId)
	{
		for(auto & Note : m_NoteTab)
		{
			if(Note.m_Code == KeyId)
			{
				Note.m_AmpADSRValue = GetADSRValue(Note, 0.f);
				Note.NoteOff();
				break;
			}
		}
	}

	//-----------------------------------------------------
	float AnalogSource::GetADSRValue(AnalogSourceNote & Note, float DTime)
	{
		if(Note.m_NoteOn)
		{
			if(Note.m_Time > m_Data->m_ADSR_Attack + m_Data->m_ADSR_Decay)
			{
				return m_Data->m_ADSR_Sustain;
			}
			else
			{
				if(Note.m_Time > m_Data->m_ADSR_Attack && m_Data->m_ADSR_Decay > 0.0f)
					return 1.0f + ((Note.m_Time - m_Data->m_ADSR_Attack) / m_Data->m_ADSR_Decay) * (m_Data->m_ADSR_Sustain - 1.0f);
				else if(m_Data->m_ADSR_Attack > 0.0f)
					return std::lerp(Note.m_AmpADSRValue, 1.f, (Note.m_Time / m_Data->m_ADSR_Attack));
				else
					return 0.0f;
			}
		}
		else
		{
			const float ReleaseTime = Note.m_Time - Note.m_NoteOffTime;
			if(m_Data->m_ADSR_Release > 0.0f && ReleaseTime < m_Data->m_ADSR_Release*5.f && m_Data->m_ADSR_Release > 0.0f)
			{
				return (1.0f - (ReleaseTime / (m_Data->m_ADSR_Release*5.f))) * Note.m_AmpADSRValue;
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

		auto & Oscillator = m_NoteTab[0].m_OscillatorTab[OscIdx];

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
		for(auto & Note : m_NoteTab)
		{
			// arpeggio : count active notes
			if(Note.m_NoteOn)
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
			for(auto & Note : m_NoteTab)
			{
				float NoteOutput = 0.0f;
				Note.m_Time += Dtime;
	
				// Get ADSR and Velocity
				const float ADSRMultiplier = GetADSRValue(Note, Dtime) * Note.m_Velocity;
				if(ADSRMultiplier == 0.0f)
					continue;

				// select the active note depending on arpeggio mode
				float BaseNote;
				switch(m_Data->m_PolyphonyMode)
				{
				case 
					PolyphonyMode::Arpeggio:
					BaseNote = float(m_NoteTab[m_ArpeggioIdx>>1].m_Code);
					break;

				case PolyphonyMode::Portamento:	
					{
						float NewBaseNote = m_PortamentoBaseNote + m_PortamentoStep * Dtime;
						if(m_PortamentoBaseNote > m_NoteTab[0].m_Code)
							m_PortamentoBaseNote = (NewBaseNote < m_NoteTab[0].m_Code ? m_NoteTab[0].m_Code : NewBaseNote);
						else if(m_PortamentoBaseNote < m_NoteTab[0].m_Code)
							m_PortamentoBaseNote = (NewBaseNote > m_NoteTab[0].m_Code ? m_NoteTab[0].m_Code : NewBaseNote);

						BaseNote = m_PortamentoBaseNote;
					}
					break;

				default:
					BaseNote = float(Note.m_Code);
					break;
				}

				BaseNote += m_Synth->m_PitchBend * 2.f; // TODO : interpolate MIDI value based on TimeStamp

				// update Oscillators
				for(int j = 0; j < AnalogsourceOscillatorNr; j++)
				{
					auto & Oscillator = Note.m_OscillatorTab[j];
					const auto & OscillatorData = m_Data->m_OscillatorTab[j];

					auto LFOVal = [&Oscillator, Note](LFODest LFODest) -> float { return Oscillator.m_LFOTab[int(LFODest)].GetUpdatedValue(Note.m_Time); };
					const float Volume		= std::max(LFOVal(LFODest::Volume ), 0.f);
					const float Morph		= LFOVal(LFODest::Morph  );
					const float Squish		= LFOVal(LFODest::Squish );
					const float DistortGain	= LFOVal(LFODest::Distort);
					const float StepShift	= LFOVal(LFODest::Tune   );
					float Decat				= LFOVal(LFODest::Decat  );

					float NoteFreq = GetNoteFreq(BaseNote + float(OscillatorData.m_NoteOffset));

					for(int o = 0; o < OscillatorData.m_OctaveOffset; o++)	NoteFreq *= 2.0f;
					for(int o = 0; o > OscillatorData.m_OctaveOffset; o--)	NoteFreq *= 0.5f;
			
					const float Alpha = .4f + .6f * std::clamp(Morph, 0.f, 1.f);
					const float C = std::powf(Alpha, 10.f) * 30.f;
					const float Flatness = Squish*Squish*Squish * 8.f;

					Decat = std::ceilf(1.f + (1.f / (Decat*Decat*Decat + .001f)));
					const float Cursor = Decat > 1000.f ? Oscillator.m_Cursor : (std::floor(Oscillator.m_Cursor * Decat) / Decat) + .5f / Decat;
					auto GetVal = [Flatness, C](float c) -> float { return 1.f - Transfer(std::powf(c * 2.f, C), Flatness); };
					float val = Cursor < .5f ? GetVal(Cursor) : -GetVal(1.f - Cursor);
					val = Distortion(DistortGain, val) * Volume;

					val = std::lerp(Oscillator.m_PrevVal, val, .4f);
					Oscillator.m_PrevVal = val;

					switch(OscillatorData.m_ModulationType)
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