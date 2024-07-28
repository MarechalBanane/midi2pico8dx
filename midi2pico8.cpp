// midi2pico8.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <map>
#include <Windows.h>

#include "RtMidi.h"
#include "json.hpp"

using json = nlohmann::json;

#define CONFIG_FILE_NAME "config.json"
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

#define FINITE_KNOB 0
#define INFINITE_KNOB -1

#define JSTR_LOG_MIDI_MESSAGES	"log_midi_messages"
#define JSTR_MESSAGE_NOTE		"message_status_note"
#define JSTR_MESSAGE_PAD		"message_status_pad"
#define JSTR_MESSAGE_BTN		"message_status_btn"
#define JSTR_MESSAGE_KNOB		"message_status_knob"
#define JSTR_NUMPAD_MODE_CC		"numpad_mode_cc"
#define JSTR_INF_KNOB_MIDVALUE	"infinite_knob_midvalue"
#define JSTR_NOTE_INPUTS		"note_inputs"
#define JSTR_PAD_INPUTS			"pad_inputs"
#define JSTR_BTN_INPUTS			"btn_inputs"
#define JSTR_KNOB_INPUTS		"knob_inputs"
#define JSTR_NOTE				"note"
#define JSTR_KEY				"key"

// hardcoded specification of which inputs are available in JSON
std::map<std::string, short> g_jstrToVk =
{
	{"1", '1'},
	{"2", '2'},
	{"3", '3'},
	{"4", '4'},
	{"5", '5'},
	{"6", '6'},
	{"7", '7'},
	{"8", '8'},
	{"9", '9'},
	{"0", '0'},
	{"-", VK_SUBTRACT},
	{"+", VK_ADD},
	{"q", 'Q'},
	{"w", 'W'},
	{"e", 'E'},
	{"r", 'R'},
	{"t", 'T'},
	{"y", 'Y'},
	{"u", 'U'},
	{"i", 'I'},
	{"o", 'O'},
	{"p", 'P'},
	{"a", 'A'},
	{"s", 'S'},
	{"d", 'D'},
	{"f", 'F'},
	{"g", 'G'},
	{"h", 'H'},
	{"j", 'J'},
	{"k", 'K'},
	{"l", 'L'},
	{"z", 'Z'},
	{"x", 'X'},
	{"c", 'C'},
	{"v", 'V'},
	{"b", 'B'},
	{"n", 'N'},
	{"m", 'M'},
	{",", VK_OEM_COMMA},
	{".", VK_OEM_PERIOD},
	{"down", VK_DOWN},
	{"pgdown", VK_NEXT},
	{"left", VK_LEFT},
	{"right", VK_RIGHT},
	{"up", VK_UP},
	{"pgup", VK_PRIOR},
	{"home", VK_HOME},
	{"del", VK_DELETE},
	{"return", VK_RETURN},
	{"space", VK_SPACE},
	{"alt", VK_LMENU},
	{"ctrl", VK_LCONTROL},
	{"shift", VK_LSHIFT},
	{"tab", VK_TAB},
	{"backspace", VK_BACK},
	{"numpad1", VK_NUMPAD1},
	{"numpad2", VK_NUMPAD2},
	{"numpad3", VK_NUMPAD3},
	{"numpad4", VK_NUMPAD4},
	{"numpad5", VK_NUMPAD5},
	{"numpad6", VK_NUMPAD6},
	{"numpad7", VK_NUMPAD7},
	{"numpad8", VK_NUMPAD8},
	{"numpad9", VK_NUMPAD9},
	{"numpad0", VK_NUMPAD0},
};

