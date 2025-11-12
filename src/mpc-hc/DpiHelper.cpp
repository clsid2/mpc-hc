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
#include "DpiHelper.h"
#include "WinapiFunc.h"
#include <VersionHelpersInternal.h>

//used for GetTextScaleFactor
#include <wrl.h>
#include <Windows.UI.ViewManagement.h>
#include <inspectable.h>
//end GetTextScaleFactor

namespace
{
    typedef enum MONITOR_DPI_TYPE {
        MDT_EFFECTIVE_DPI = 0,
        MDT_ANGULAR_DPI = 1,
        MDT_RAW_DPI = 2,
        MDT_DEFAULT = MDT_EFFECTIVE_DPI
    } MONITOR_DPI_TYPE;

    typedef int (WINAPI* tpGetSystemMetricsForDpi)(int nIndex, UINT dpi);
    HRESULT WINAPI GetDpiForMonitor(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT* dpiX, UINT* dpiY);
    BOOL WINAPI SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi);
    int WINAPI GetSystemMetricsForDpi(int nIndex);
    UINT WINAPI GetDpiForWindow(HWND hwnd);
    double WINAPI TextScaleFactor(void);
}

DpiHelper::DpiHelper()
{
    HDC hDC = ::GetDC(nullptr);
    m_sdpix = GetDeviceCaps(hDC, LOGPIXELSX);
    m_sdpiy = GetDeviceCaps(hDC, LOGPIXELSY);
    ::ReleaseDC(nullptr, hDC);
    m_dpix = m_sdpix;
    m_dpiy = m_sdpiy;
}

UINT DpiHelper::GetDPIForWindow(HWND wnd) {
    const WinapiFunc<decltype(GetDpiForWindow)>
        fnGetDpiForWindow = { _T("user32.dll"), "GetDpiForWindow" };
    if (fnGetDpiForWindow) {
        return fnGetDpiForWindow(wnd);
    }
    return 0;
}

UINT DpiHelper::GetDPIForMonitor(HMONITOR hMonitor) {
    const WinapiFunc<decltype(GetDpiForMonitor)>
        fnGetDpiForMonitor = { _T("Shcore.dll"), "GetDpiForMonitor" };

    if (hMonitor && fnGetDpiForMonitor) {
        UINT tdpix, tdpiy;
        if (fnGetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &tdpix, &tdpiy) == S_OK) {
            return tdpix;
        }
    }
    return 0;
}

void DpiHelper::Override(HWND hWindow)
{
    const WinapiFunc<decltype(GetDpiForMonitor)>
        fnGetDpiForMonitor = { _T("Shcore.dll"), "GetDpiForMonitor" };

    if (hWindow && fnGetDpiForMonitor) {
        if (fnGetDpiForMonitor(MonitorFromWindow(hWindow, MONITOR_DEFAULTTONULL),
                               MDT_EFFECTIVE_DPI, (UINT*)&m_dpix, (UINT*)&m_dpiy) != S_OK) {
            m_dpix = m_sdpix;
            m_dpiy = m_sdpiy;
        }
    }
}

void DpiHelper::Override(int dpix, int dpiy)
{
    m_dpix = dpix;
    m_dpiy = dpiy;
}

int DpiHelper::GetSystemMetricsDPI(int nIndex) {
    if (IsWindows10OrGreater()) {
        static tpGetSystemMetricsForDpi pGetSystemMetricsForDpi = (tpGetSystemMetricsForDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi");
        if (pGetSystemMetricsForDpi) {
            return pGetSystemMetricsForDpi(nIndex, m_dpix);
        }
    }

    return ScaleSystemToOverrideX(::GetSystemMetrics(nIndex));
}

void DpiHelper::GetMessageFont(LOGFONT* lf) {
    NONCLIENTMETRICS ncm;
    bool dpiCorrected = false;
    GetNonClientMetrics(&ncm, dpiCorrected);
    *lf = ncm.lfMessageFont;
    ASSERT(lf->lfHeight);
    if (!dpiCorrected) {
        lf->lfHeight = ScaleSystemToOverrideY(lf->lfHeight);
    }
}

bool DpiHelper::GetNonClientMetrics(PNONCLIENTMETRICSW ncm, bool& dpiCorrected) {
    const WinapiFunc<decltype(SystemParametersInfoForDpi)>
        fnSystemParametersInfoForDpi = { L"user32.dll", "SystemParametersInfoForDpi" };

    ZeroMemory(ncm, sizeof(NONCLIENTMETRICS));
    ncm->cbSize = sizeof(NONCLIENTMETRICS);
    dpiCorrected = false;

    if (fnSystemParametersInfoForDpi) {
        if (fnSystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(*ncm), ncm, 0, m_dpiy)) {
            dpiCorrected = true;
            return true;
        }
    }
    if (!dpiCorrected) {
        return SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm->cbSize, ncm, 0);
    }
    return false; //never gets here
}

