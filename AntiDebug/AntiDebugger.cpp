//By AlSch092 @github
#include "AntiDebugger.hpp"

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

void Debugger::AntiDebug::StartAntiDebugThread()
{
	this->DetectionThread = new Thread();

	this->DetectionThread->handle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Debugger::AntiDebug::CheckForDebugger, (LPVOID)this, 0, &this->DetectionThread->Id);

	if (this->DetectionThread->handle == INVALID_HANDLE_VALUE || this->DetectionThread->handle == NULL)
	{
		Logger::logf("UltimateAnticheat.log", Err, "Couldn't start anti-debug thread @ Debugger::AntiDebug::StartAntiDebugThread\n");
		//optionally shut down here
	}

	Logger::logf("UltimateAnticheat.log", Info, "Created Debugger detection thread with Id: %d\n", this->DetectionThread->Id);

	this->DetectionThread->CurrentlyRunning = true;
}

void Debugger::AntiDebug::CheckForDebugger(LPVOID AD)
{
	if (AD == nullptr)
	{
		Logger::logf("UltimateAnticheat.log", Err, "AntiDbg PTR was NULL @ CheckForDebugger");
		return;
	}

	Logger::logf("UltimateAnticheat.log", Info, "STARTED Debugger detection thread");

	bool MonitoringDebugger = true;

	while (MonitoringDebugger)
	{
		Debugger::AntiDebug* AntiDbg = reinterpret_cast<Debugger::AntiDebug*>(AD);

		if (AntiDbg == NULL)
		{
			Logger::logf("UltimateAnticheat.log", Err, "AntiDbg PTR was NULL @ CheckForDebugger");
			return;
		}

		if (AntiDbg->DetectionThread->ShutdownSignalled)
		{
			Logger::logf("UltimateAnticheat.log", Info, "Shutting down Debugger detection thread with Id: %d", AntiDbg->DetectionThread->Id);
			AntiDbg->DetectionThread->CurrentlyRunning = false;
			return; //exit thread
		}

		//Basic winAPI check
		bool basicDbg = AntiDbg->_IsDebuggerPresent();

		if (basicDbg)
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::WINAPI_DEBUGGER);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: WINAPI_DEBUGGER!");
		}

		if (AntiDbg->_IsDebuggerPresent_PEB())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::PEB_FLAG);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: PEB_FLAG!");
		}

		if (AntiDbg->_IsHardwareDebuggerPresent())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::HARDWARE_REGISTERS);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: HARDWARE_REGISTERS.");
		}

		if (AntiDbg->_IsDebuggerPresent_HeapFlags())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::HEAP_FLAG);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: HEAP_FLAG.");
		}

		if (AntiDbg->_IsKernelDebuggerPresent())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::KERNEL_DEBUGGER);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: KERNEL_DEBUGGER.");
		}

		if (AntiDbg->_IsDebuggerPresent_DbgBreak())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::INT3);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: DbgBreak Excpetion Handler");
		}

		if (AntiDbg->_IsDebuggerPresent_WaitDebugEvent())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::DEBUG_EVENT);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: WaitDebugEvent.");
		}

		if (AntiDbg->_IsDebuggerPresent_VEH()) //also patches over InitializeVEH's first byte if the dll is found
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::VEH_DEBUGGER);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: VEH ");
		}

		if (AntiDbg->_IsDebuggerPresent_DebugPort())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::DEBUG_PORT);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: DebugPort");
		}

		if (AntiDbg->_IsDebuggerPresent_ProcessDebugFlags())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::PROCESS_DEBUG_FLAGS);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: ProcessDebugFlags\n");
		}

		if (AntiDbg->_IsDebuggerPresent_CloseHandle())
		{
			AntiDbg->DebuggerMethodsDetected.push_back(Detections::CLOSEHANDLE);
			Logger::logf("UltimateAnticheat.log", Detection, "Found debugger: CloseHandle");
		}
		
		if (AntiDbg->DebuggerMethodsDetected.size() > 0)
		{
			Logger::logf("UltimateAnticheat.log", Info, "Atleast one method has caught a running debugger!\n");
		}	

		Sleep(2000);
	}
}

