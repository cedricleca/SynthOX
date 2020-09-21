// SynthOXTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "../SynthOX.h"
#include <iostream>

int main()
{    
	SynthOX::Synth Synth;
	SynthOX::AnalogSourceData Data;
	SynthOX::AnalogSource AnalogSource0(&Synth.m_OutBuf, 0, &Data);
	Synth.BindSource(AnalogSource0);
	Synth.NoteOn(0, 10, 1.f);
	Synth.Render(255);
	Synth.NoteOff(0, 10);
	Synth.Render(255);

	for(int i = 0; i < 100; i++)
	{
		float L, R;
		for(int i = 0; i < 255+255; i++)
			Synth.PopOutputVal(L, R);

		auto Scope = AnalogSource0.RenderScope(0, 44000);
	}
}
