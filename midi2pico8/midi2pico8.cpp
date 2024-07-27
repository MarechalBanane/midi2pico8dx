// midi2pico8.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cstdlib>
#include <Windows.h>

#include "../RtMidi.h"

#define MESSAGE_TYPE_NOTE 144
#define MESSAGE_TYPE_PAD 153
#define MESSAGE_TYPE_BTN 191
#define MESSAGE_TYPE_KNOB 176

int oldKnobValue = 0;
std::vector< unsigned char >* old_message;
std::vector<std::string> padkeyNames = { "Down", "PgDown", "Left", "Right", "Up", "PgUp", "", "" };
std::vector<std::string> buttonkeyNames = { "Home", "-", "+", "Del", "Enter", "Space"};

typedef struct s_pad
{
	int cc;
	char vk;
	int scs;
};

std::vector<s_pad> pads = {
	{36,VK_DOWN,0x50},
	{38,VK_NEXT,0x51},
	{42,VK_LEFT,0x4b},
	{46,VK_RIGHT,0x4d},
	{50,VK_UP,0x48},
	{45,VK_PRIOR,0x49},
	{51,0,0},
	{49,0,0},
};

void mycallback(double deltatime, std::vector< unsigned char > *message, void *userData)
{

	if (message == old_message)
	{
		old_message = NULL;
		return;
	}
	unsigned int nBytes = message->size();
	int type = message->at(0);
	if (type == MESSAGE_TYPE_NOTE)
	{
		// 36 -> 84
		int note = message->at(1);
		char vks[] = "ZSXDCVGBHNJMQ2W3ER5T6Y7UI9O0P";
		int scs[] =
		{
			0x2c, 0x1f, 0x2d, 0x20, 0x2e, 0x2f, 0x22, 0x30, 0x23, 0x31, 0x24, 0x32,
			0x10, 0x3, 0x11, 0x4, 0x12, 0x13, 0x6, 0x14, 0x7, 0x15, 0x8, 0x16, 0x17, 0xa, 0x18, 0xb, 0x19
		};
		if (note >= 48 && note <= 76)
		{
			auto vk = vks[note - 48];
			auto sc = scs[note - 48];
			auto hCurrentWindow = GetForegroundWindow();

			INPUT ip;
			ip.type = INPUT_KEYBOARD;
			ip.ki.time = 0;
			ip.ki.wVk = vk;
			ip.ki.wScan = sc; // note - 48; //VK_RETURN is the code of Return key

			ip.ki.dwExtraInfo = 0;
			ip.ki.dwFlags = 0;
			SendInput(1, &ip, sizeof(INPUT));
			ip.ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, &ip, sizeof(INPUT));

			std::cout << "note " << note << ", key " << vk << "\n";

			old_message = message;
		}
	}
	else
	{
		
		// only correct for axiom 25 with default settings
		if (type == MESSAGE_TYPE_PAD)
		{
			int pad = message->at(1);
			if (message->at(2) != 0)
			{
				bool found = false;
				for (int i = 0; i < pads.size(); i++)
				{
					s_pad ipad = pads[i];
					if (pad == ipad.cc)
					{
						if (ipad.vk != 0)
						{
							INPUT ip;
							ip.type = INPUT_KEYBOARD;
							ip.ki.time = 0;
							ip.ki.wVk = ipad.vk;
							ip.ki.wScan = 0xE000 | ipad.scs;
							ip.ki.dwExtraInfo = GetMessageExtraInfo();
							ip.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
							SendInput(1, &ip, sizeof(INPUT));
							ip.ki.dwFlags |= KEYEVENTF_KEYUP;
							SendInput(1, &ip, sizeof(INPUT));
							found = true;
							std::cout << "pad " << pad << ", key " << padkeyNames[i] << "\n";
							break;
						}
					}
				}
				if (!found)
					std::cout << "pad " << pad << "\n";
			}
		}
		else if (type == MESSAGE_TYPE_BTN)
		{
			if (message->at(2) != 0)
			{
				int btnVal = (int)message->at(1);
				if (btnVal >= 113 && btnVal <= 118)
				{
					// HOME, UP, DOWN, DEL, ENTER, SPACE 
					char vks[] = { VK_HOME, VK_SUBTRACT, VK_ADD, VK_DELETE, VK_RETURN, VK_SPACE };

					int scs[] =	{ 0x47, 0xc, 0xd, 0x53, 0x1c, 0x39};
					auto vk = vks[btnVal - 113];
					auto sc = scs[btnVal - 113];

					INPUT ip;
					ip.type = INPUT_KEYBOARD;
					ip.ki.time = 0;
					ip.ki.wVk = vk;
					ip.ki.wScan = sc;

					ip.ki.dwExtraInfo = GetMessageExtraInfo();
					ip.ki.dwFlags = 0;
					if (btnVal==113 || btnVal==116)
					{
						ip.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
						ip.ki.wScan |= 0xE000;
					}
					SendInput(1, &ip, sizeof(INPUT));
					ip.ki.dwFlags |= KEYEVENTF_KEYUP;
					SendInput(1, &ip, sizeof(INPUT));

					std::cout << "button " << btnVal << ", key " << buttonkeyNames[btnVal-113] << "\n";
				}
				else
				{
					std::cout << "button " << btnVal << "\n";
				}
			}
		}
		else if (type == MESSAGE_TYPE_KNOB)
		{
			int knob = (int)message->at(1);
			int val = (int)message->at(2);

			if (knob == 74)
			{
				if (val <= oldKnobValue)
				{
					INPUT ip;
					ip.type = INPUT_KEYBOARD;
					ip.ki.time = 0;
					ip.ki.wVk = VK_OEM_COMMA;
					ip.ki.wScan = 0x33;
					ip.ki.dwExtraInfo = GetMessageExtraInfo();
					ip.ki.dwFlags = 0;
					SendInput(1, &ip, sizeof(INPUT));
					ip.ki.dwFlags |= KEYEVENTF_KEYUP;
					SendInput(1, &ip, sizeof(INPUT));
					std::cout << "knob " << knob << ", val " << val << ", key comma\n";
				}
				else if (val > oldKnobValue)
				{
					INPUT ip;
					ip.type = INPUT_KEYBOARD;
					ip.ki.time = 0;
					ip.ki.wVk = VK_OEM_PERIOD;
					ip.ki.wScan = 0x34;
					ip.ki.dwExtraInfo = GetMessageExtraInfo();
					ip.ki.dwFlags = 0;
					SendInput(1, &ip, sizeof(INPUT));
					ip.ki.dwFlags |= KEYEVENTF_KEYUP;
					SendInput(1, &ip, sizeof(INPUT));
					std::cout << "knob " << knob << ", val " << val <<  ", key period\n";

				}
				oldKnobValue = val;
			}
			else
			{
				std::cout << "knob " << knob << ", val " << val << "\n";
			}
		}
		else
		{
			for (unsigned int i = 0; i < nBytes; i++)
				std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
			if (nBytes > 0)
				std::cout << "stamp = " << deltatime << std::endl;
		}
	}
}

int main()
{
	RtMidiIn *midiin = new RtMidiIn();
	midiin->setCallback(&mycallback);
	midiin->ignoreTypes(true, true, true);

	std::cout << "MIDI to PICO-8\n\n";
	std::cout << "Waiting for a MIDI input device...\n";

	while (midiin->getPortCount() == 0)
		Sleep(200);

	std::cout << "Reading MIDI input from " << midiin->getPortName(0) << "...\n";
	std::cout << "Press Q to quit.\n\n";
	midiin->openPort(0);

	while (midiin->getPortCount() > 0)
	{
		Sleep(200);
		if (GetAsyncKeyState('Q') & 0x8000)
			break;
	}

	midiin->closePort();

	delete midiin;
	return 0;
}
