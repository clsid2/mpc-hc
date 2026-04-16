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
        case 0xE06D7363:
            return _T("UNDEFINED C++ EXCEPTION");
        default:
            return _T("[UNKNOWN]");
    }
}

HMODULE GetExceptionModule(LPVOID address, LPWSTR moduleName)
{
    HMODULE moduleList[1024];
    DWORD sizeNeeded = 0;
    if (!EnumProcessModules(GetCurrentProcess(), moduleList, sizeof(moduleList), &sizeNeeded) || sizeNeeded > sizeof(moduleList)) {
        return nullptr;
    }

    int curModule = -1;
    for (DWORD i = 0; i < (sizeNeeded / sizeof(HMODULE)); ++i) {
        if (moduleList[i] < address) {
            if (curModule == -1) {
                curModule = i;
            } else {
                if (moduleList[curModule] < moduleList[i]) {
                    curModule = i;
                }
            }
        }
    }

    if (curModule == -1) {
        return nullptr;
    }

    if (!GetModuleFileName(moduleList[curModule], moduleName, MAX_PATH)) {
        return nullptr;
    }

    return moduleList[curModule];
}

void HandleCommonException(LPEXCEPTION_POINTERS exceptionInfo)
{
    wchar_t message[MAX_PATH + 255];
    wchar_t module[MAX_PATH];
    const wchar_t* moduleName = GetExceptionModule(exceptionInfo->ExceptionRecord->ExceptionAddress, module) ? module : _T("[UNKNOWN]");

    const uintptr_t codeBase = uintptr_t(GetModuleHandle(nullptr));
    const uintptr_t offset   = uintptr_t(exceptionInfo->ExceptionRecord->ExceptionAddress) - codeBase;

    swprintf_s(message, _countof(message), _T(\
                                              "An error has occurred. MPC-HC will close now.\n\n"\
                                              "Exception:\n%s\n\n"\
                                              "Crashing module:\n%s\n"\
                                              "Offset: 0x%" PRIXPTR ", Codebase: 0x%" PRIXPTR "\n\n"),
               GetExceptionName(exceptionInfo->ExceptionRecord->ExceptionCode),
               moduleName,
               offset,
               codeBase);

    MessageBox(NULL, message, _T("Unexpected error"), MB_OK | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL);
}

void HandleAccessViolation(LPEXCEPTION_POINTERS exceptionInfo)
{
    wchar_t message[MAX_PATH + 255];
    wchar_t module[MAX_PATH];
    const wchar_t* moduleName = GetExceptionModule(exceptionInfo->ExceptionRecord->ExceptionAddress, module) ? module : _T("[UNKNOWN]");

    const uintptr_t codeBase = uintptr_t(GetModuleHandle(nullptr));
    const uintptr_t offset   = uintptr_t(exceptionInfo->ExceptionRecord->ExceptionAddress) - codeBase;

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

    swprintf_s(message, _countof(message), _T(\
                                              "An error has occurred. MPC-HC will close now.\n\n"\
                                              "Exception:\n%s\n\n"\
                                              "Crashing module:\n%s\n"\
                                              "Offset: 0x%" PRIXPTR ", Codebase: 0x%" PRIXPTR "\n"\
                                              "The thread %lu tried to %s memory at address 0x%" PRIXPTR "\n\n"),
               GetExceptionName(exceptionInfo->ExceptionRecord->ExceptionCode),
               moduleName,
               offset,
               codeBase,
               GetCurrentThreadId(),
               accessType,
               exceptionInfo->ExceptionRecord->ExceptionInformation[1]);

    MessageBox(NULL, message, _T("Unexpected error"), MB_OK | MB_TOPMOST | MB_SETFOREGROUND | MB_SYSTEMMODAL);
}

LONG WINAPI UnhandledException(LPEXCEPTION_POINTERS exceptionInfo)
{
#if 1
    if (AfxGetMyApp()->m_fClosingState) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif

    switch (exceptionInfo->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            HandleAccessViolation(exceptionInfo);
            break;
        default:
            HandleCommonException(exceptionInfo);
            break;
    }

    return EXCEPTION_EXECUTE_HANDLER;
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
