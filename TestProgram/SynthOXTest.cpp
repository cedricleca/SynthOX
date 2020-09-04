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

	float L, R;
	for(int i = 0; i < 255+255; i++)
		Synth.PopOutputVal(L, R);

	auto Scope = AnalogSource0.RenderScope(0, 44000);

    std::cout << "Hello World!\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