json c_defaultConf =
{
	{JSTR_LOG_MIDI_MESSAGES,false},
	{JSTR_MESSAGE_NOTE,0x90},
	{JSTR_MESSAGE_PAD,0x99},
	{JSTR_MESSAGE_BTN,0xbf},
	{JSTR_MESSAGE_KNOB,0xb0},
	{JSTR_NUMPAD_MODE_CC,113},
	{JSTR_INF_KNOB_MIDVALUE,64},
	{JSTR_NOTE_INPUTS,{
		{{JSTR_NOTE, 48}, {JSTR_KEY, "z"}},
		{{JSTR_NOTE, 49}, {JSTR_KEY, "s"}},
		{{JSTR_NOTE, 50}, {JSTR_KEY, "x"}},
		{{JSTR_NOTE, 51}, {JSTR_KEY, "d"}},
		{{JSTR_NOTE, 52}, {JSTR_KEY, "c"}},
		{{JSTR_NOTE, 53}, {JSTR_KEY, "v"}},
		{{JSTR_NOTE, 54}, {JSTR_KEY, "g"}},
		{{JSTR_NOTE, 55}, {JSTR_KEY, "b"}},
		{{JSTR_NOTE, 56}, {JSTR_KEY, "h"}},
		{{JSTR_NOTE, 57}, {JSTR_KEY, "n"}},
		{{JSTR_NOTE, 58}, {JSTR_KEY, "j"}},
		{{JSTR_NOTE, 59}, {JSTR_KEY, "m"}},
		{{JSTR_NOTE, 60}, {JSTR_KEY, "q"}},
		{{JSTR_NOTE, 61}, {JSTR_KEY, "2"}},
		{{JSTR_NOTE, 62}, {JSTR_KEY, "w"}},
		{{JSTR_NOTE, 63}, {JSTR_KEY, "3"}},
		{{JSTR_NOTE, 64}, {JSTR_KEY, "e"}},
		{{JSTR_NOTE, 65}, {JSTR_KEY, "r"}},
		{{JSTR_NOTE, 66}, {JSTR_KEY, "5"}},
		{{JSTR_NOTE, 67}, {JSTR_KEY, "t"}},
		{{JSTR_NOTE, 68}, {JSTR_KEY, "6"}},
		{{JSTR_NOTE, 69}, {JSTR_KEY, "y"}},
		{{JSTR_NOTE, 70}, {JSTR_KEY, "7"}},
		{{JSTR_NOTE, 71}, {JSTR_KEY, "u"}},
		{{JSTR_NOTE, 72}, {JSTR_KEY, "i"}},
		{{JSTR_NOTE, 73}, {JSTR_KEY, "9"}},
		{{JSTR_NOTE, 74}, {JSTR_KEY, "o"}},
		{{JSTR_NOTE, 75}, {JSTR_KEY, "0"}},
		{{JSTR_NOTE, 76}, {JSTR_KEY, "p"}},
	}},
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

json* g_currentConf = 0;

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
	if (type == g_currentConf->at(JSTR_MESSAGE_NOTE))
	{
		bool found = false;
		int note = message->at(1);
		if (g_currentConf->contains(JSTR_NOTE_INPUTS))
		{
			json noteArray = g_currentConf->at(JSTR_NOTE_INPUTS);
			for (int i = 0; i < noteArray.size(); ++i)
			{
				json noteData = noteArray[i];
				if (noteData.at(JSTR_NOTE) == note)
				{
					auto key = noteData.at(JSTR_KEY);
					short vk = g_jstrToVk.at(key);
					bool press = message->at(2) != 0;
					found = keypress(vk, press, !press);
					break;
				}
			}
		}
		if (!found)
		{
			std::cout << "note " << note << "\n";
		}
	}
	else if (type == g_currentConf->at(JSTR_MESSAGE_PAD))
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
	else if (type == g_currentConf->at(JSTR_MESSAGE_BTN))
	{
		int btnVal = (int)message->at(1);
		bool press = message->at(2) != 0;
		if (btnVal == g_currentConf->at(JSTR_NUMPAD_MODE_CC))
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
	else if (type == g_currentConf->at(JSTR_MESSAGE_KNOB))
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
				if (val <= g_currentConf->at(JSTR_INF_KNOB_MIDVALUE))
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

	if (g_currentConf->at(JSTR_LOG_MIDI_MESSAGES))
	{
		if (nBytes == 3)
		{
			std::cout << "Status = " << (int)message->at(0) << ", ";
			std::cout << "Data1 = " << (int)message->at(1) << ", ";
			std::cout << "Data2 = " << (int)message->at(2) << "\n";
		}
		else if (nBytes > 0)
		{
			for (unsigned int i = 0; i < nBytes; i++)
			{
				std::cout << "Byte " << i << " = " << (int)message->at(i);
				if (i < nBytes-1)
				{
					std::cout << ", ";
				}
			}

			std::cout << "\n";
		}
	}
}

int main()
{
	std::cout << "==================\n";
	std::cout << "* MIDI to PICO-8 *\n";
	std::cout << "==================\n\n";

	// load config file
	json data;
	std::cout << "Loading '" << CONFIG_FILE_NAME << "'...\n";
	std::ifstream confFile(CONFIG_FILE_NAME);
	if (confFile.fail())
	{
		std::cout << "Could not load '" << CONFIG_FILE_NAME << "', revert to default config.\n";
		g_currentConf = &c_defaultConf;
	}
	else
	{
		try
		{
			data = json::parse(confFile, nullptr, true, true);
			g_currentConf = &data;
		}
		catch (json::parse_error& ex)
		{
			std::cerr << "parse error at byte " << ex.byte << ": " << ex.what() << std::endl;
			std::cout << "Error while loading config file, revert to default config.\n";
			g_currentConf = &c_defaultConf;
		}
	}

	confFile.close();

	// setup midi callback
	RtMidiIn *midiin = new RtMidiIn();
	midiin->setCallback(&mycallback);
	midiin->ignoreTypes(true, true, true);
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