bool Debugger::AntiDebug::_IsHardwareDebuggerPresent()
{
	DWORD ProcessId = GetCurrentProcessId(); //this can be replaced later, or a process that is not us

	HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;

	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
		return(FALSE);

	te32.dwSize = sizeof(THREADENTRY32);

	if (!Thread32First(hThreadSnap, &te32))
	{
		CloseHandle(hThreadSnap);
		return(FALSE);
	}
	
	do
	{
		if (te32.th32OwnerProcessID == ProcessId)
		{
			CONTEXT lpContext;
			memset(&lpContext, 0, sizeof(CONTEXT));
			lpContext.ContextFlags = CONTEXT_FULL;

			HANDLE _tThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te32.th32ThreadID);

			if (_tThread)
			{
				if (GetThreadContext(_tThread, &lpContext))
				{
					if (lpContext.Dr0 || lpContext.Dr1 || lpContext.Dr2 || lpContext.Dr3 )
					{
						Logger::logf("UltimateAnticheat.log", Detection, "Found at least one debug register enabled\n");
						CloseHandle(hThreadSnap);
						CloseHandle(_tThread);
						return true;
					}
				}
				else
				{
					Logger::logf("UltimateAnticheat.log", Err, "GetThreadContext failed with: %d\n", GetLastError());
					CloseHandle(_tThread);
					continue;
				}
			}
			else
			{
				Logger::logf("UltimateAnticheat.log", Err, "Could not call openthread! %d\n", GetLastError());
				continue;
			}
		}
	} while (Thread32Next(hThreadSnap, &te32));

	CloseHandle(hThreadSnap);
	return false;
}

bool Debugger::AntiDebug::_IsKernelDebuggerPresent()
{
	typedef long NTSTATUS;
	HANDLE hProcess = GetCurrentProcess();

	typedef struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION { bool DebuggerEnabled; bool DebuggerNotPresent; } SYSTEM_KERNEL_DEBUGGER_INFORMATION, * PSYSTEM_KERNEL_DEBUGGER_INFORMATION;

	enum SYSTEM_INFORMATION_CLASS { SystemKernelDebuggerInformation = 35 };
	typedef NTSTATUS(__stdcall* ZW_QUERY_SYSTEM_INFORMATION)(IN SYSTEM_INFORMATION_CLASS SystemInformationClass, IN OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength);
	ZW_QUERY_SYSTEM_INFORMATION ZwQuerySystemInformation;
	SYSTEM_KERNEL_DEBUGGER_INFORMATION Info;

	HMODULE hModule = GetModuleHandleA("ntdll.dll");

	if (hModule == NULL)
	{
		Logger::logf("UltimateAnticheat.log", Err, "Error fetching module ntdll.dll @ _IsKernelDebuggerPresent: %d\n", GetLastError());
		return false;
	}

	ZwQuerySystemInformation = (ZW_QUERY_SYSTEM_INFORMATION)GetProcAddress(hModule, "ZwQuerySystemInformation");
	if (ZwQuerySystemInformation == NULL)
		return false;

	if (!ZwQuerySystemInformation(SystemKernelDebuggerInformation, &Info, sizeof(Info), NULL)) 
	{
		if (Info.DebuggerEnabled && !Info.DebuggerNotPresent)
			return true; 
		else
			return false;
	}

	return false;
}

bool Debugger::AntiDebug::_IsKernelDebuggerPresent_SharedKData()
{
	_KUSER_SHARED_DATA* sharedData = USER_SHARED_DATA;

	if (sharedData->DbgKdEnabled) 	//Check the kernel debugger enabled flag
	{
		return true;
	}

	return false;
}

bool Debugger::AntiDebug::_IsDebuggerPresent_HeapFlags()
{
	DWORD_PTR pPeb64 = (DWORD_PTR)__readgsqword(0x60);

	if (pPeb64)
	{
		PVOID ptrHeap = (PVOID)*(PDWORD_PTR)((PBYTE)pPeb64 + 0x30);
		PDWORD heapForceFlagsPtr = (PDWORD)((PBYTE)ptrHeap + 0x74);

		__try
		{
			if (*heapForceFlagsPtr >= 0x40000060)
				return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	return false;
}

bool Debugger::AntiDebug::_IsDebuggerPresent_CloseHandle()
{
#ifndef _DEBUG
	__try
	{
		CloseHandle((HANDLE)1);
	}
	__except (EXCEPTION_INVALID_HANDLE == GetExceptionCode() ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return true;
	}
#endif
	return false;
}

bool Debugger::AntiDebug::_IsDebuggerPresent_RemoteDebugger()
{
	BOOL bDebugged = false;
	if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &bDebugged))
		if (bDebugged)
			return true;

	return false;
}

bool Debugger::AntiDebug::_IsDebuggerPresent_Int2c()
{
	__try
	{
		__int2c();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return true;
	}

	return false;
}

