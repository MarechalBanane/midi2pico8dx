// midi2pico8.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cstdlib>
#include <Windows.h>
#include <map>

#include "../RtMidi.h"

#define MESSAGE_TYPE_NOTE 144
#define MESSAGE_TYPE_PAD 153
#define MESSAGE_TYPE_BTN 191
#define MESSAGE_TYPE_KNOB 176

int oldKnobValue = 0;
std::vector< unsigned char >* old_message;
std::vector<std::string> padkeyNames = { "Down", "PgDown", "Left", "Right", "Up", "PgUp", "", "" };
std::vector<std::string> buttonkeyNames = { "Home", "-", "+", "Del", "Enter", "Space"};

typedef struct s_key
{
	int scs;
	bool ext;
	char name[10];
};

std::map<char, s_key> g_keys = {
	{VK_DOWN,{0x50,true,"Down"}},
	{VK_NEXT,{0x51,true,"PgDown"}},
	{VK_LEFT,{0x4b,true,"Left"}},
	{VK_RIGHT,{0x4d,true,"Right"}},
	{VK_UP,{0x48,true,"Up"}},
	{VK_PRIOR,{0x49,true,"PgUp"}},
	{'Z',{0x2c,false,"Z"}},
	{'S',{0x1f,false,"S"}},
	{'X',{0x2d,false,"X"}},
	{'D',{0x20,false,"D"}},
	{'C',{0x2e,false,"C"}},
	{'V',{0x2f,false,"V"}},
	{'G',{0x22,false,"G"}},
	{'B',{0x30,false,"B"}},
	{'H',{0x23,false,"H"}},
	{'N',{0x31,false,"N"}},
	{'J',{0x24,false,"J"}},
	{'M',{0x32,false,"M"}},
	{'Q',{0x10,false,"Q"}},
	{'2',{0x3,false,"2"}},
	{'W',{0x11,false,"W"}},
	{'3',{0x4,false,"3"}},
	{'E',{0x12,false,"E"}},
	{'R',{0x13,false,"R"}},
	{'5',{0x6,false,"5"}},
	{'T',{0x14,false,"T"}},
	{'6',{0x7,false,"6"}},
	{'Y',{0x15,false,"Y"}},
	{'7',{0x8,false,"7"}},
	{'U',{0x16,false,"U"}},
	{'I',{0x17,false,"I"}},
	{'9',{0xa,false,"9"}},
	{'O',{0x18,false,"O"}},
	{'0',{0xb,false,"0"}},
	{'P',{0x19,false,"P"}},
};

std::map<char, char> g_notes = {
	{48,'Z'},
	{49,'S'},
	{50,'X'},
	{51,'D'},
	{52,'C'},
	{53,'V'},
	{54,'G'},
	{55,'B'},
	{56,'H'},
	{57,'N'},
	{58,'J'},
	{59,'M'},
	{60,'Q'},
	{61,'2'},
	{62,'W'},
	{63,'3'},
	{64,'E'},
	{65,'R'},
	{66,'5'},
	{67,'T'},
	{68,'6'},
	{69,'Y'},
	{70,'7'},
	{71,'U'},
	{72,'I'},
	{73,'9'},
	{74,'O'},
	{75,'0'},
	{76,'P'},
};

std::map<char,char> g_pads = {
	{36,VK_DOWN},
	{38,VK_NEXT},
	{42,VK_LEFT},
	{46,VK_RIGHT},
	{50,VK_UP},
	{45,VK_PRIOR},
	{51,0},
	{49,0},
};

bool keyhit(char vk)
{
	if (vk != 0)
	{
		s_key key = g_keys.at(vk);
		if (key.scs != 0)
		{
			INPUT ip;
			ip.type = INPUT_KEYBOARD;
			ip.ki.time = 0;
			ip.ki.wVk = vk;
			ip.ki.wScan = key.scs;
			ip.ki.dwExtraInfo = GetMessageExtraInfo();
			ip.ki.dwFlags = 0;
			if (key.ext)
			{
				ip.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
				ip.ki.wScan |= 0xE000;
			}

			SendInput(1, &ip, sizeof(INPUT));
			ip.ki.dwFlags |= KEYEVENTF_KEYUP;
			SendInput(1, &ip, sizeof(INPUT));
			std::cout << "key hit " << key.name << "\n";

			return true;
		}
	}

	return false;
}

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
		int note = message->at(1);
		char vk = g_notes.at(note);
		if (vk!=0)
		{
			if (!keyhit(vk))
			{
				std::cout << "note " << note << "\n";
			}
		}

		old_message = message;
	}
	else
	{
		// only correct for axiom 25 with default settings
		if (type == MESSAGE_TYPE_PAD)
		{
			int pad = message->at(1);
			if (message->at(2) != 0)
			{
				char vk=g_pads.at(pad);
				if (!keyhit(vk))
				{
					std::cout << "pad " << pad << "\n";
				}
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
