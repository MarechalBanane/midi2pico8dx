// midi2pico8.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cstdlib>
#include <map>
#include <Windows.h>

#include "RtMidi.h"
#include "json.hpp"

using json = nlohmann::json;

#define LOG_ALL false

#define MESSAGE_TYPE_NOTE 144
#define MESSAGE_TYPE_PAD 153
#define MESSAGE_TYPE_BTN 191
#define MESSAGE_TYPE_KNOB 176

#define PADMODE_BTN 113

#define FINITE_KNOB 0
#define INFINITE_KNOB -1
#define INFINITE_KNOB_MIDVALUE 64

#define SPECIAL_VK_NUMPAD_SET -1
#define SPECIAL_VK_NUMPAD_SEND -2
#define MAX_NUMPAD_VALUE 7

// source for scan codes : https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-6.0/aa299374(v=vs.60)
// source for virtual keys : https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
// source for extended keys : https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#extended-key-flag

// On Axiom 25, by default:
// "finite" knobs (0-127) are cc 74 through 81.
// "infinite" knobs (+/-) are cc 146 through 152.
// 146 through 149 use cc defined in data3 (0 by default).
// 146: + = 65, - = 63
// 147: + =  1, - = 127
// 148: + = 65, - = 1 (redundant)
// 149: + = 1, - = 65 (redundant)
// 150 uses cc 96 (+) and 97 (-) with a constant val of 0.
// 151 uses cc 96 (+) and 97 (-) with a constant val of 1. cc 100 and 101 are set to 0 (data2) and 127 (data3) respectively before a series of values.
// 152 uses cc 96 (+) and 97 (-) with a constant val of 1. cc 98 and 99 are set to 0 (data2) and 127 (data3) respectively before a series of values.

int g_lastNumpadValue = 0;
bool g_padsAsNumpad=false;

typedef struct s_key
{
	short scs;
	bool ext;
	char name[10];
};

typedef struct s_knob
{
	short vkminus;
	short vkplus;

	// >= 0 for "finite knobs". Will be used to store the last sent value.
	// < 0 for "infinite knobs". These knobs are assumed to send < 64 for - and > 64 for +. (=> cc 146 on Axiom 25)
	// unused in SPECIAL_VK_NUMPAD case.
	short data0; 
};

std::map<short, s_key> g_keys = {
	{VK_DOWN,{0x50,true,"Down"}},
	{VK_NEXT,{0x51,true,"PgDown"}},
	{VK_LEFT,{0x4b,true,"Left"}},
	{VK_RIGHT,{0x4d,true,"Right"}},
	{VK_UP,{0x48,true,"Up"}},
	{VK_PRIOR,{0x49,true,"PgUp"}},
	{VK_HOME,{0x47,true,"Home"}},
	{VK_SUBTRACT,{0xc,false,"-"}},
	{VK_ADD,{0xd,false,"+"}},
	{VK_DELETE,{0x53,true,"Del"}},
	{VK_RETURN,{0x1c,false,"Enter"}},
	{VK_SPACE,{0x39,false,"Space"}},
	{VK_OEM_COMMA,{0x33,false,","}},
	{VK_OEM_PERIOD,{0x34,false,"."}},
	{VK_LMENU,{0x38,false,"Alt"}},
	{VK_LCONTROL,{0x1d,false,"Ctrl"}},
	{VK_LSHIFT,{0x2a,false,"Shift"}},
	{VK_TAB,{0x0f,false,"Tab"}},
	{VK_BACK,{0x0e,false,"Backspace"}},
	{VK_NUMPAD1,{0x4f,false,"Numpad1"}},
	{VK_NUMPAD2,{0x50,false,"Numpad2"}},
	{VK_NUMPAD3,{0x51,false,"Numpad3"}},
	{VK_NUMPAD4,{0x4b,false,"Numpad4"}},
	{VK_NUMPAD5,{0x4c,false,"Numpad5"}},
	{VK_NUMPAD6,{0x4d,false,"Numpad6"}},
	{VK_NUMPAD7,{0x47,false,"Numpad7"}},
	{VK_NUMPAD8,{0x48,false,"Numpad8"}},
	{VK_NUMPAD9,{0x49,false,"Numpad9"}},
	{VK_NUMPAD0,{0x52,false,"Numpad0"}},
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

json g_TestData =
{
	{48,'Z'},
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

std::map<char, short> g_pads = {
	{36,VK_LCONTROL},
	{38,VK_LEFT},
	{42,VK_DOWN},
	{46,VK_RIGHT},
	{50,VK_LSHIFT},
	{45,VK_LMENU},
	{51,VK_UP},
	{49,VK_TAB},
};

std::map<char, short> g_numpadPads = {
	{36,VK_NUMPAD4},
	{38,VK_NUMPAD5},
	{42,VK_NUMPAD6},
	{46,VK_NUMPAD7},
	{50,VK_NUMPAD0},
	{45,VK_NUMPAD1},
	{51,VK_NUMPAD2},
	{49,VK_NUMPAD3},
};

std::map<char, short> g_btns = {
	{114,VK_SUBTRACT},	// previous sound or pattern
	{115,VK_ADD},		// next sound or pattern
	{116,VK_DELETE},	// delete note
	{117,VK_RETURN},	// insert note
	{118,VK_SPACE},		// play/pause
};

std::map<char, s_knob> g_knobs = {
	{74,{ VK_OEM_COMMA, VK_OEM_PERIOD, INFINITE_KNOB }}, // sfx speed
	{75,{ SPECIAL_VK_NUMPAD_SET, SPECIAL_VK_NUMPAD_SET}}, // numpad set
	{76,{ SPECIAL_VK_NUMPAD_SEND, SPECIAL_VK_NUMPAD_SEND}}, // numpad send
	{78,{ VK_UP, VK_DOWN, INFINITE_KNOB }}, // up/down
	{79,{ VK_LEFT, VK_RIGHT, INFINITE_KNOB }}, // left/right
};

bool keypress(short vk, bool press, bool release)
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

			if (press)
			{
				SendInput(1, &ip, sizeof(INPUT));
			}

			if (release)
			{
				ip.ki.dwFlags |= KEYEVENTF_KEYUP;
				SendInput(1, &ip, sizeof(INPUT));
			}

			if (press and release)
				std::cout << "hit " << key.name << "\n";
			else if (press)
				std::cout << "press " << key.name << "\n";
			else if (release)
				std::cout << "release " << key.name << "\n";

			return true;
		}
	}

	return false;
}

