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
			for(int j = 0; j < int(LFODest::Max); j++)
				m_OscillatorTab[i].m_LFOTab[j].Init(&m_Data->m_OscillatorTab[i].m_LFOTab[j], *m_Synth);
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
	float AnalogSource::GetADSRValue(Note & Note, float Time)
	{
		if(Note.m_NoteOn)
		{
			if(Note.m_Time > m_Data->m_ADSR_Attack + m_Data->m_ADSR_Decay)
			{
				Note.m_SustainTime += Time;
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
	void AnalogSource::Render(long SampleNr)
	{
		long wc = m_Dest->m_WriteCursor;
		long wcSav = wc;

		float time = (float)SampleNr / PlaybackFreq;

		// update Low freq oscillators
		for(int i = 0; i < AnalogsourceOscillatorNr; i++)
			for(int j = 0; j < int(LFODest::Max); j++)
				m_OscillatorTab[i].m_LFOTab[j].Update(time);
	
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

		int PolyNoteNr = (m_Data->m_PolyphonyMode == PolyphonyMode::Poly ? AnalogsourcePolyphonyNoteNr : 1);
		for(int k = 0; k < PolyNoteNr; k++)
		{
			auto & Note = m_NoteTab[k];
			Note.m_Time += time;
	
			// Get ADSR and Velocity
			float VolumeMultiplier = GetADSRValue(Note, time) * Note.m_Velocity * 1.5f; // ???
			if(VolumeMultiplier == 0.0f)
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
					float Newfreq = m_PortamentoCurFreq + m_PortamentoStep * time;
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
				Oscillator.m_Volume			= m_OscillatorTab[j].m_LFOTab[int(LFODest::Volume )].GetValue(Note.m_Time);
				Oscillator.m_Morph			= m_OscillatorTab[j].m_LFOTab[int(LFODest::Morph  )].GetValue(Note.m_Time);
				Oscillator.m_DistortGain	= m_OscillatorTab[j].m_LFOTab[int(LFODest::Distort)].GetValue(Note.m_Time);
				Oscillator.m_StepShift		= m_OscillatorTab[j].m_LFOTab[int(LFODest::Tune   )].GetValue(Note.m_Time, true);

				for(int i = 0; i < m_Data->m_OscillatorTab[j].m_OctaveOffset; i++)	NoteFreq *= 2.0f;
				for(int i = 0; i > m_Data->m_OscillatorTab[j].m_OctaveOffset; i--)	NoteFreq *= 0.5f;
			
				Oscillator.m_Step	= std::min(NoteFreq + Oscillator.m_StepShift, 0.f);
				Oscillator.m_Morph	= std::clamp(Oscillator.m_Morph, 0.f, 1.f);
				Oscillator.m_Volume	= std::min(Oscillator.m_Volume, 0.f);
				Oscillator.m_Volume	*= VolumeMultiplier;

				// calc interp value
				Oscillator.m_VolumeInterp = (Oscillator.m_Volume - Note.m_InterpTab[j].m_Volume) / SampleNr;
				Oscillator.m_MorphInterp = (Oscillator.m_Morph - Note.m_InterpTab[j].m_Morph) / SampleNr;
				Oscillator.m_DistortGainInterp = (Oscillator.m_DistortGain - Note.m_InterpTab[j].m_DistortGain) / SampleNr;
			}

			wc = wcSav;

			// compute samples
			for(long i = 0; i < SampleNr; i++)
			{
				float Output = 0.0f;
				for(int j = 0; j < AnalogsourceOscillatorNr; j++)
				{
					const auto & Oscillator = m_OscillatorTab[j];
					auto & NoteInterp = Note.m_InterpTab[j];

					float val;
					const float alpha = .4f + .6f * NoteInterp.m_Morph;
					const float C = std::powf(alpha, 10.f) * 30.f;
			        const float Flatness = 2.f; // [2., 8.]
					if(NoteInterp.m_Cursor < .5f)
					{
						const float XX = pow(NoteInterp.m_Cursor * 2.f, C);
						val = .5f * (1.f - std::powf((2.f * XX - 1.f), Flatness));
					}
					else
					{
						const float XX = pow((NoteInterp.m_Cursor - .5f) * 2.f, C);
						val = .5f * std::powf((2.f * XX - 1.f), Flatness);
					}

					val = Distortion(NoteInterp.m_DistortGain, val);
					val *= NoteInterp.m_Volume;

					switch(m_Data->m_OscillatorTab[j].m_ModulationType)
					{
					case ModulationType::Mix:	Output += val;	break;
					case ModulationType::Mul:	Output *= val;	break;
					case ModulationType::Ring:	Output *= 1.0f - 0.5f*(val+NoteInterp.m_Volume);	break;
					}

					// interp
					NoteInterp.m_Volume			+= Oscillator.m_VolumeInterp;
					NoteInterp.m_Morph			+= Oscillator.m_MorphInterp;
					NoteInterp.m_DistortGain	+= Oscillator.m_DistortGainInterp;

					// avance le curseur de lecture de l'oscillateur
					NoteInterp.m_Cursor += Oscillator.m_Step;		
					while(unsigned int(NoteInterp.m_Cursor) >= PlaybackFreq)
						NoteInterp.m_Cursor -= PlaybackFreq;
				}

				m_Dest->m_Data[wc] = { Output * m_Data->m_LeftVolume, Output * m_Data->m_RightVolume };
				wc = (wc + 1) % PlaybackFreq;
			}
		}
	}

};