bool Debugger::AntiDebug::_IsDebuggerPresent_Int2d()
{
	unsigned char cBuf[2] = { 0x2D, 0xC3 };

	DWORD pOldProt = 0;
	if (!VirtualProtect((LPVOID)cBuf, 2, PAGE_EXECUTE_READ, &pOldProt))
	{
		Logger::logf("UltimateAnticheat.log", Err, "VirtualProtect failed at _IsDebuggerPresent_Int2d: %d\n", GetLastError());
		return false;
	}

	__try
	{
		void (*fun_ptr)() = (void(*)(void))(&cBuf);
		fun_ptr();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}

bool Debugger::AntiDebug::_IsDebuggerPresent_DbgBreak()
{
#ifdef _DEBUG
	return false;  //only use __fastfail in release build , since it will trip up our execution when debugging this project
#else
	__try
	{
		DebugBreak();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	Logger::logf("UltimateAnticheat.log", Info, "Calling __fastfail() to prevent further execution since a debugger was found running.\n");
	__fastfail(1); //code should not reach here unless process is being debugged
	return true;
#endif
}

/*
	_IsDebuggerPresent_VEH - Checks if vehdebug-x86_64.dll is loaded and exporting InitiallizeVEH. If so, the first byte of this routine is patched and the module's internal name is changed to STOP_CHEATING
	returns true if CE's VEH debugger is found, but this won't stop home-rolled VEH debuggers via APC injection
*/
inline bool Debugger::AntiDebug::_IsDebuggerPresent_VEH()
{
	bool bFound = false;

	HMODULE veh_debugger = GetModuleHandleA("vehdebug-x86_64.dll");

	if (veh_debugger != NULL) 
	{
		UINT64 veh_addr = (UINT64)GetProcAddress(veh_debugger, "InitializeVEH"); //check for named exports of cheat engine's VEH debugger
		
		if (veh_addr > 0)
		{
			bFound = true;

			DWORD dwOldProt = 0;

			if (!VirtualProtect((void*)veh_addr, 1, PAGE_EXECUTE_READWRITE, &dwOldProt))
			{
				Logger::logf("UltimateAnticheat.log", Warning, "VirtualProtect failed @ _IsDebuggerPresent_VEH");
			}

			memcpy((void*)veh_addr, "\xC3", sizeof(BYTE)); //patch first byte of `InitializeVEH` with a ret, stops call to InitializeVEH from succeeding.

			if (!VirtualProtect((void*)veh_addr, 1, dwOldProt, &dwOldProt)) //change back to old prot's
			{
				Logger::logf("UltimateAnticheat.log", Warning, "VirtualProtect failed @ _IsDebuggerPresent_VEH");
			}

			if (Process::ChangeModuleName(L"vehdebug-x86_64.dll", L"STOP_CHEATING"))
			{
				Logger::logf("UltimateAnticheat.log", Info, "Changed module name of vehdebug-x86_64.dll to STOP_CHEATING to prevent VEH debugging.");
			}
		}
	}

	return bFound;
}

inline bool Debugger::AntiDebug::_IsDebuggerPresent_WaitDebugEvent()
{
	bool bFound = false;
	LPDEBUG_EVENT lpd_e = { 0 };

	if (WaitForDebugEvent(lpd_e, INFINITE))
	{
		bFound = true;
	}

	return bFound;
}

inline bool  Debugger::AntiDebug::_IsDebuggerPresent_PEB()
{
	MYPEB* _PEB = (MYPEB*)__readgsqword(0x60);
	return _PEB->BeingDebugged;
}

inline bool Debugger::AntiDebug::_IsDebuggerPresent_DebugPort()
{
	typedef NTSTATUS(NTAPI* TNtQueryInformationProcess)(IN HANDLE ProcessHandle, IN PROCESS_INFORMATION_CLASS ProcessInformationClass,OUT PVOID ProcessInformation,IN ULONG ProcessInformationLength,OUT PULONG ReturnLength);

	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");

	if(!hNtdll)
	    hNtdll = LoadLibraryA("ntdll.dll");
	
	if (hNtdll)
	{
		auto pfnNtQueryInformationProcess = (TNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");

		if (pfnNtQueryInformationProcess)
		{
			const PROCESS_INFORMATION_CLASS ProcessDebugPort = (PROCESS_INFORMATION_CLASS)7;
			DWORD dwProcessDebugPort, dwReturned;
			NTSTATUS status = pfnNtQueryInformationProcess(GetCurrentProcess(), ProcessDebugPort, &dwProcessDebugPort, sizeof(DWORD), &dwReturned);

			if (NT_SUCCESS(status) && (dwProcessDebugPort == -1))
				return true;
		}
	}

	return false;
}


inline bool Debugger::AntiDebug::_IsDebuggerPresent_ProcessDebugFlags()
{
	typedef NTSTATUS(NTAPI* TNtQueryInformationProcess)(IN HANDLE ProcessHandle, IN PROCESS_INFORMATION_CLASS ProcessInformationClass, OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength, OUT PULONG ReturnLength);

	HMODULE hNtdll = LoadLibraryA("ntdll.dll");
	if (hNtdll)
	{
		auto pfnNtQueryInformationProcess = (TNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");

		if (pfnNtQueryInformationProcess)
		{
			PROCESS_INFORMATION_CLASS pic = (PROCESS_INFORMATION_CLASS)0x1F;
			DWORD dwProcessDebugFlags, dwReturned;
			NTSTATUS status = pfnNtQueryInformationProcess(GetCurrentProcess(), pic, &dwProcessDebugFlags, sizeof(DWORD), &dwReturned);

			if (NT_SUCCESS(status) && (dwProcessDebugFlags == 0))
				return true;
		}
	}

	return false;
}