void mycallback(double deltatime, std::vector< unsigned char > *message, void *userData)
{
	unsigned int nBytes = message->size();
	int type = message->at(0);
	if (type == MESSAGE_TYPE_NOTE)
	{
		int note = message->at(1);
		if (g_notes.find(note) != g_notes.end())
		{
			short vk = g_notes.at(note);
			bool press = message->at(2) != 0;
			if (!keypress(vk, press,!press))
			{
				std::cout << "note " << note << "\n";
			}
		}
	}
	else if (type == MESSAGE_TYPE_PAD)
	{
		int pad = message->at(1);
		std::map<char, short>* padsToUse;
		if (g_padsAsNumpad)
		{
			padsToUse = &g_numpadPads;
		}
		else
		{
			padsToUse = &g_pads;
		}

		if (padsToUse->find(pad) != padsToUse->end())
		{
			short vk = padsToUse->at(pad);
			bool press = message->at(2) != 0;
			if (!keypress(vk, press, !press))
			{
				std::cout << "pad " << pad << "\n";
			}
		}
	}
	else if (type == MESSAGE_TYPE_BTN)
	{
		int btnVal = (int)message->at(1);
		bool press = message->at(2) != 0;
		if (btnVal == PADMODE_BTN)
		{
			g_padsAsNumpad = press;
			if (g_padsAsNumpad)
			{
				std::cout << "pads in number mode\n";
			}
			else
			{
				std::cout << "pads in cursor mode\n";
			}
		}
		else if (g_btns.find(btnVal) != g_btns.end())
		{
			short vk = g_btns.at(btnVal);
			if (!keypress(vk, press, !press))
			{
				std::cout << "button " << btnVal << "\n";
			}
		}
	}
	else if (type == MESSAGE_TYPE_KNOB)
	{
		int knob = (int)message->at(1);
		int val = (int)message->at(2);

		if (g_knobs.find(knob) != g_knobs.end())
		{
			s_knob def = g_knobs.at(knob);
			if (def.vkminus == SPECIAL_VK_NUMPAD_SET)
			{
				val = val % (MAX_NUMPAD_VALUE+1);
				if (val != g_lastNumpadValue)
				{
					g_lastNumpadValue = val;
					std::cout << "virtual numpad set to " << g_lastNumpadValue << "\n";
				}
			}
			else if (def.vkminus == SPECIAL_VK_NUMPAD_SEND)
			{
				keypress(VK_NUMPAD0 + g_lastNumpadValue, true, true);
			}
			else if (def.data0 >= 0)
			{
				if (val <= def.data0)
				{
					keypress(def.vkminus, true, true);
				}
				else if (val > def.data0)
				{
					keypress(def.vkplus, true, true);
				}

				def.data0 = val;
				g_knobs[knob] = def;
			}
			else
			{
				if (val <= INFINITE_KNOB_MIDVALUE)
				{
					keypress(def.vkminus, true, true);
				}
				else
				{
					keypress(def.vkplus, true, true);
				}
			}
		}
		else
		{
			std::cout << "knob " << knob << ", val " << val << "\n";
		}
	}

	if (LOG_ALL)
	{
		for (unsigned int i = 0; i < nBytes; i++)
			std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
		if (nBytes > 0)
			std::cout << "stamp = " << deltatime << std::endl;
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
	midiin->openPort(0);

	while (midiin->getPortCount() > 0)
	{
		Sleep(200);
	}

	midiin->closePort();

	delete midiin;
	return 0;
}
