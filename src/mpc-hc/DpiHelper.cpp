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
#include <map>

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

UINT DpiHelper::GetDPIForOS() {
    HDC hDC = ::GetDC(nullptr);
    if (hDC) {
        UINT dpiX = GetDeviceCaps(hDC, LOGPIXELSX);
        ::ReleaseDC(nullptr, hDC);

        if (dpiX) {
            return dpiX;
        }
    }

    //default to 96, only option on older OS anyway
    return 96;
}

UINT DpiHelper::GetDPIForWindow(HWND wnd) {
    // Use GetDPIForWindow to get the window's logical DPI, not the monitor's physical DPI
    // During DPI transitions, a window can be on a 168 DPI monitor but have 96 DPI logical DPI
    // note: GetDpiForMonitor available since 8.1, GetDPIForWindow since 10 1607

    //try number 1
    static const WinapiFunc<UINT WINAPI(HWND)>
        fnGetDpiForWindow = { L"user32.dll", "GetDpiForWindow" };
    if (fnGetDpiForWindow) {
        return fnGetDpiForWindow(wnd);
    }

    //try number 2
    static const WinapiFunc<HRESULT WINAPI(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*)>
        fnGetDpiForMonitor = { L"Shcore.dll", "GetDpiForMonitor" };
    if (fnGetDpiForMonitor) {
        UINT dpiX, dpiY;
        if (fnGetDpiForMonitor(MonitorFromWindow(wnd, MONITOR_DEFAULTTONULL), MDT_EFFECTIVE_DPI, &dpiX, &dpiY) == S_OK) {
            return dpiX;
        }
    }

    //try 3, should never fail
    {
        return GetDPIForOS();
    }

}

UINT DpiHelper::GetDPIForMonitor(HMONITOR hMonitor) {
    static const WinapiFunc<HRESULT WINAPI(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*)>
        fnGetDpiForMonitor = { L"Shcore.dll", "GetDpiForMonitor" };

    if (hMonitor && fnGetDpiForMonitor) {
        UINT tdpix, tdpiy;
        if (fnGetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &tdpix, &tdpiy) == S_OK) {
            return tdpix;
        }
    }
    return GetDPIForOS();
}

UINT DpiHelper::GetDPIForRect(const RECT* pRect) {
    if (!pRect) {
        return 0;
    }
    HMONITOR hMonitor = MonitorFromRect(pRect, MONITOR_DEFAULTTONEAREST);
    return GetDPIForMonitor(hMonitor);
}

void DpiHelper::Override(HWND hWindow)
{
    if (hWindow) {
        UINT windowDpi = GetDPIForWindow(hWindow);
        if (windowDpi != 0) {
            m_dpix = windowDpi;
            m_dpiy = windowDpi;
        }
    }
}

void DpiHelper::Override(int dpix, int dpiy)
{
    m_dpix = dpix;
    m_dpiy = dpiy;
}

int DpiHelper::GetSystemMetricsDPI(int nIndex) {
    static const WinapiFunc<int WINAPI(int, UINT)>
        fnGetSystemMetricsForDpi = { L"user32.dll", "GetSystemMetricsForDpi" };
    if (fnGetSystemMetricsForDpi) {
        return fnGetSystemMetricsForDpi(nIndex, m_dpix);
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
    static const WinapiFunc<BOOL WINAPI(UINT, UINT, PVOID, UINT, UINT)>
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
        if (!cdc) {
            ASSERT(false);
            return 1;
        }
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

// Cache key for dialog font metrics
struct DialogFontKey {
    UINT dpi;
    CString fontFace;
    int fontSize;

    bool operator<(const DialogFontKey& other) const {
        if (dpi != other.dpi) return dpi < other.dpi;
        int cmp = fontFace.Compare(other.fontFace);
        if (cmp != 0) return cmp < 0;
        return fontSize < other.fontSize;
    }
};

// Static cache for dialog font metrics
static std::map<DialogFontKey, std::pair<int, int>> s_dialogFontMetricsCache;

bool DpiHelper::GetDialogFontMetricsForDPI(UINT dpi, LPCTSTR fontFace, int fontSize, int& avgWidth, int& avgHeight) {
    // Create cache key
    DialogFontKey key;
    key.dpi = dpi;
    key.fontFace = fontFace;
    key.fontSize = fontSize;

    // Check cache first
    auto it = s_dialogFontMetricsCache.find(key);
    if (it != s_dialogFontMetricsCache.end()) {
        avgWidth = it->second.first;
        avgHeight = it->second.second;
        return true;
    }

    // Not in cache, create font at specified DPI and measure it
    HDC hdc = ::GetDC(NULL);
    if (!hdc) {
        return false;
    }

    // Calculate font height for the specified DPI
    // Font size is in points (1/72 inch), convert to pixels at this DPI
    int fontHeight = -MulDiv(fontSize, dpi, 72);

    LOGFONT lf = {};
    lf.lfHeight = fontHeight;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, fontFace);

    HFONT hFont = CreateFontIndirect(&lf);
    if (!hFont) {
        ::ReleaseDC(NULL, hdc);
        return false;
    }

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);

    SIZE size;
    GetTextExtentPoint32(hdc, _T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"), 52, &size);
    avgWidth = (size.cx / 26 + 1) / 2; // Average width of upper and lower case
    avgHeight = tm.tmHeight;

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ::ReleaseDC(NULL, hdc);

    // Cache the result
    s_dialogFontMetricsCache[key] = std::make_pair(avgWidth, avgHeight);

    return true;
}

void DpiHelper::ClearDialogFontMetricsCache() {
    s_dialogFontMetricsCache.clear();
}

BOOL DpiHelper::AdjustWindowRectExForDpi(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi) {
    // Use DPI-aware window rect adjustment if available (Windows 10 1607+)
    static const WinapiFunc<BOOL WINAPI(LPRECT, DWORD, BOOL, DWORD, UINT)>
        fnAdjustWindowRectExForDpi = { L"user32.dll", "AdjustWindowRectExForDpi" };

    if (fnAdjustWindowRectExForDpi) {
        return fnAdjustWindowRectExForDpi(lpRect, dwStyle, bMenu, dwExStyle, dpi);
    }
    // Fallback for older Windows versions
    return ::AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
}
