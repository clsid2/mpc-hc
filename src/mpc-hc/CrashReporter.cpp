/*
 * (C) 2015-2016 see Authors.txt
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
#include "CrashReporter.h"
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
#include "CrashEventLog.h"
#include "mplayerc.h"
#include "VersionInfo.h"
#include "mpc-hc_config.h"
#include "DoctorDump/CrashRpt.h"


using namespace crash_rpt;

namespace CrashReporter
{
    static bool g_bEnabled = false;
    static CrashRpt g_crashReporter(L"CrashReporter\\crashrpt.dll");

    static CrashProcessingCallbackResult CALLBACK CrashProcessingCallback(CrashProcessingCallbackStage stage,
            ExceptionInfo* pExceptionInfo,
            LPVOID pUserData);
}

namespace
{
    SRWLOCK g_eventLock = SRWLOCK_INIT;
    CrashEventLog g_eventLog = { "MPC-HC events" };
    const ULONGLONG g_startTick = GetTickCount64();

    const wchar_t* EventName(uint16_t code)
    {
        static const wchar_t* const names[] = {
            L"Start", L"Load", L"Graph", L"Play", L"Pause", L"Stop", L"FS", L"D3DFS",
            L"Reset", L"CloseStep", L"OnClose", L"OnDestroy",
        };
        return code < _countof(names) ? names[code] : L"?";
    }
}
#endif

void CrashReporter::Enable()
{
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    crash_rpt::ApplicationInfo appInfo = {
        sizeof(appInfo),
        "80dcded5-e256-46c1-ae51-fe7384cae2be",
        "mpc-hc",
        L"MPC-HC",
        L"MPC-HC Team",
        {
            USHORT(VersionInfo::GetMajorNumber()),
            USHORT(VersionInfo::GetMinorNumber()),
            USHORT(VersionInfo::GetPatchNumber()),
            USHORT(VersionInfo::GetRevisionNumber()),
        },
        0,
        nullptr
    };

    const MINIDUMP_TYPE dumpType = MINIDUMP_TYPE(
#if ENABLE_FULLDUMP
                                       MiniDumpWithFullMemory |
                                       MiniDumpWithTokenInformation |
#else
                                       MiniDumpWithIndirectlyReferencedMemory |
                                       MiniDumpWithDataSegs |
#endif // ENABLE_FULLDUMP
                                       MiniDumpWithHandleData |
                                       MiniDumpWithThreadInfo |
                                       MiniDumpWithProcessThreadData |
                                       MiniDumpWithFullMemoryInfo |
                                       MiniDumpWithUnloadedModules |
                                       MiniDumpIgnoreInaccessibleMemory
                                   );

    crash_rpt::HandlerSettings handlerSettings = {
        sizeof(handlerSettings),
        FALSE,      // If dumps should be kept in temp dir
        TRUE,       // Open the problem page in the browser
        FALSE,      // If Windows Error Reporting should be used
        0,          // Anonymous submitter
        TRUE,       // Additional info
        TRUE,       // Override the "full" dump settings
        dumpType,   // "Full" dump custom settings
        nullptr,    // No lang file (for now)
        nullptr,    // Default path for SendRpt
        nullptr,    // Default path for DbgHelp
        CrashProcessingCallback,    // Callback function
        nullptr,    // No user defined parameter for the callback function
        nullptr,    // Custom data collection settings
        nullptr
    };

    g_bEnabled = g_crashReporter.InitCrashRpt(&appInfo, &handlerSettings);
    LogEvent(CrashEvent::Start);
#endif
}

void CrashReporter::Disable()
{
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    g_bEnabled = false;
#endif
}

bool CrashReporter::IsEnabled()
{
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    return g_bEnabled;
#else
    return false;
#endif
}

void CrashReporter::SetInfo(LPCWSTR key, LPCWSTR value)
{
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    if (!g_bEnabled) {
        return;
    }
    if (value && *value) {
        g_crashReporter.AddUserInfoToReport(key, value);
    } else {
        g_crashReporter.RemoveUserInfoFromReport(key);
    }
#else
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(value);
#endif
}

void CrashReporter::LogEvent(CrashEvent e, int arg /*= 0*/)
{
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    AcquireSRWLockExclusive(&g_eventLock);
    g_eventLog.Push(uint32_t(GetTickCount64() - g_startTick), uint16_t(e), arg);
    if (g_bEnabled) {
        // updated on every event, so nothing has to run after a crash
        SetInfo(L"Events", g_eventLog.Format(EventName).c_str());
    }
    ReleaseSRWLockExclusive(&g_eventLock);
#else
    UNREFERENCED_PARAMETER(e);
    UNREFERENCED_PARAMETER(arg);
#endif
}

void CrashReporter::UpdatePlaybackInfo(int videoRenderer, int mpcvrRenderType, int lavHWAccel, LPCWSTR activeDecoder)
{
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
    if (!g_bEnabled) {
        return;
    }
    const CAppSettings& s = AfxGetAppSettings();
    SetInfo(L"GPU", s.gpuid1 + L" " + s.gpudriver1);

    CStringW v;
    v.Format(L"%d", videoRenderer);
    if (videoRenderer == VIDRNDT_DS_MPCVR) {
        DWORD useD3D11 = 0;
        ReadRegistryDWORD(HKEY_CURRENT_USER, L"Software\\MPC-BE Filters\\MPC Video Renderer", L"UseD3D11", useD3D11);
        // renderType: 9 or 11, 0 before the input pin connects
        v.AppendFormat(L" UseD3D11=%lu renderType=%d", useD3D11, mpcvrRenderType);
    }
    SetInfo(L"Renderer", v);

    v.Empty();
    if (lavHWAccel >= 0) {
        v.Format(L"HWAccel=%d active=%s", lavHWAccel, activeDecoder ? activeDecoder : L"none");
    }
    SetInfo(L"LAVVideo", v);
#else
    UNREFERENCED_PARAMETER(videoRenderer);
    UNREFERENCED_PARAMETER(mpcvrRenderType);
    UNREFERENCED_PARAMETER(lavHWAccel);
    UNREFERENCED_PARAMETER(activeDecoder);
#endif
}

#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
CrashProcessingCallbackResult CALLBACK CrashReporter::CrashProcessingCallback(CrashProcessingCallbackStage stage,
        ExceptionInfo* pExceptionInfo,
        LPVOID pUserData)
{
    return g_bEnabled ? DoDefaultActions : SkipSendReportReturnDefaultResult;
}
#endif
