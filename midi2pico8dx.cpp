// midi2pico8dx.cpp : This file contains the 'main' function. Program execution begins and ends there.
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
bool g_padsMode1=false;

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

#define JSTR_LOG_MIDI_MESSAGES	"log_midi_messages"
#define JSTR_MESSAGE_NOTE		"message_status_note"
#define JSTR_MESSAGE_PAD		"message_status_pad"
#define JSTR_MESSAGE_BTN		"message_status_btn"
#define JSTR_MESSAGE_KNOB		"message_status_knob"
#define JSTR_PAD_MODE_CC		"pad_mode_cc"
#define JSTR_INF_KNOB_MIDVALUE	"infinite_knob_midvalue"
#define JSTR_NOTE_INPUTS		"note_inputs"
#define JSTR_PAD0_INPUTS		"pad0_inputs"
#define JSTR_PAD1_INPUTS		"pad1_inputs"
#define JSTR_BTN_INPUTS			"btn_inputs"
#define JSTR_KNOB_INPUTS		"knob_inputs"
#define JSTR_DATA				"data"
#define JSTR_INPUT				"input"
#define JSTR_INPUTM				"input-"
#define JSTR_INPUTP				"input+"

#define JSTR_SINPUT_NUMPADSET	"numpadset"
#define JSTR_SINPUT_NUMPADSEND	"numpadsend"

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

// hardcoded fallback configuration, if none can be loaded.
json c_defaultConf =
{
	{JSTR_LOG_MIDI_MESSAGES,false},
	{JSTR_MESSAGE_NOTE,0x90},
	{JSTR_MESSAGE_PAD,0x99},
	{JSTR_MESSAGE_BTN,0xbf},
	{JSTR_MESSAGE_KNOB,0xb0},
	{JSTR_PAD_MODE_CC,113},
	{JSTR_INF_KNOB_MIDVALUE,64},
	{JSTR_NOTE_INPUTS,{
		{{JSTR_DATA, 48}, {JSTR_INPUT, "z"}},
		{{JSTR_DATA, 49}, {JSTR_INPUT, "s"}},
		{{JSTR_DATA, 50}, {JSTR_INPUT, "x"}},
		{{JSTR_DATA, 51}, {JSTR_INPUT, "d"}},
		{{JSTR_DATA, 52}, {JSTR_INPUT, "c"}},
		{{JSTR_DATA, 53}, {JSTR_INPUT, "v"}},
		{{JSTR_DATA, 54}, {JSTR_INPUT, "g"}},
		{{JSTR_DATA, 55}, {JSTR_INPUT, "b"}},
		{{JSTR_DATA, 56}, {JSTR_INPUT, "h"}},
		{{JSTR_DATA, 57}, {JSTR_INPUT, "n"}},
		{{JSTR_DATA, 58}, {JSTR_INPUT, "j"}},
		{{JSTR_DATA, 59}, {JSTR_INPUT, "m"}},
		{{JSTR_DATA, 60}, {JSTR_INPUT, "q"}},
		{{JSTR_DATA, 61}, {JSTR_INPUT, "2"}},
		{{JSTR_DATA, 62}, {JSTR_INPUT, "w"}},
		{{JSTR_DATA, 63}, {JSTR_INPUT, "3"}},
		{{JSTR_DATA, 64}, {JSTR_INPUT, "e"}},
		{{JSTR_DATA, 65}, {JSTR_INPUT, "r"}},
		{{JSTR_DATA, 66}, {JSTR_INPUT, "5"}},
		{{JSTR_DATA, 67}, {JSTR_INPUT, "t"}},
		{{JSTR_DATA, 68}, {JSTR_INPUT, "6"}},
		{{JSTR_DATA, 69}, {JSTR_INPUT, "y"}},
		{{JSTR_DATA, 70}, {JSTR_INPUT, "7"}},
		{{JSTR_DATA, 71}, {JSTR_INPUT, "u"}},
		{{JSTR_DATA, 72}, {JSTR_INPUT, "i"}},
		{{JSTR_DATA, 73}, {JSTR_INPUT, "9"}},
		{{JSTR_DATA, 74}, {JSTR_INPUT, "o"}},
		{{JSTR_DATA, 75}, {JSTR_INPUT, "0"}},
		{{JSTR_DATA, 76}, {JSTR_INPUT, "p"}},
	}},
};

