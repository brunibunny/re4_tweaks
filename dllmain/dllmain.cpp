#include <iostream>
#include "..\includes\stdafx.h"
#include "..\Wrappers\wrapper.h"

std::string RealDllPath;
std::string WrapperMode;
std::string WrapperName;

HMODULE wrapper_dll = nullptr;
HMODULE proxy_dll = nullptr;

//#define VERBOSE

float fFOVAdditional = 0.0f;
float fFOVScale = 0.1f;

bool shouldflip;
bool isItemUp;
bool FixSniperZoom;
bool RestorePickupTransparency;

uintptr_t jmpAddrFOV;
uintptr_t jmpAddrFlip;
uintptr_t jmpAddrItemDown;
uintptr_t jmpAddrItemUp;

int flipdirection;
int KB_flip_up;
int KB_flip_down;
int KB_flip_left;
int KB_flip_right;

void __declspec(naked) ScaleFOV()
{
	_asm
	{
		fld    dword ptr[ebx + 0x0000011C]
		fld    dword ptr ds : [fFOVAdditional]
		fld    dword ptr ds : [fFOVScale]
		fmul
		fadd
		jmp    jmpAddrFOV
	}
}

void __declspec(naked) FlipInv()
{
	if (shouldflip)
	{
		shouldflip = false;
		_asm {mov eax, flipdirection}
	}
	_asm
	{
		test eax, 0xC00000
		jmp jmpAddrFlip
	}
}

void __declspec(naked) invItemDown()
{
	isItemUp = false;
	_asm
	{
		and byte ptr[eax + 0x0000008A], -0x10
		jmp    jmpAddrItemDown
	}
}

void __declspec(naked) invItemUp()
{
	isItemUp = true;
	_asm
	{
		and byte ptr[eax + 0x0000008A], 0x0F
		jmp    jmpAddrItemUp
	}
}

void ReadSettings()
{
	CIniReader iniReader("");
	fFOVAdditional = iniReader.ReadFloat("CAMERA", "FOVAdditional", 0.0f);
	FixSniperZoom = iniReader.ReadBoolean("CAMERA", "FixSniperZoom", true);
	RestorePickupTransparency = iniReader.ReadBoolean("MISC", "RestorePickupTransparency", true);
	KB_flip_up = iniReader.ReadInteger("KEYBOARD", "KB_flip_up", 0x24);
	KB_flip_down = iniReader.ReadInteger("KEYBOARD", "KB_flip_down", 0x23);
	KB_flip_left = iniReader.ReadInteger("KEYBOARD", "KB_flip_left", 0x2D);
	KB_flip_right = iniReader.ReadInteger("KEYBOARD", "KB_flip_right", 0x21);
}


DWORD WINAPI Init(LPVOID)
{
	std::cout << "Big ironic thanks to QLOC S.A." << std::endl;

	ReadSettings();

	//FOV
	if (fFOVAdditional != 0.0f)
	{
		auto pattern = hook::pattern("DC ? D9 ? DE ? D9 ? D9 ? ? ? ? ? D9 ? ? ? ? ? DE");
		injector::MakeNOP(pattern.get_first(14), 6, true);
		injector::MakeJMP(pattern.get_first(14), ScaleFOV, true);
		jmpAddrFOV = (uintptr_t)pattern.count(1).get(0).get<uint32_t>(20);
	}
	
	//Fix camera after zooming with the sniper
	if (FixSniperZoom)
	{
		auto pattern = hook::pattern("6A 7F 6A 81 6A 7F E8 ? ? ? ? 83 C4 0C A2");
		injector::MakeNOP(pattern.get_first(14), 5, true);
	}
	
	if (RestorePickupTransparency)
	{
		auto pattern = hook::pattern("C7 40 58 FF FF FF FF A1 ? ? ? ? 81 60 58 FF FF FE FF A1 ? ? ? ? 81 60 58 FF FF FF FB A1 ? ? ? ? 81 60 58 FF DF FF FF A1 ? ? ? ? 81 60 58 FF F7 FF FF 8B 45 D8");
		injector::MakeNOP(pattern.get_first(0), 7, true);
		injector::MakeNOP(pattern.get_first(94), 8, true);
	}
	
	//Inventory item flip
	auto pattern = hook::pattern("A9 ? ? ? ? 74 ? 6A 01 8B CE E8 ? ? ? ? BB 02 00 00 00");
	injector::MakeNOP(pattern.get_first(0), 5, true);
	injector::MakeJMP(pattern.get_first(0), FlipInv, true);
	jmpAddrFlip = (uintptr_t)pattern.count(1).get(0).get<uint32_t>(5);
	
	pattern = hook::pattern("80 A0 8A 00 00 00 F0 5D C3 80 88 8A 00 00 00 0F 5D C3");
	injector::MakeNOP(pattern.get_first(0), 7, true);
	injector::MakeJMP(pattern.get_first(0), invItemDown, true);
	jmpAddrItemDown = (uintptr_t)pattern.count(1).get(0).get<uint32_t>(7);
	
	injector::MakeNOP(pattern.get_first(9), 7, true);
	injector::MakeJMP(pattern.get_first(9), invItemUp, true);
	jmpAddrItemUp = (uintptr_t)pattern.count(1).get(0).get<uint32_t>(16);
	
	//Inventory bindings
	while (true)
	{
		if (GetAsyncKeyState(KB_flip_left) & 1 || GetAsyncKeyState(KB_flip_right) & 1)
		{
			if (isItemUp)
			{
				shouldflip = true;
				flipdirection = 0x300000;
			}
		}
	
		if (GetAsyncKeyState(KB_flip_up) & 1 || GetAsyncKeyState(KB_flip_down) & 1)
		{
			if (isItemUp)
			{
				shouldflip = true;
				flipdirection = 0x400000;
			}
		}
		Sleep(5);
	}
	
	return S_OK;
}

void LoadRealDLL(HMODULE hModule)
{
	char configname[MAX_PATH];
	GetModuleFileNameA(hModule, configname, MAX_PATH);
	WrapperName.assign(strrchr(configname, '\\') + 1);

	// Get wrapper mode
	const char* RealWrapperMode = Wrapper::GetWrapperName((WrapperMode.size()) ? WrapperMode.c_str() : WrapperName.c_str());

	proxy_dll = Wrapper::CreateWrapper((RealDllPath.size()) ? RealDllPath.c_str() : nullptr, (WrapperMode.size()) ? WrapperMode.c_str() : nullptr, WrapperName.c_str());
}

// Dll main function
bool APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:

#ifdef VERBOSE
		AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
#endif
		LoadRealDLL(hModule);

		CloseHandle(CreateThread(nullptr, 0, Init, nullptr, 0, nullptr));

		break;
	case DLL_PROCESS_DETACH:
		break;
	}

	return true;
}