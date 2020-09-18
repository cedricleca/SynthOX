
#pragma once

#include <vector>
#include <memory>
#include <array>
#include <utility>
#include <map>

namespace SynthOX
{
	enum class LFODest : char
	{
		Tune,
		Morph,
		Squish,
		Distort,
		Volume,
		Decat,
		Max
	};

	static const unsigned int PlaybackFreq = 44100;
	static const int PrimaryBufferSize = PlaybackFreq*2;

	enum class WaveType : char
	{
		Square,
		Saw,
		Triangle,
		Sine,
		Rand,
		Max,
	};

	enum class ModulationType
	{
		Mix,
		Mul,
		Ring,
		Max,
	};

	enum class PolyphonyMode : char
	{
		Poly,
		Arpeggio,
		Portamento,
	};

	extern float OctaveFreq[];
	class Synth;

	void FloatClear(float * Dest, long len);
	float Distortion(float _Gain, float _Sample);
	float GetNoteFreq(float _NoteCode);
	float GetWaveformValue(WaveType Type, float Cursor);

	//_________________________________________________
	template <class DataType = float, size_t Size = 16>
	struct SoundBuf
	{
		std::vector<DataType>	m_Data;
		long					m_WriteCursor = 0;

		SoundBuf()	{ m_Data.resize(Size); }
	};

	struct StereoSoundBuf : SoundBuf<std::pair<float, float>, PlaybackFreq>
	{
		void Clear(long NbSamples)
		{
			for(int i = 0; i < NbSamples; i++)
			{
				m_Data[m_WriteCursor] = {0.f, 0.f};
				m_WriteCursor = (m_WriteCursor + 1) % PlaybackFreq;
			}
		}
	};

	//_________________________________________________
	class Waveform : public SoundBuf<float, PlaybackFreq> 	{};

	//_________________________________________________
	class SoundSource
	{
	protected:
		StereoSoundBuf *	m_Dest;
		Synth *				m_Synth = nullptr;
		int					m_Channel = 0;

	public:
		SoundSource(StereoSoundBuf * Dest, int Channel) : m_Dest(Dest), m_Channel(Channel) {}
		virtual void OnBound(Synth * Synth) { m_Synth = Synth; }

		virtual void NoteOn(int _Channel, int _KeyId, float _Velocity)
		{
			if(_Channel==m_Channel)
				NoteOn(_KeyId, _Velocity);
		}
		virtual void NoteOff(int _Channel, int _KeyId)
		{
			if(_Channel==m_Channel)
				NoteOff(_KeyId);
		}
		virtual void NoteOn(int KeyId, float Velocity) = 0;
		virtual void NoteOff(int KeyId) = 0;
		virtual void Render(long SampleNr) = 0;
		virtual StereoSoundBuf & GetDest(){ return *m_Dest; }
	};

	//_________________________________________________
	class FilterSource : public SoundSource
	{
	protected:
		StereoSoundBuf	m_SrcWaveForm;

	public:
		long			m_Cursor;
	};

	//_________________________________________________
	template <size_t Size = 16>
	class EchoFilterSource : public FilterSource
	{
		SoundBuf<std::pair<float, float>, Size>	m_DelayWaveForm;
		float			m_S0 = 0.f;
		float			m_S1 = 0.f;

	public:
		long	m_DelayLen = 0;
		long	m_ResoDelayLen = 0;
		long	m_ResoSteps;
		float	m_ResoFeedback = 0.f;
		float	m_Feedback = 0.f;

		EchoFilterSource(long DelayLen, SoundSource * Dest, int Channel) : 
			FilterSource(Dest, Channel)
		{}

		void OnBound(Synth * Synth) override { SoundSource::OnBound(Synth); }
		virtual void Render(long _SampleNr) override;
	};

	//_________________________________________________
	class Note
	{
	public:
		float	m_Time = 0.f;
		float	m_SustainTime = 0.f;
		float	m_Velocity = 0.f;
		int		m_Code = 0;
		bool	m_Died = true;
		bool	m_NoteOn = false;