// hardcoded key definitions that can be used in this software.
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

bool trySendInput(const char* inputArrayJStr, int data1, int data2)
{
	if (g_currentConf->contains(inputArrayJStr))
	{
		json inputArray = g_currentConf->at(inputArrayJStr);
		for (int i = 0; i < inputArray.size(); ++i)
		{
			json inputData = inputArray[i];
			if (inputData.at(JSTR_DATA) == data1)
			{
				auto key = inputData.at(JSTR_INPUT);
				short vk = g_jstrToVk.at(key);
				bool press = data2 != 0;
				return keypress(vk, press, !press);
			}
		}
	}

	return false;
}

bool trySendKnobInput(int data1, int data2)
{
	if (g_currentConf->contains(JSTR_KNOB_INPUTS))
	{
		json knobInputArray = g_currentConf->at(JSTR_KNOB_INPUTS);
		for (int i = 0; i < knobInputArray.size(); ++i)
		{
			json knobInputData = knobInputArray[i];
			if (knobInputData.at(JSTR_DATA) == data1)
			{
				auto inputMinus = knobInputData.at(JSTR_INPUTM);
				if (inputMinus == JSTR_SINPUT_NUMPADSET)
				{
					data2 = data2 % (MAX_NUMPAD_VALUE + 1);
					if (data2 != g_lastNumpadValue)
					{
						g_lastNumpadValue = data2;
						std::cout << "virtual numpad set to " << g_lastNumpadValue << "\n";
					}

					return true;
				}
				else if (inputMinus == JSTR_SINPUT_NUMPADSEND)
				{
					return keypress(VK_NUMPAD0 + g_lastNumpadValue, true, true);
				}
				else
				{
					auto inputPlus = knobInputData.at(JSTR_INPUTP);
					short vkminus = g_jstrToVk.at(inputMinus);
					short vkplus = g_jstrToVk.at(inputPlus);
					if (data2 <= g_currentConf->at(JSTR_INF_KNOB_MIDVALUE))
					{
						return keypress(vkminus, true, true);
					}
					else
					{
						return keypress(vkplus, true, true);
					}
				}
			}
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
		int note = message->at(1);
		if (!trySendInput(JSTR_NOTE_INPUTS, note, message->at(2)))
		{
			std::cout << "note " << note << "\n";
		}
	}
	else if (type == g_currentConf->at(JSTR_MESSAGE_PAD))
	{
		int pad = message->at(1);
		if (g_padsMode1)
		{
			if (!trySendInput(JSTR_PAD1_INPUTS, pad, message->at(2)))
			{
				std::cout << "pad " << pad << "\n";
			}
		}
		else
		{
			if (!trySendInput(JSTR_PAD0_INPUTS, pad, message->at(2)))
			{
				std::cout << "pad " << pad << "\n";
			}
		}
	}
	else if (type == g_currentConf->at(JSTR_MESSAGE_BTN))
	{
		int btnVal = (int)message->at(1);
		if (btnVal == g_currentConf->at(JSTR_PAD_MODE_CC))
		{
			g_padsMode1 = message->at(2) != 0;
			if (g_padsMode1)
			{
				std::cout << "pads mode 1\n";
			}
			else
			{
				std::cout << "pads mode 0\n";
			}
		}
		else
		{
			if (!trySendInput(JSTR_BTN_INPUTS, btnVal, message->at(2)))
			{
				std::cout << "button " << btnVal << "\n";
			}
		}
	}
	else if (type == g_currentConf->at(JSTR_MESSAGE_KNOB))
	{
		int knob = (int)message->at(1);
		int val = (int)message->at(2);

		if (!trySendKnobInput(knob, val))
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
