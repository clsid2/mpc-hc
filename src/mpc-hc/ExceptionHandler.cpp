/*
 * (C) 2017 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "ExceptionHandler.h"
#include <windows.h>
#include <psapi.h>
#include <inttypes.h>
#include "mplayerc.h"

#ifndef _DEBUG

typedef BOOL(WINAPI* tpSymInitialize)(
	_In_ HANDLE hProcess,
	_In_opt_ PCSTR UserSearchPath,
	_In_ BOOL fInvadeProcess
	);
typedef BOOL(__stdcall* tpSymCleanup)(
	_In_ HANDLE hProcess
	);
typedef BOOL(__stdcall* tpStackWalk64)(
	_In_ DWORD MachineType,
	_In_ HANDLE hProcess,
	_In_ HANDLE hThread,
	_Inout_ LPSTACKFRAME64 StackFrame,
	_Inout_ PVOID ContextRecord,
	_In_opt_ PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	_In_opt_ PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	_In_opt_ PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	_In_opt_ PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
	);
typedef PVOID(__stdcall* tpSymFunctionTableAccess64)(
	_In_ HANDLE hProcess,
	_In_ DWORD64 AddrBase
	);
typedef DWORD64(__stdcall* tpSymGetModuleBase64)(
	_In_ HANDLE hProcess,
	_In_ DWORD64 qwAddr
	);
typedef BOOL(__stdcall* tpSymFromAddrW)(
	_In_ HANDLE hProcess,
	_In_ DWORD64 Address,
	_Out_opt_ PDWORD64 Displacement,
	_Inout_ PSYMBOL_INFOW Symbol
	);
typedef BOOL(__stdcall* tpSymGetLineFromAddrW64)(
	_In_ HANDLE hProcess,
	_In_ DWORD64 qwAddr,
	_Out_ PDWORD pdwDisplacement,
	_Out_ PIMAGEHLP_LINEW64 Line
	);

CString GetStackTrace(LPEXCEPTION_POINTERS exp)
{
    CString trace = L"";

    HMODULE hDbhHelp = ::LoadLibraryW(L"dbghelp.dll");
	if (!hDbhHelp) {
		return trace;
	}

	auto pSymInitialize = (tpSymInitialize)GetProcAddress(hDbhHelp, "SymInitialize");
	auto pSymCleanup = (tpSymCleanup)GetProcAddress(hDbhHelp, "SymCleanup");
	auto pStackWalk64 = (tpStackWalk64)GetProcAddress(hDbhHelp, "StackWalk64");
	auto pSymFunctionTableAccess64 = (tpSymFunctionTableAccess64)GetProcAddress(hDbhHelp, "SymFunctionTableAccess64");
	auto pSymGetModuleBase64 = (tpSymGetModuleBase64)GetProcAddress(hDbhHelp, "SymGetModuleBase64");
	auto pSymFromAddrW = (tpSymFromAddrW)GetProcAddress(hDbhHelp, "SymFromAddrW");
	auto pSymGetLineFromAddrW64 = (tpSymGetLineFromAddrW64)GetProcAddress(hDbhHelp, "SymGetLineFromAddrW64");

	if (!pSymInitialize || !pSymCleanup || !pStackWalk64 || !pSymFunctionTableAccess64 || !pSymGetModuleBase64 || !pSymFromAddrW || !pSymGetLineFromAddrW64) {
		return trace;
	}

	HANDLE process = GetCurrentProcess();

	// load symbols
	if (!pSymInitialize(process, nullptr, TRUE)) {
		return trace;
	}

	auto ctx = exp->ContextRecord;
	char symbolBuffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)];
	auto pSymbol = (PSYMBOL_INFOW)symbolBuffer;

	HANDLE thread = GetCurrentThread();

	STACKFRAME64 frame = {};
	DWORD imageType = 0;

#ifdef _M_X64
	imageType = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = ctx->Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = ctx->Rsp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = ctx->Rsp;
	frame.AddrStack.Mode = AddrModeFlat;
#else
	imageType = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = ctx->Eip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = ctx->Ebp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = ctx->Esp;
	frame.AddrStack.Mode = AddrModeFlat;
#endif

    int line_count = 0;
    CString last_line = L"";
    CString current_line = L"";
    while (pStackWalk64(imageType, process, thread, &frame, ctx, nullptr, pSymFunctionTableAccess64, pSymGetModuleBase64, nullptr)) {
        // get symbol name for address
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
        pSymbol->MaxNameLen = MAX_SYM_NAME * sizeof(wchar_t);
        DWORD64 displacement = 0;
        pSymFromAddrW(process, frame.AddrPC.Offset, &displacement, pSymbol);

        // try to get line
        current_line = L"";
        IMAGEHLP_LINEW64 line = { sizeof(IMAGEHLP_LINEW64) };
        DWORD disp = 0;
        if (pSymGetLineFromAddrW64(process, frame.AddrPC.Offset, &disp, &line)) {
            current_line.Format(L"%s(%lu) : %s()\n", line.FileName, line.LineNumber, pSymbol->Name);
        } else {
            HMODULE hModule = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)(frame.AddrPC.Offset), &hModule);
            if (hModule) {
                wchar_t mfn[MAX_PATH] = {};
                if (GetModuleFileNameW(hModule, mfn, MAX_PATH)) {
                    CLongPath file = mfn;
                    file.StripPath();
                    CString moduleName = file.m_strPath;
                    moduleName.MakeLower();
                    if (frame.AddrPC.Offset > pSymbol->Address && (frame.AddrPC.Offset - pSymbol->Address < 0x100000)) {
                        current_line.Format(L"%s : %s() + 0x%" PRIXPTR "\n", moduleName.GetString(), pSymbol->Name, frame.AddrPC.Offset - pSymbol->Address);
                    } else {
                        current_line = file.m_strPath + L"\n";
                    }
                }
            }
        }
        if (current_line != last_line) {
            trace.Append(current_line);
            last_line = current_line;
            if (++line_count >= 10) {
                break;
            }
        } if (current_line.IsEmpty()) {
            break;
        }
    }

	pSymCleanup(process);

    return trace;
}


LPCWSTR GetExceptionName(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            return _T("ACCESS VIOLATION");
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return _T("ARRAY BOUNDS EXCEEDED");
        case EXCEPTION_BREAKPOINT:
            return _T("BREAKPOINT");
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            return _T("DATATYPE MISALIGNMENT");
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            return _T("FLT DENORMAL OPERAND");
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return _T("FLT DIVIDE BY ZERO");
        case EXCEPTION_FLT_INEXACT_RESULT:
            return _T("FLT INEXACT RESULT");
        case EXCEPTION_FLT_INVALID_OPERATION:
            return _T("FLT INVALID OPERATION");
        case EXCEPTION_FLT_OVERFLOW:
            return _T("FLT OVERFLOW");
        case EXCEPTION_FLT_STACK_CHECK:
            return _T("FLT STACK CHECK");
        case EXCEPTION_FLT_UNDERFLOW:
            return _T("FLT UNDERFLOW");
        case EXCEPTION_GUARD_PAGE:
            return _T("GUARD PAGE");
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return _T("ILLEGAL_INSTRUCTION");
        case EXCEPTION_IN_PAGE_ERROR:
            return _T("IN PAGE ERROR");
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return _T("INT DIVIDE BY ZERO");
        case EXCEPTION_INT_OVERFLOW:
            return _T("INT OVERFLOW");
        case EXCEPTION_INVALID_DISPOSITION:
            return _T("INVALID DISPOSITION");
        case EXCEPTION_INVALID_HANDLE:
            return _T("INVALID HANDLE");
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            return _T("NONCONTINUABLE EXCEPTION");
        case EXCEPTION_PRIV_INSTRUCTION:
            return _T("PRIV INSTRUCTION");
        case EXCEPTION_SINGLE_STEP:
            return _T("SINGLE STEP");
        case EXCEPTION_STACK_OVERFLOW:
            return _T("STACK OVERFLOW");
        case 0xC06D007E:
            return _T("0xC06D007E - DEPENDENCY MISSING");
        case 0xE06D7363:
            return _T("UNDEFINED C++ EXCEPTION");
    }

    CString res;
    res.Format(L"0x%08X", code);
    return res.GetString();
}

LONG WINAPI UnhandledException(LPEXCEPTION_POINTERS exceptionInfo)
{
#if 1
    if (AfxGetMyApp()->m_fClosingState) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif

    uintptr_t moduleBase = 0;
    uintptr_t offset = 0;
    HMODULE hModule = nullptr;
    CString moduleName;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCWSTR>(exceptionInfo->ExceptionRecord->ExceptionAddress), &hModule);
    if (hModule) {
        wchar_t mfn[MAX_PATH] = {};
        if (GetModuleFileNameW(hModule, mfn, MAX_PATH)) {
            moduleName = mfn;
            moduleName.MakeLower();
        } else {
            moduleName = L"[UNKNOWN]";
        }
        moduleBase = reinterpret_cast<uintptr_t>(hModule);
    } else {
        moduleName = L"[UNKNOWN]";
        moduleBase = uintptr_t(GetModuleHandle(nullptr));        
    }
    offset = uintptr_t(exceptionInfo->ExceptionRecord->ExceptionAddress) - moduleBase;

    CString errmsg = L"An error has occurred. MPC-HC will close now.\n\n";
    errmsg.AppendFormat(L"Exception:\n%s\n\nCrashing module:\n%s\nOffset: 0x%" PRIXPTR, GetExceptionName(exceptionInfo->ExceptionRecord->ExceptionCode), moduleName.GetString(), offset);

    if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        const wchar_t* accessType;
        switch (exceptionInfo->ExceptionRecord->ExceptionInformation[0]) {
            case 0:
                accessType = _T("read");
                break;
            case 1:
                accessType = _T("write");
                break;
            case 2:
                accessType = _T("execute");
                break;
            default:
                accessType = _T("[UNKNOWN]");
                break;
        }
        errmsg.AppendFormat(L"\nThe thread %lu tried to %s memory at address 0x%" PRIXPTR, GetCurrentThreadId(), accessType, exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
    }

    CString trace = GetStackTrace(exceptionInfo);
    if (!trace.IsEmpty()) {
        errmsg.Append(L"\n\nStack trace:\n");
        errmsg.Append(trace);
    }

    bool use_wer = false; // Send to Windows Error Reporting
    CString comment = _T("");
    if (moduleName.Find(_T("nvd3dumx.dll")) >= 0 || moduleName.Find(_T("nvwgf2umx.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in the NVIDIA graphics driver. If this happens often, then you should install a different version of their driver.");
        use_wer = true;
    } else if (moduleName.Find(_T("atiumd")) >= 0 || moduleName.Find(_T("aticfx")) >= 0 || moduleName.Find(_T("atidxx")) >= 0 || moduleName.Find(_T("atiu9")) >= 0) {
        comment = _T("This crash was caused by a fault in the AMD graphics driver. If this happens often, then you should install a different version of their driver.");
        use_wer = true;
    } else if (moduleName.Find(_T("igdumd")) >= 0 || moduleName.Find(_T("igd9dxva")) >= 0 || moduleName.Find(_T("igc64.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in the Intel graphics driver. If this happens often, then you should install a different version of their driver.");
        use_wer = true;
    } else if (moduleName.Find(_T("-lav-")) >= 0 || moduleName.Find(_T("lavvideo")) >= 0 || moduleName.Find(_T("lavaudio")) >= 0 || moduleName.Find(_T("lavsplitter")) >= 0) {
        comment = _T("This crash was caused by a fault in LAV Filters. Please report this crash to us. Then we can investigate why this crash happens and try to fix it.");
    } else if (moduleName.Find(_T("madvr")) >= 0 || moduleName.Find(_T("mvrsettings64.dll")) >= 0 || moduleName.Find(_T("madhcnet64.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in madVR.");
    } else if (moduleName.Find(_T("atklumdispx.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in ASUS GamerOSD.");
    } else if (moduleName.Find(_T("gtii-osd64.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in Asus GPU Tweak II.");
    } else if (moduleName.Find(_T("splitter.x64.ax")) >= 0) {
        comment = _T("This crash was caused by a fault in Haali Media Splitter.");
    } else if (moduleName.Find(_T("dxr.x64.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in Haali Video Renderer.");
    } else if (moduleName.Find(_T("vboxdispd3d.dll")) >= 0) {
        comment = _T("This crash was caused by a fault in VirtualBox display driver.");
    } else if (moduleName.Find(_T("explorerframe")) >= 0) {
        comment = _T("This crash was caused by a fault in a Windows Explorer DLL.\nDo this to avoid the problem:\nMPC-HC options > Player > User Interface > uncheck \"Use enhanced taskbar features\"");
        use_wer = true;
    } else if (moduleName.Find(_T("kernelbase.dll")) >= 0 && trace.Find(_T("vapoursynth_filter")) >= 0) {
        comment = _T("This crash is likely caused by missing VapourSynth runtime or missing C++ runtime installation.\nYou can also fix it by removing this external filter in the options.");
    } else if (moduleName.Find(_T("kernelbase.dll")) >= 0 && trace.Find(_T("avisynth_filter")) >= 0) {
        comment = _T("This crash is likely caused by missing AviSynth runtime or missing C++ runtime installation.\nYou can also fix it by removing this external filter in the options.");
    } else if (moduleName.Find(_T("loilocap")) >= 0) {
        comment = _T("This crash is caused by a fault in LoiLo Game Recorder.");
    } else if (moduleName.Find(_T("evr.dll")) >= 0) {
        comment = _T("This crash is caused by a fault in EVR. If this happens often, choose a different video renderer in the options.");
        use_wer = true;
    } else if (moduleName.Find(_T("d3d9on12")) >= 0) {
        use_wer = true;
    } else if (moduleName.Find(_T("dsound")) >= 0 || moduleName.Find(_T("audiokse")) >= 0 || moduleName.Find(_T("audioses")) >= 0) {
        use_wer = true;
    }

    if (!comment.IsEmpty()) {
        errmsg.Append(L"\n");
        errmsg.Append(comment);
    }

    MessageBox(NULL, errmsg, _T("Unexpected error"), MB_OK | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL);

    return use_wer ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER;
}
#endif

void MPCExceptionHandler::Enable()
{
#ifndef _DEBUG
    SetUnhandledExceptionFilter(UnhandledException);
#endif
};

void MPCExceptionHandler::Disable()
{
#ifndef _DEBUG
    SetUnhandledExceptionFilter(nullptr);
#endif
};