		void NoteOn(int _KeyId, float _Velocity)
		{
			if(!m_NoteOn)
			{
				m_Time = 0.0f; 
				m_SustainTime = 0.0f; 
				m_NoteOn = true; 
				m_Code = _KeyId;
				m_Velocity = _Velocity;
				m_Died = false;
			}
		}
		void NoteOff(){ m_NoteOn = false; }
	};


	//_________________________________________________
	struct LFOData
	{
		float				m_Delay = .0f;
		float				m_Attack = .1f;
		float				m_Magnitude = 0.f;
		float				m_Rate = .0f;
		float				m_BaseValue = 1.f;
		WaveType			m_WF = WaveType::Sine;
		char				m_NoteSync = 0;
	};

	//_________________________________________________
	class LFOTransients
	{
		float		m_Cursor = .0f;

	public:
		LFOData	*	m_Data = nullptr;
		bool		m_ZeroCentered = false;

		float GetUpdatedValue(float NoteTime);
		void NoteOn();
	};

	//_________________________________________________
	struct OscillatorData
	{
		LFOData			m_LFOTab[int(LFODest::Max)];
		char			m_OctaveOffset = 0;
		char			m_NoteOffset = 0;	
		ModulationType	m_ModulationType = ModulationType::Mul;
	};

	static const int AnalogsourceOscillatorNr = 2;
	static const int AnalogsourcePolyphonyNoteNr = 6;

	//_________________________________________________
	struct AnalogSourceData
	{
		OscillatorData			m_OscillatorTab[AnalogsourceOscillatorNr];
		float					m_ADSR_Attack = 0.f;
		float					m_ADSR_Decay = 0.f;
		float					m_ADSR_Sustain = 0.f;
		float					m_ADSR_Release = 0.f;
		float					m_LeftVolume = 0.f;
		float					m_RightVolume = 0.f;
		float					m_PortamentoTime = 0.f;
		PolyphonyMode			m_PolyphonyMode = PolyphonyMode::Poly;
	};

	//_________________________________________________
	class AnalogSource : public SoundSource
	{
		struct OscillatorTransients
		{
			LFOTransients	m_LFOTab[int(LFODest::Max)];
			float			m_Cursor = 0.f;
			float			m_PrevVal = 0.f;
		};

		struct AnalogSourceNote : Note
		{
			OscillatorTransients	m_OscillatorTab[AnalogsourceOscillatorNr];
			
			// fliter stuff
			float az1;
			float az2;
			float az3;
			float az4;
			float az5;
			float ay1;
			float ay2;
			float ay3;
			float ay4;
			float amf;
		};

	public:
		AnalogSourceData		* m_Data;
		AnalogSourceNote		m_NoteTab[AnalogsourcePolyphonyNoteNr];
		int						m_ArpeggioIdx = 0;
		float					m_PortamentoBaseNote = 0.f;
		float					m_PortamentoStep = 0.f;

		AnalogSource(StereoSoundBuf * Dest, int Channel, AnalogSourceData * Data);
		void OnBound(Synth * Synth) override;
		void NoteOn(int KeyId, float Velocity) override;
		void NoteOff(int KeyId) override;
		std::vector<float> RenderScope(int OscIdx, unsigned int NbSamples);
		void Render(long SampleNr) override;
		float GetADSRValue(Note & Note, float DTime);
	};

	//_________________________________________________
	class Synth
	{
		std::vector<SoundSource*>					m_SourceTab;

	public:
		StereoSoundBuf								m_OutBuf;

		void Render(unsigned int SamplesToRender);
		void NoteOn(int Channel, int KeyId, float Velocity);
		void NoteOff(int Channel, int KeyId);
		void BindSource(SoundSource & NewSource) { NewSource.OnBound(this); m_SourceTab.push_back(&NewSource); }
		void PopOutputVal(float & OutLeft, float & OutRight);
	};

}; // namespace SynthOX
