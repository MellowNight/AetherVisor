#include "aethervisor_test.h"
#include "global.h"
#include "portable_executable.h"

#define ENTRYPOINT_METHOD  2 // 1 == NPT hook on present | 2 == SetWindowsHookEx

enum INJECTOR_CONSTANTS
{
	mapped_dll_header = 0x12345678,
};

size_t dll_file_size = 0;

__declspec(dllexport) DllParams* Global::dll_params = 0;

bool entry_not_called = true;

extern "C" __declspec(dllexport) HRESULT __fastcall HookEntryPoint(IDXGISwapChain * pChain, UINT SyncInterval, UINT Flags)
{
	if (entry_not_called)
	{
		entry_not_called = false;

		if (ENTRYPOINT_METHOD == 1)
		{		
			/*	restore entrypoint hook		*/

			auto dxgi = (uintptr_t)GetModuleHandle(L"dxgi.dll");
			auto dxgi = (uintptr_t)GetModuleHandle(L"dxgi.dll");

			auto present_address = Utils::FindPattern(dxgi, PeHeader(dxgi)->OptionalHeader.SizeOfImage,
				"\x48\x89\x74\x24\x00\x55\x57\x41\x56\x48\x8D\x6C\x24\x00\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\x60", 35, 0x00
			) - 5;

			AetherVisor::NptHook::Remove(present_address);
		}

		PE::ResolveImports((uint8_t*)Global::dll_params->dll_base);

		StartTests();
	}

	return 0;
}

extern "C" __declspec(dllexport) void __fastcall CreateUserThreadEntry()
{
	PE::ResolveImports((uint8_t*)Global::dll_params->dll_base);

	StartTests();
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		Global::dll_params = new DllParams;
		Global::dll_params->dll_size = PeHeader(hModule)->OptionalHeader.SizeOfImage;
		Global::dll_params->dll_base = (uintptr_t)hModule;

		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)StartTests, NULL, NULL, NULL);

		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}