int DpiHelper::GetSystemMetrics(int type) {
    const WinapiFunc<decltype(GetSystemMetricsForDpi)>
        fnGetSystemMetricsForDpi = { L"user32.dll", "GetSystemMetricsForDpi" };

    bool dpiCorrected = false;

    if (fnGetSystemMetricsForDpi) {
        dpiCorrected = true;
        return fnGetSystemMetricsForDpi(type);
    }
    if (!dpiCorrected) {
        int ret = fnGetSystemMetricsForDpi(type);
        return ScaleSystemToOverrideY(ret);
    }
}

bool DpiHelper::CanUsePerMonitorV2() {
    RTL_OSVERSIONINFOW osvi = GetRealOSVersion();
    bool ret = (osvi.dwMajorVersion >= 10 && osvi.dwMajorVersion >= 0 && osvi.dwBuildNumber >= 15063); //PerMonitorV2 with common control scaling first available in win 10 1703
    return ret;
}

int DpiHelper::CalculateListCtrlItemHeight(CListCtrl* wnd) {
    INT nItemHeight;

    DWORD type = wnd->GetStyle() & LVS_TYPEMASK;
    if (type == LVS_ICON || type == LVS_SMALLICON) {
        int h, v;
        wnd->GetItemSpacing(type == LVS_SMALLICON, &h, &v);
        nItemHeight = v;
    } else {
        TEXTMETRICW tm;
        CDC* cdc = wnd->GetDC();
        cdc->SelectObject(wnd->GetFont());
        cdc->GetTextMetricsW(&tm);

        nItemHeight = tm.tmHeight + 4;
        CImageList* ilist;
        if ((ilist = wnd->GetImageList(LVSIL_STATE)) != 0) {
            int cx, cy;
            ImageList_GetIconSize(ilist->m_hImageList, &cx, &cy);
            nItemHeight = std::max(nItemHeight, ScaleY(cy + 1));
        }
        if ((ilist = wnd->GetImageList(LVSIL_SMALL)) != 0) {
            int cx, cy;
            ImageList_GetIconSize(ilist->m_hImageList, &cx, &cy);
            nItemHeight = std::max(nItemHeight, ScaleY(cy + 1));
        }
    }

    return std::max(nItemHeight, 1);
}

//reduced interface from windows.ui.viewmanagement.h in windows 10 sdk
MIDL_INTERFACE("BAD82401-2721-44F9-BB91-2BB228BE442F")
IUISettings2: public IInspectable
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_TextScaleFactor(__RPC__out DOUBLE * value) = 0;
};

//Windows 7 can return exception due to ::WindowsCreateStringReference not found
double DoGetTextScaleFactor() {
    double value = 1.0;

    using namespace Microsoft::WRL;
    using namespace Microsoft::WRL::Wrappers;

    ComPtr<IInspectable> instance;
    HRESULT hr = RoActivateInstance(HStringReference(RuntimeClass_Windows_UI_ViewManagement_UISettings).Get(), &instance);
    if (FAILED(hr)) return value;

    ComPtr<IUISettings2> settings2;
    hr = instance.As(&settings2);
    if (FAILED(hr)) return value;

    hr = settings2->get_TextScaleFactor(&value);
    if (FAILED(hr)) return value;

    return value;
}

double DpiHelper::GetTextScaleFactor() {
    if (!IsWindows10OrGreater()) {
        return 1.0;
    }
    __try {
        return DoGetTextScaleFactor();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1.0;
    }
}
