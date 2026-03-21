/*
 * (C) 2013-2018 see Authors.txt
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
#include "mplayerc.h"
#include "PathUtils.h"
#include "../filters/InternalPropertyPage.h"
#include "../filters/PinInfoWnd.h"
#include <FileVersionInfo.h>

#include <initguid.h>
#include "FGFilterLAV.h"

#define LAV_FILTERS_VERSION(major, minor, rev, commit) ((QWORD)(major) << 48 | (QWORD)(minor) << 32 | (QWORD)(rev) << 16 | (QWORD)(commit))

#ifndef _WIN64
#define LAVFILTERS_DIR _T("LAVFilters\\")
#else
#define LAVFILTERS_DIR _T("LAVFilters64\\")
#endif

//
// CFGFilterLAV
//

CList<const IBaseFilter*> CFGFilterLAV::s_instances;
QWORD CFGFilterLAV::lav_version = 0;

CFGFilterLAV::CFGFilterLAV(const CLSID& clsid, CString path, CStringW name, bool bAddLowMeritSuffix, UINT64 merit, bool bIsPreview)   
    : isPreview(bIsPreview)
    , CFGFilterFile(clsid, path, name + (bAddLowMeritSuffix ? LowMeritSuffix : L""), merit)
{
}

CString CFGFilterLAV::GetFilterPath(LAVFILTER_TYPE filterType)
{
    // Default path
    CString filterPath = PathUtils::CombinePaths(PathUtils::GetProgramPath(), LAVFILTERS_DIR);
    CLSID filterCLSID;

    switch (filterType) {
        case SPLITTER:
        case SPLITTER_SOURCE:
            filterPath  = PathUtils::CombinePaths(filterPath, CFGFilterLAVSplitterBase::filename);
            filterCLSID = GUID_LAVSplitter;
            break;
        case VIDEO_DECODER:
            filterPath  = PathUtils::CombinePaths(filterPath, CFGFilterLAVVideo::filename);
            filterCLSID = GUID_LAVVideo;
            break;
        case AUDIO_DECODER:
            filterPath  = PathUtils::CombinePaths(filterPath, CFGFilterLAVAudio::filename);
            filterCLSID = GUID_LAVAudio;
            break;
        default:
            ASSERT(FALSE); // This should never happen
            break;
    }

#if ENABLE_LOAD_EXTERNAL_LAVF_AS_INTERNAL
    // Check that the filter's version is correct
    if (!CheckVersion(filterPath)) {
        // If not, check if a registered version of the filter is available.
        filterPath = ::GetFilterPath(filterCLSID);
        // and if it can be used
        if (!CheckVersion(filterPath)) {
            filterPath = _T("");
        }
    }
#endif

    return filterPath;
}

bool CFGFilterLAV::CheckVersion(CString filterPath)
{
    QWORD fversion = FileVersionInfo::GetFileVersionNum(filterPath);
    if (fversion >= 0 && (lav_version == 0 || lav_version > fversion)) {
        lav_version = fversion;
    }

    return fversion >= LAV_FILTERS_VERSION(0, 77, 0, 0);
}

CString CFGFilterLAV::GetVersion(LAVFILTER_TYPE filterType /*= INVALID*/)
{
    CStringList paths;

    if (filterType == INVALID) {
        paths.AddTail(GetFilterPath(SPLITTER));
        paths.AddTail(GetFilterPath(VIDEO_DECODER));
        paths.AddTail(GetFilterPath(AUDIO_DECODER));
    } else {
        paths.AddTail(GetFilterPath(filterType));
    }

    QWORD uiVersionMin = UINT64_MAX;
    QWORD uiVersionMax = 0ui64;
    CString strVersionMin, strVersionMax;
    POSITION pos = paths.GetHeadPosition();
    while (pos) {
        CString& path = paths.GetNext(pos);

        QWORD version = FileVersionInfo::GetFileVersionNum(path);
        if (version) {
            if (version < uiVersionMin) {
                uiVersionMin = version;
                strVersionMin = FileVersionInfo::FormatVersionString(version);
            }
            if (version > uiVersionMax) {
                uiVersionMax = version;
                strVersionMax = FileVersionInfo::FormatVersionString(version);
            }
        }
    }

    CString version;
    if (uiVersionMin != UINT64_MAX) {
        version = strVersionMin;
        if (uiVersionMax != uiVersionMin) {
            version.AppendFormat(_T(" - %s"), strVersionMax.GetString());
        }
    }

    return version;
}

CFGFilterLAV* CFGFilterLAV::CreateFilter(LAVFILTER_TYPE filterType, UINT64 merit /*= MERIT64_DO_USE*/, bool bAddLowMeritSuffix /*= false*/, bool bIsPreview /*= false*/)
{
    CFGFilterLAV* filter = nullptr;

    CString filterPath = GetFilterPath(filterType);

    switch (filterType) {
        case SPLITTER:
            filter = DEBUG_NEW CFGFilterLAVSplitter(filterPath, merit, bAddLowMeritSuffix, bIsPreview);
            break;
        case SPLITTER_SOURCE:
            filter = DEBUG_NEW CFGFilterLAVSplitterSource(filterPath, merit, bAddLowMeritSuffix, bIsPreview);
            break;
        case VIDEO_DECODER:
            filter = DEBUG_NEW CFGFilterLAVVideo(filterPath, merit, bAddLowMeritSuffix, bIsPreview);
            break;
        case AUDIO_DECODER:
            filter = DEBUG_NEW CFGFilterLAVAudio(filterPath, merit, bAddLowMeritSuffix, bIsPreview);
            break;
        default:
            ASSERT(FALSE); // This should never happen
            break;
    }

    return filter;
}

CFGFilterLAV* CFGFilterLAV::CreateFilterPreview(LAVFILTER_TYPE filterType, UINT64 merit /*= MERIT64_DO_USE*/, bool bAddLowMeritSuffix /*= false*/) {
    CFGFilterLAV* filter = CreateFilter(filterType, merit, bAddLowMeritSuffix, true);

    return filter;
}


bool CFGFilterLAV::IsInternalInstance(IBaseFilter* pBF, LAVFILTER_TYPE* pLAVFilterType /*= nullptr*/)
{
    bool bIsInternalInstance = (s_instances.Find(pBF) != nullptr);

    if (bIsInternalInstance && pLAVFilterType) {
        CLSID clsid;
        *pLAVFilterType = INVALID;

        if (SUCCEEDED(pBF->GetClassID(&clsid))) {
            if (clsid == GUID_LAVSplitter) {
                *pLAVFilterType = SPLITTER;
            } else if (clsid == GUID_LAVSplitterSource) {
                *pLAVFilterType = SPLITTER_SOURCE;
            } else if (clsid == GUID_LAVVideo) {
                *pLAVFilterType = VIDEO_DECODER;
            } else if (clsid == GUID_LAVAudio) {
                *pLAVFilterType = AUDIO_DECODER;
            }
        }
    }

    return bIsInternalInstance;
}

HRESULT CFGFilterLAV::PropertyPageCallback(IBaseFilter* pBF)
{
    CheckPointer(pBF, E_POINTER);

    // If called from a non-main thread (e.g. LAV tray icon callback), marshal
    // to the main thread via PostMessage so the property sheet runs there.
    // This avoids a cross-thread SendMessage deadlock in NotifyFloatingWindows.
    CWnd* pMainWnd = AfxGetMyApp()->GetMainWnd();
    if (pMainWnd && ::GetCurrentThreadId() != ::GetWindowThreadProcessId(pMainWnd->m_hWnd, nullptr)) {
        pBF->AddRef();
        pMainWnd->PostMessage(WM_LAV_PROPPAGE_CALLBACK, 0, reinterpret_cast<LPARAM>(pBF));
        return S_OK;
    }

    CComPropertySheet ps(IDS_PROPSHEET_PROPERTIES, pMainWnd);

    // Find out which internal filter we are opening the property page for
    CFGFilterLAV::LAVFILTER_TYPE LAVFilterType = CFGFilterLAV::INVALID;
    if (!CFGFilterLAV::IsInternalInstance(pBF, &LAVFilterType)) {
        return E_UNEXPECTED;
    }

    HRESULT hr = E_FAIL;
    if (CComQIPtr<ISpecifyPropertyPages> pSPP = pBF) {
        ps.AddPages(pSPP, (LAVFilterType != CFGFilterLAV::AUDIO_DECODER) ? 1 : 2);
    }

    CComPtr<IPropertyPage> pPP = DEBUG_NEW CInternalPropertyPageTempl<CPinInfoWnd>(nullptr, &hr);
    ps.AddPage(pPP, pBF);

    if (ps.GetPageCount() > 1) {
        ps.DoModal();

        if (CComQIPtr<ILAVFSettings> pLAVFSettings = pBF) {
            CFGFilterLAVSplitterBase::Settings settings;
            if (settings.GetSettings(pLAVFSettings)) { // Get current settings from LAVSplitter
                settings.SaveSettings(); // Save them to the registry/ini
            }
        } else if (CComQIPtr<ILAVVideoSettings> pLAVVideoSettings = pBF) {
            CFGFilterLAVVideo::Settings settings;
            if (settings.GetSettings(pLAVVideoSettings)) { // Get current settings from LAVVideo
                settings.SaveSettings(); // Save them to the registry/ini
            }
        } else if (CComQIPtr<ILAVAudioSettings> pLAVAudioSettings = pBF) {
            CFGFilterLAVAudio::Settings settings;
            if (settings.GetSettings(pLAVAudioSettings)) { // Get current settings from LAVAudio
                settings.SaveSettings(); // Save them to the registry/ini
            }
        }

        hr = S_OK;
    }

    return hr;
}

//
// CFGFilterLAVSplitterBase
//

const CString CFGFilterLAVSplitterBase::filename = _T("LAVSplitter.ax");

CFGFilterLAVSplitterBase::CFGFilterLAVSplitterBase(CString path, const CLSID& clsid, CStringW name, bool bAddLowMeritSuffix, UINT64 merit, bool bIsPreview)
    : CFGFilterLAV(clsid, path, name, bAddLowMeritSuffix, merit, bIsPreview)
{
}

HRESULT CFGFilterLAVSplitterBase::Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks)
{
    HRESULT hr = __super::Create(ppBF, pUnks);

    if (SUCCEEDED(hr)) {
        if (!CheckVersion(m_path)) {
            hr = E_FAIL;
        } else if (CComQIPtr<ILAVFSettings> pLAVFSettings = *ppBF) {
            // Take control over LAVSplitter's settings, the settings are reseted to default values
            hr = pLAVFSettings->SetRuntimeConfig(TRUE);

            if (SUCCEEDED(hr)) {
                if (isPreview) {
                    pLAVFSettings->SetMaxQueueMemSize(20);
                    pLAVFSettings->SetMaxQueueSize(50);
                    pLAVFSettings->SetSubstreamsEnabled(false);
                    pLAVFSettings->SetSubtitleMode(LAVSubtitleMode_NoSubs);
                } else {
                    Settings settings;
                    if (settings.GetSettings(pLAVFSettings)) { // Get default settings from LAVSplitter
                        settings.LoadSettings(); // Load our current settings from registry/ini
                        settings.SetSettings(pLAVFSettings); // Set our settings in LAVSplitter
                    }
                }

                SetEnabledDisabledFormats(pLAVFSettings);

                // Keep track of LAVFilters instances in runtime mode
                s_instances.AddTail(*ppBF);
            }
        } else {
            hr = E_NOINTERFACE;
        }
    }

    return hr;
}

void CFGFilterLAVSplitterBase::SetEnabledDisabledFormats(CComQIPtr<ILAVFSettings> pLAVFSettings)
{
    // "*" is a special case and means all formats are enabled
    if (m_enabledFormats.IsEmpty() || m_enabledFormats.GetHead() != "*") {
        // We turn off all formats by default to ensure that we won't hijack other filters
        LPSTR* formats;
        UINT nFormats;
        if (SUCCEEDED(pLAVFSettings->GetFormats(&formats, &nFormats))) {
            for (UINT i = 0; i < nFormats; i++) {
                pLAVFSettings->SetFormatEnabled(formats[i], FALSE);
                // Free the memory immediately since we won't need it later
                CoTaskMemFree(formats[i]);
            }
            CoTaskMemFree(formats);
        }
        // We turn on only the formats specified explicitly
        POSITION pos = m_enabledFormats.GetHeadPosition();
        while (pos) {
            const CStringA& format = m_enabledFormats.GetNext(pos);
            pLAVFSettings->SetFormatEnabled(format, TRUE);
        }
    }

    // Explicitly disabled formats
    POSITION pos = m_disabledFormats.GetHeadPosition();
    while (pos) {
        const CStringA& format = m_disabledFormats.GetNext(pos);
        pLAVFSettings->SetFormatEnabled(format, FALSE);
    }
}

void CFGFilterLAVSplitterBase::ShowPropertyPages(CWnd* pParendWnd)
{
    CFGFilterLAV::ShowPropertyPages<SPLITTER, CFGFilterLAVSplitterBase, ILAVFSettings, 1>(pParendWnd);
}

void CFGFilterLAVSplitterBase::Settings::LoadSettings()
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    ASSERT(pApp);

    bTrayIcon = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("TrayIcon"), bTrayIcon);

    prefAudioLangs = pApp->GetProfileString(IDS_R_INTERNAL_LAVSPLITTER, _T("prefAudioLangs"), prefAudioLangs.c_str());
    prefSubLangs = pApp->GetProfileString(IDS_R_INTERNAL_LAVSPLITTER, _T("prefSubLangs"), prefSubLangs.c_str());
    subtitleAdvanced = pApp->GetProfileString(IDS_R_INTERNAL_LAVSPLITTER, _T("subtitleAdvanced"), subtitleAdvanced.c_str());

    subtitleMode = (LAVSubtitleMode)pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("subtitleMode"), subtitleMode);

    bPGSForcedStream = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("PGSForcedStream"), bPGSForcedStream);

    bPGSOnlyForced = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("PGSOnlyForced"), bPGSOnlyForced);

    iVC1Mode = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("vc1TimestampMode"), iVC1Mode);

    bSubstreams = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("substreams"), bSubstreams);

    bMatroskaExternalSegments = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("MatroskaExternalSegments"), bMatroskaExternalSegments);

    bStreamSwitchReselectSubs = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("StreamSwitchReselectSubs"), bStreamSwitchReselectSubs);

    bStreamSwitchRemoveAudio = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("StreamSwitchRemoveAudio"), bStreamSwitchRemoveAudio);

    bPreferHighQualityAudio = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("PreferHighQualityAudio"), bPreferHighQualityAudio);

    bImpairedAudio = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("ImpairedAudio"), bImpairedAudio);

    dwQueueMaxMemSize = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("QueueMaxSize"), dwQueueMaxMemSize);

    dwQueueMaxPackets = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("QueueMaxPackets"), dwQueueMaxPackets);

    dwNetworkAnalysisDuration = pApp->GetProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("NetworkAnalysisDuration"), dwNetworkAnalysisDuration);
}

void CFGFilterLAVSplitterBase::Settings::SaveSettings()
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    ASSERT(pApp);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("TrayIcon"), bTrayIcon);

    pApp->WriteProfileString(IDS_R_INTERNAL_LAVSPLITTER, _T("prefAudioLangs"), prefAudioLangs.c_str());
    pApp->WriteProfileString(IDS_R_INTERNAL_LAVSPLITTER, _T("prefSubLangs"), prefSubLangs.c_str());
    pApp->WriteProfileString(IDS_R_INTERNAL_LAVSPLITTER, _T("subtitleAdvanced"), subtitleAdvanced.c_str());

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("subtitleMode"), subtitleMode);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("PGSForcedStream"), bPGSForcedStream);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("PGSOnlyForced"), bPGSOnlyForced);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("vc1TimestampMode"), iVC1Mode);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("substreams"), bSubstreams);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("MatroskaExternalSegments"), bMatroskaExternalSegments);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("StreamSwitchReselectSubs"), bStreamSwitchReselectSubs);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("StreamSwitchRemoveAudio"), bStreamSwitchRemoveAudio);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("PreferHighQualityAudio"), bPreferHighQualityAudio);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("ImpairedAudio"), bImpairedAudio);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("QueueMaxSize"), dwQueueMaxMemSize);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("QueueMaxPackets"), dwQueueMaxPackets);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVSPLITTER, _T("NetworkAnalysisDuration"), dwNetworkAnalysisDuration);
}

bool CFGFilterLAVSplitterBase::Settings::GetSettings(CComQIPtr<ILAVFSettings> pLAVFSettings)
{
    if (!pLAVFSettings) {
        return false;
    }

    bTrayIcon = pLAVFSettings->GetTrayIcon();

    HRESULT hr;
    LPWSTR lpwstr = nullptr;
    hr = pLAVFSettings->GetPreferredLanguages(&lpwstr);
    if (SUCCEEDED(hr) && lpwstr) {
        prefAudioLangs = lpwstr;
        CoTaskMemFree(lpwstr);
    }
    lpwstr = nullptr;
    hr = pLAVFSettings->GetPreferredSubtitleLanguages(&lpwstr);
    if (SUCCEEDED(hr) && lpwstr) {
        prefSubLangs = lpwstr;
        CoTaskMemFree(lpwstr);
    }
    lpwstr = nullptr;
    hr = pLAVFSettings->GetAdvancedSubtitleConfig(&lpwstr);
    if (SUCCEEDED(hr) && lpwstr) {
        subtitleAdvanced = lpwstr;
        CoTaskMemFree(lpwstr);
    }

    subtitleMode = pLAVFSettings->GetSubtitleMode();

    bPGSForcedStream = pLAVFSettings->GetPGSForcedStream();

    bPGSOnlyForced = pLAVFSettings->GetPGSOnlyForced();

    iVC1Mode = pLAVFSettings->GetVC1TimestampMode();

    bSubstreams = pLAVFSettings->GetSubstreamsEnabled();

    bMatroskaExternalSegments = pLAVFSettings->GetLoadMatroskaExternalSegments();

    bStreamSwitchReselectSubs = pLAVFSettings->GetStreamSwitchReselectSubtitles();

    bStreamSwitchRemoveAudio = pLAVFSettings->GetStreamSwitchRemoveAudio();

    bImpairedAudio = pLAVFSettings->GetUseAudioForHearingVisuallyImpaired();

    bPreferHighQualityAudio = pLAVFSettings->GetPreferHighQualityAudioStreams();

    dwQueueMaxMemSize = pLAVFSettings->GetMaxQueueMemSize();

    dwQueueMaxPackets = pLAVFSettings->GetMaxQueueSize();

    dwNetworkAnalysisDuration = pLAVFSettings->GetNetworkStreamAnalysisDuration();

    return true;
}

bool CFGFilterLAVSplitterBase::Settings::SetSettings(CComQIPtr<ILAVFSettings> pLAVFSettings)
{
    if (!pLAVFSettings) {
        return false;
    }

    pLAVFSettings->SetTrayIcon(bTrayIcon);

    pLAVFSettings->SetPreferredLanguages(prefAudioLangs.c_str());
    pLAVFSettings->SetPreferredSubtitleLanguages(prefSubLangs.c_str());
    pLAVFSettings->SetAdvancedSubtitleConfig(subtitleAdvanced.c_str());

    pLAVFSettings->SetSubtitleMode(subtitleMode);

    pLAVFSettings->SetPGSForcedStream(bPGSForcedStream);

    pLAVFSettings->SetPGSOnlyForced(bPGSOnlyForced);

    pLAVFSettings->SetVC1TimestampMode(iVC1Mode);

    pLAVFSettings->SetSubstreamsEnabled(bSubstreams);

    pLAVFSettings->SetLoadMatroskaExternalSegments(bMatroskaExternalSegments);

    pLAVFSettings->SetStreamSwitchReselectSubtitles(bStreamSwitchReselectSubs);

    pLAVFSettings->SetStreamSwitchRemoveAudio(bStreamSwitchRemoveAudio);

    pLAVFSettings->SetUseAudioForHearingVisuallyImpaired(bImpairedAudio);

    pLAVFSettings->SetPreferHighQualityAudioStreams(bPreferHighQualityAudio);

    pLAVFSettings->SetMaxQueueMemSize(dwQueueMaxMemSize);

    pLAVFSettings->SetMaxQueueSize(dwQueueMaxPackets);

    pLAVFSettings->SetNetworkStreamAnalysisDuration(dwNetworkAnalysisDuration);

    // Custom interface available only in patched build, will be removed after it's upstreamed
    if (CComQIPtr<ILAVFSettingsMPCHCCustom> pLAVFSettingsMPCHCCustom = pLAVFSettings) {
        pLAVFSettingsMPCHCCustom->SetPropertyPageCallback(PropertyPageCallback);
    }

    return true;
}

//
// CFGFilterLAVSplitter
//

CFGFilterLAVSplitter::CFGFilterLAVSplitter(CString path, UINT64 merit /*= MERIT64_DO_USE*/, bool bAddLowMeritSuffix /*= false*/, bool bIsPreview /*= false*/)
    : CFGFilterLAVSplitterBase(path, GUID_LAVSplitter, L"LAV Splitter (internal)", bAddLowMeritSuffix, merit, bIsPreview)
{
}

//
// CFGFilterLAVSplitterSource
//

CFGFilterLAVSplitterSource::CFGFilterLAVSplitterSource(CString path, UINT64 merit /*= MERIT64_DO_USE*/, bool bAddLowMeritSuffix /*= false*/, bool bIsPreview /*= false*/)
    : CFGFilterLAVSplitterBase(path, GUID_LAVSplitterSource, L"LAV Splitter Source (internal)", bAddLowMeritSuffix, merit, bIsPreview)
{
}

//
// CFGFilterLAVVideo
//

const CString CFGFilterLAVVideo::filename = _T("LAVVideo.ax");

CFGFilterLAVVideo::CFGFilterLAVVideo(CString path, UINT64 merit /*= MERIT64_DO_USE*/, bool bAddLowMeritSuffix /*= false*/, bool bIsPreview /*= false*/)
    : CFGFilterLAV(GUID_LAVVideo, path, L"LAV Video Decoder (internal)", bAddLowMeritSuffix, merit, bIsPreview)
{
}

HRESULT CFGFilterLAVVideo::Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks)
{
    HRESULT hr = __super::Create(ppBF, pUnks);

    if (SUCCEEDED(hr)) {
        if (!CheckVersion(m_path)) {
            hr = E_FAIL;
        } else if (CComQIPtr<ILAVVideoSettings> pLAVFSettings = *ppBF) {
            // Take control over LAVVideo's settings, the settings are reseted to default values
            hr = pLAVFSettings->SetRuntimeConfig(TRUE);

            if (SUCCEEDED(hr)) {
                if (isPreview) {
                    pLAVFSettings->SetNumThreads(2);
                    pLAVFSettings->SetPixelFormat(LAVOutPixFmt_P010, false);
                    pLAVFSettings->SetPixelFormat(LAVOutPixFmt_P016, false);
                } else {
                    Settings settings;
                    if (settings.GetSettings(pLAVFSettings)) { // Get default settings from LAVVideo
                        settings.LoadSettings(); // Load our current settings from registry/ini
                        settings.SetSettings(pLAVFSettings); // Set our settings in LAVVideo
                    }
                }
                // Keep track of LAVFilters instances in runtime mode
                s_instances.AddTail(*ppBF);
            }
        } else {
            hr = E_NOINTERFACE;
        }
    }

    return hr;
}

void CFGFilterLAVVideo::ShowPropertyPages(CWnd* pParendWnd)
{
    CFGFilterLAV::ShowPropertyPages<VIDEO_DECODER, CFGFilterLAVVideo, ILAVVideoSettings, 1>(pParendWnd);
}

LPCTSTR CFGFilterLAVVideo::GetUserFriendlyDecoderName(const LPCWSTR decoderName)
{
    static constexpr std::pair<const LPCWSTR, const LPCTSTR> userFriendlyDecoderNames[] = {
        std::make_pair(L"avcodec", _T("FFmpeg")),
        std::make_pair(L"dxva2n", _T("DXVA2 Native")),
        std::make_pair(L"dxva2cb", _T("DXVA2 Copy-back")),
        std::make_pair(L"dxva2cb direct", _T("DXVA2 Copy-back (Direct)")),
        std::make_pair(L"cuvid", _T("NVIDIA CUVID")),
        std::make_pair(L"quicksync", _T("Intel QuickSync")),
        std::make_pair(L"d3d11 cb direct", _T("D3D11 Copy-back (Direct)")),
        std::make_pair(L"d3d11 cb", _T("D3D11 Copy-back")),
        std::make_pair(L"d3d11 native", _T("D3D11 Native")),
        std::make_pair(L"msdk mvc hw", _T("Intel H.264 (MVC 3D)")),
    };

    for (const auto& name : userFriendlyDecoderNames) {
        if (wcscmp(decoderName, name.first) == 0) {
            return name.second;
        }
    }

    return decoderName;
}

static LPCTSTR pixFmtSettingsMap[LAVOutPixFmt_NB] = {
    _T("yv12"), _T("nv12"), _T("yuy2"), _T("uyvy"), _T("ayuv"), _T("p010"), _T("p210"), _T("y410"),
    _T("p016"), _T("p216"), _T("y416"), _T("rgb32"), _T("rgb24"), _T("v210"), _T("v410"), _T("yv16"),
    _T("yv24"), _T("rgb48")
};

# define PCIV_INTEL     0x8086
# define PCIV_AMD       0x1002
# define PCIV_NVIDIA    0x10de
# define PCIV_QUALCOMM  0x4351
# define PCIV_MICROSOFT 0x1414

bool GetGPUDetailsD3D9(DWORD* vendorid, DWORD* deviceid, DWORD* pixelshaderversion) {
    bool result = false;
    try {
        IDirect3D9Ex* pD3D = nullptr;
        HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
        if (hr == D3D_OK && pD3D) {
            UINT count = pD3D->GetAdapterCount();
            if (count > 0) {
                D3DCAPS9 caps;
                hr = pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
                if (hr == D3D_OK) {
                    *pixelshaderversion = caps.PixelShaderVersion & 0xFFFF;
                }
                D3DADAPTER_IDENTIFIER9 aid9;
                hr = pD3D->GetAdapterIdentifier(0, 0, &aid9);
                if (hr == D3D_OK) {
                    *vendorid = aid9.VendorId;
                    *deviceid = aid9.DeviceId;
                    result = true;
                }
            }
            pD3D->Release();
        }
    } catch (...) {
    }
    return result;
}

#if 0
#include <d3d11.h>

bool SupportsD3D11VA()
{
    bool result = false;
    try {
        ID3D11Device* pDevice = nullptr;
        ID3D11DeviceContext* pContext = nullptr;
        D3D_FEATURE_LEVEL fl_available;
        D3D_FEATURE_LEVEL fl_list[] = { D3D_FEATURE_LEVEL_11_1 };
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl_list, _countof(fl_list), D3D11_SDK_VERSION, &pDevice, &fl_available, &pContext);
        if (hr == S_OK) {
            if (fl_available >= D3D_FEATURE_LEVEL_11_1) {
                result = true;
            }
        }
    } catch (...) {
    }
    return result;
}
#endif

// GPUs which have (slow) partial acceleration of HEVC
bool IntelHEVCBlacklist(DWORD deviceid) {
    bool result = false;

    switch (deviceid) {
        case 0x0412: // Haswell
        case 0x0416:
        case 0x041a:
        case 0x041e:
        case 0x0a16:
        case 0x0a1e:
        case 0x0a26:
        case 0x0a2e:
        case 0x0c02:
        case 0x0c06:
        case 0x0c12:
        case 0x0c16:
        case 0x0c22:
        case 0x0c26:
        case 0x0d06:
        case 0x0d16:
        case 0x0d22:
        case 0x0d26:
        case 0x1612: // Broadwell
        case 0x1616:
        case 0x161a:
        case 0x161b:
        case 0x161d:
        case 0x161e:
        case 0x1622:
        case 0x1626:
        case 0x162a:
        case 0x162b:
        case 0x162d:
        case 0x162e:
            result = true;
    }

    return result;
}

void CFGFilterLAVVideo::Settings::LoadSettings()
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    ASSERT(pApp);

    bTrayIcon = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("TrayIcon"), bTrayIcon);

    dwStreamAR = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("StreamAR"), dwStreamAR);

    dwNumThreads = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("NumThreads"), dwNumThreads);

    dwDeintFieldOrder = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("DeintFieldOrder"), dwDeintFieldOrder);

    deintMode = (LAVDeintMode)pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("DeintMode"), deintMode);

    dwRGBRange = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("RGBRange"), dwRGBRange);

    dwSWDeintMode = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("SWDeintMode"), dwSWDeintMode);

    dwSWDeintOutput = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("SWDeintOutput"), dwSWDeintOutput);

    dwDitherMode = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("DitherMode"), dwDitherMode);

    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
        bPixFmts[i] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_OUTPUTFORMAT, pixFmtSettingsMap[i], bPixFmts[i]);
    }

    dwHWAccel = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), -1);
    if (dwHWAccel == DWORD(-1)) {
        // Detect GPU and decide if HWA should be used
        // ToDo: if MPCVR is used as renderer, check registry if it uses D3D11 and then use D3D11 decoding (if supported)
        dwHWAccel = HWAccel_None;
        DWORD vendorid = 0;
        DWORD deviceid = 0;
        DWORD pixelshaderversion = 0;
        if (GetGPUDetailsD3D9(&vendorid, &deviceid, &pixelshaderversion)) {
            if (pixelshaderversion >= 0x300) {
                if (vendorid == PCIV_NVIDIA || vendorid == PCIV_INTEL || vendorid == PCIV_AMD || vendorid == PCIV_QUALCOMM) {
                    dwHWAccel = HWAccel_DXVA2Native;
                    if (vendorid == PCIV_INTEL && IntelHEVCBlacklist(deviceid)) {
                        bHWFormats[HWCodec_HEVC] = false;
                    }
                }
            }
        }
    }

    bHWFormats[HWCodec_H264] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("h264"), bHWFormats[HWCodec_H264]);
    bHWFormats[HWCodec_VC1] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("vc1"), bHWFormats[HWCodec_VC1]);
    bHWFormats[HWCodec_MPEG2] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("mpeg2"), bHWFormats[HWCodec_MPEG2]);
    bHWFormats[HWCodec_MPEG4] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("mpeg4"), bHWFormats[HWCodec_MPEG4]);
    bHWFormats[HWCodec_HEVC] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("hevc"), bHWFormats[HWCodec_HEVC]);
    bHWFormats[HWCodec_VP9] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("vp9"), bHWFormats[HWCodec_VP9]);
    bHWFormats[HWCodec_H264MVC] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("h264mvc"), bHWFormats[HWCodec_H264MVC]);
    bHWFormats[HWCodec_AV1] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("av1"), bHWFormats[HWCodec_AV1]);

    dwHWAccelResFlags = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWResFlags"), dwHWAccelResFlags);
    dwHWDeintMode = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWDeintMode"), dwHWDeintMode);
    dwHWDeintOutput = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWDeintOutput"), dwHWDeintOutput);

    dwHWAccelDeviceDXVA2 = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceDXVA2"), dwHWAccelDeviceDXVA2);
    dwHWAccelDeviceDXVA2Desc = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceDXVA2Desc"), dwHWAccelDeviceDXVA2Desc);
    dwHWAccelDeviceD3D11 = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), dwHWAccelDeviceD3D11);
    dwHWAccelDeviceD3D11Desc = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11Desc"), dwHWAccelDeviceD3D11Desc);
    bHWAccelCUVIDXVA = pApp->GetProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelCUVIDXVA"), bHWAccelCUVIDXVA);
}

void CFGFilterLAVVideo::Settings::SaveSettings()
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    ASSERT(pApp);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("TrayIcon"), bTrayIcon);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("StreamAR"), dwStreamAR);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("NumThreads"), dwNumThreads);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("DeintFieldOrder"), dwDeintFieldOrder);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("DeintMode"), deintMode);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("RGBRange"), dwRGBRange);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("SWDeintMode"), dwSWDeintMode);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("SWDeintOutput"), dwSWDeintOutput);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO, _T("DitherMode"), dwDitherMode);

    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_OUTPUTFORMAT, pixFmtSettingsMap[i], bPixFmts[i]);
    }

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccel"), dwHWAccel);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("h264"), bHWFormats[HWCodec_H264]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("vc1"), bHWFormats[HWCodec_VC1]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("mpeg2"), bHWFormats[HWCodec_MPEG2]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("mpeg4"), bHWFormats[HWCodec_MPEG4]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("hevc"), bHWFormats[HWCodec_HEVC]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("vp9"), bHWFormats[HWCodec_VP9]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("h264mvc"), bHWFormats[HWCodec_H264MVC]);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("av1"), bHWFormats[HWCodec_AV1]);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWResFlags"), dwHWAccelResFlags);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWDeintMode"), dwHWDeintMode);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWDeintOutput"), dwHWDeintOutput);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceDXVA2"), dwHWAccelDeviceDXVA2);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceDXVA2Desc"), dwHWAccelDeviceDXVA2Desc);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11"), dwHWAccelDeviceD3D11);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelDeviceD3D11Desc"), dwHWAccelDeviceD3D11Desc);
    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVVIDEO_HWACCEL, _T("HWAccelCUVIDXVA"), bHWAccelCUVIDXVA);
}

bool CFGFilterLAVVideo::Settings::GetSettings(CComQIPtr<ILAVVideoSettings> pLAVFSettings)
{
    if (!pLAVFSettings) {
        return false;
    }

    bTrayIcon = pLAVFSettings->GetTrayIcon();

    dwStreamAR = pLAVFSettings->GetStreamAR();

    dwNumThreads = pLAVFSettings->GetNumThreads();

    dwDeintFieldOrder = pLAVFSettings->GetDeintFieldOrder();

    deintMode = pLAVFSettings->GetDeinterlacingMode();

    dwRGBRange = pLAVFSettings->GetRGBOutputRange();

    dwSWDeintMode = pLAVFSettings->GetSWDeintMode();

    dwSWDeintOutput = pLAVFSettings->GetSWDeintOutput();

    dwDitherMode = pLAVFSettings->GetDitherMode();

    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
        bPixFmts[i] = pLAVFSettings->GetPixelFormat((LAVOutPixFmts)i);
    }

    dwHWAccel = pLAVFSettings->GetHWAccel();

    for (int i = 0; i < HWCodec_NB; ++i) {
        bHWFormats[i] = pLAVFSettings->GetHWAccelCodec((LAVVideoHWCodec)i);
    }

    dwHWAccelResFlags = pLAVFSettings->GetHWAccelResolutionFlags();
    dwHWDeintMode = pLAVFSettings->GetHWAccelDeintMode();
    dwHWDeintOutput = pLAVFSettings->GetHWAccelDeintOutput();

    dwHWAccelDeviceDXVA2 = pLAVFSettings->GetHWAccelDeviceIndex(HWAccel_DXVA2CopyBack, &dwHWAccelDeviceDXVA2Desc);
    dwHWAccelDeviceD3D11 = pLAVFSettings->GetHWAccelDeviceIndex(HWAccel_D3D11, &dwHWAccelDeviceD3D11Desc);
    bHWAccelCUVIDXVA = pLAVFSettings->GetHWAccelDeintHQ();

    return true;
}

bool CFGFilterLAVVideo::Settings::SetSettings(CComQIPtr<ILAVVideoSettings> pLAVFSettings)
{
    if (!pLAVFSettings) {
        return false;
    }

    pLAVFSettings->SetTrayIcon(bTrayIcon);

    pLAVFSettings->SetStreamAR(dwStreamAR);

    pLAVFSettings->SetNumThreads(dwNumThreads);

    pLAVFSettings->SetDeintFieldOrder((LAVDeintFieldOrder)dwDeintFieldOrder);

    pLAVFSettings->SetDeinterlacingMode(deintMode);

    pLAVFSettings->SetRGBOutputRange(dwRGBRange);

    pLAVFSettings->SetSWDeintMode((LAVSWDeintModes)dwSWDeintMode);

    pLAVFSettings->SetSWDeintOutput((LAVDeintOutput)dwSWDeintOutput);

    pLAVFSettings->SetDitherMode((LAVDitherMode)dwDitherMode);

    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
        pLAVFSettings->SetPixelFormat((LAVOutPixFmts)i, bPixFmts[i]);
    }

    pLAVFSettings->SetHWAccel((LAVHWAccel)dwHWAccel);

    for (int i = 0; i < HWCodec_NB; ++i) {
        pLAVFSettings->SetHWAccelCodec((LAVVideoHWCodec)i, bHWFormats[i]);
    }

    pLAVFSettings->SetHWAccelResolutionFlags(dwHWAccelResFlags);
    pLAVFSettings->SetHWAccelDeintMode((LAVHWDeintModes)dwHWDeintMode);
    pLAVFSettings->SetHWAccelDeintOutput((LAVDeintOutput)dwHWDeintOutput);

    pLAVFSettings->SetHWAccelDeviceIndex(HWAccel_DXVA2CopyBack, dwHWAccelDeviceDXVA2, dwHWAccelDeviceDXVA2Desc);
    pLAVFSettings->SetHWAccelDeviceIndex(HWAccel_D3D11, dwHWAccelDeviceD3D11, dwHWAccelDeviceD3D11Desc);
    pLAVFSettings->SetHWAccelDeintHQ(bHWAccelCUVIDXVA);

    // Force RV1/2 and v210/v410 enabled, the user can control it from our own options
    pLAVFSettings->SetFormatConfiguration(Codec_RV12, TRUE);
    pLAVFSettings->SetFormatConfiguration(Codec_v210, TRUE);
    // Enable Cinepack and QPEG so that they can be used in low-merit mode
    pLAVFSettings->SetFormatConfiguration(Codec_Cinepak, TRUE);
    pLAVFSettings->SetFormatConfiguration(Codec_QPEG, TRUE);

    // Custom interface available only in patched build, will be removed after it's upstreamed
    if (CComQIPtr<ILAVVideoSettingsMPCHCCustom> pLAVFSettingsMPCHCCustom = pLAVFSettings) {
        pLAVFSettingsMPCHCCustom->SetPropertyPageCallback(PropertyPageCallback);
    }

    if (AfxGetAppSettings().iLAVGPUDevice != DWORD_MAX) {
        pLAVFSettings->SetGPUDeviceIndex(AfxGetAppSettings().iLAVGPUDevice);
    }

    return true;
}


//
// CFGFilterLAVAudio
//

const CString CFGFilterLAVAudio::filename = _T("LAVAudio.ax");

CFGFilterLAVAudio::CFGFilterLAVAudio(CString path, UINT64 merit /*= MERIT64_DO_USE*/, bool bAddLowMeritSuffix /*= false*/, bool bIsPreview /*= false*/)
    : CFGFilterLAV(GUID_LAVAudio, path, L"LAV Audio Decoder (internal)", bAddLowMeritSuffix, merit, bIsPreview)
{
}

HRESULT CFGFilterLAVAudio::Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks)
{
    HRESULT hr = __super::Create(ppBF, pUnks);

    if (SUCCEEDED(hr)) {
        if (!CheckVersion(m_path)) {
            hr = E_FAIL;
        } else if (CComQIPtr<ILAVAudioSettings> pLAVFSettings = *ppBF) {
            // Take control over LAVAudio's settings, the settings are reseted to default values
            hr = pLAVFSettings->SetRuntimeConfig(TRUE);

            if (SUCCEEDED(hr)) {
                if (!isPreview) {
                    Settings settings;
                    if (settings.GetSettings(pLAVFSettings)) { // Get default settings from LAVAudio
                        settings.LoadSettings(); // Load our current settings from registry/ini
                        settings.SetSettings(pLAVFSettings); // Set our settings in LAVAudio
                    }
                }
                // Keep track of LAVFilters instances in runtime mode
                s_instances.AddTail(*ppBF);
            }
        } else {
            hr = E_NOINTERFACE;
        }
    }

    return hr;
}

void CFGFilterLAVAudio::ShowPropertyPages(CWnd* pParendWnd)
{
    CFGFilterLAV::ShowPropertyPages<AUDIO_DECODER, CFGFilterLAVAudio, ILAVAudioSettings, 2>(pParendWnd);
}

static LPCTSTR bitstreamingCodecs[Bitstream_NB] = {
    _T("ac3"), _T("eac3"), _T("truehd"), _T("dts"), _T("dtshd")
};

static LPCTSTR sampleFormats[SampleFormat_Bitstream] = {
    _T("s16"), _T("s24"), _T("s32"), _T("u8"), _T("fp32")
};

void CFGFilterLAVAudio::Settings::LoadSettings()
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    ASSERT(pApp);

    bTrayIcon = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("TrayIcon"), bTrayIcon);

    bDRCEnabled = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("DRCEnabled"), bDRCEnabled);

    iDRCLevel = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("DRCLevel"), iDRCLevel);

    bDTSHDFraming = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("DTSHDFraming"), bDTSHDFraming);

    bBitstreamingFallback = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("BitstreamingFallback"), bBitstreamingFallback);

    bAutoAVSync = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("AutoAVSync"), bAutoAVSync);

    bExpandMono = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("ExpandMono"), bExpandMono);

    bExpand61 = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("Expand61"), bExpand61);

    bOutputStandardLayout = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("OutputStandardLayout"), bOutputStandardLayout);

    bOutput51Legacy = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("Output51Legacy"), bOutput51Legacy);

    bMixingEnabled = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("Mixing"), bMixingEnabled);

    dwMixingLayout = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingLayout"), dwMixingLayout);

    dwMixingFlags = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingFlags"), dwMixingFlags);

    dwMixingMode = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingMode"), dwMixingMode);

    dwMixingCenterLevel = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingCenterLevel"), dwMixingCenterLevel);

    dwMixingSurroundLevel = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingSurroundLevel"), dwMixingSurroundLevel);

    dwMixingLFELevel = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingLFELevel"), dwMixingLFELevel);

    bAudioDelayEnabled = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("AudioDelayEnabled"), bAudioDelayEnabled);

    iAudioDelay = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("AudioDelay"), iAudioDelay);

    for (int i = 0; i < Bitstream_NB; ++i) {
        CString key = CString(_T("Bitstreaming_")) + bitstreamingCodecs[i];
        bBitstream[i] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, key, bBitstream[i]);
    }

    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
        CString key = CString(_T("SampleFormat_")) + sampleFormats[i];
        bSampleFormats[i] = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, key, bSampleFormats[i]);
    }

    bSampleConvertDither = pApp->GetProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("SampleConvertDither"), bSampleConvertDither);
}

void CFGFilterLAVAudio::Settings::SaveSettings()
{
    CMPlayerCApp* pApp = AfxGetMyApp();
    ASSERT(pApp);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("TrayIcon"), bTrayIcon);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("DRCEnabled"), bDRCEnabled);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("DRCLevel"), iDRCLevel);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("DTSHDFraming"), bDTSHDFraming);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("BitstreamingFallback"), bBitstreamingFallback);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("AutoAVSync"), bAutoAVSync);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("ExpandMono"), bExpandMono);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("Expand61"), bExpand61);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("OutputStandardLayout"), bOutputStandardLayout);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("Output51Legacy"), bOutput51Legacy);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("Mixing"), bMixingEnabled);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingLayout"), dwMixingLayout);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingFlags"), dwMixingFlags);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingMode"), dwMixingMode);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingCenterLevel"), dwMixingCenterLevel);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingSurroundLevel"), dwMixingSurroundLevel);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("MixingLFELevel"), dwMixingLFELevel);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("AudioDelayEnabled"), bAudioDelayEnabled);

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("AudioDelay"), iAudioDelay);

    for (int i = 0; i < Bitstream_NB; ++i) {
        CString key = CString(_T("Bitstreaming_")) + bitstreamingCodecs[i];
        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, key, bBitstream[i]);
    }

    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
        CString key = CString(_T("SampleFormat_")) + sampleFormats[i];
        pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, key, bSampleFormats[i]);
    }

    pApp->WriteProfileInt(IDS_R_INTERNAL_LAVAUDIO, _T("SampleConvertDither"), bSampleConvertDither);
}

bool CFGFilterLAVAudio::Settings::GetSettings(CComQIPtr<ILAVAudioSettings> pLAVFSettings)
{
    if (!pLAVFSettings) {
        return false;
    }

    bTrayIcon = pLAVFSettings->GetTrayIcon();

    pLAVFSettings->GetDRC(&bDRCEnabled, &iDRCLevel);

    bDTSHDFraming = pLAVFSettings->GetDTSHDFraming();

    bBitstreamingFallback = pLAVFSettings->GetBitstreamingFallback();

    bAutoAVSync = pLAVFSettings->GetAutoAVSync();

    bExpandMono = pLAVFSettings->GetExpandMono();

    bExpand61 = pLAVFSettings->GetExpand61();

    bOutputStandardLayout = pLAVFSettings->GetOutputStandardLayout();

    bOutput51Legacy = pLAVFSettings->GetOutput51LegacyLayout();

    bMixingEnabled = pLAVFSettings->GetMixingEnabled();

    dwMixingLayout = pLAVFSettings->GetMixingLayout();

    dwMixingFlags = pLAVFSettings->GetMixingFlags();

    dwMixingMode = pLAVFSettings->GetMixingMode();

    pLAVFSettings->GetMixingLevels(&dwMixingCenterLevel, &dwMixingSurroundLevel, &dwMixingLFELevel);

    pLAVFSettings->GetAudioDelay(&bAudioDelayEnabled, &iAudioDelay);

    for (int i = 0; i < Bitstream_NB; ++i) {
        bBitstream[i] = pLAVFSettings->GetBitstreamConfig((LAVBitstreamCodec)i);
    }

    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
        bSampleFormats[i] = pLAVFSettings->GetSampleFormat((LAVAudioSampleFormat)i);
    }

    bSampleConvertDither = pLAVFSettings->GetSampleConvertDithering();

    return true;
}

bool CFGFilterLAVAudio::Settings::SetSettings(CComQIPtr<ILAVAudioSettings> pLAVFSettings)
{
    if (!pLAVFSettings) {
        return false;
    }

    pLAVFSettings->SetTrayIcon(bTrayIcon);

    pLAVFSettings->SetDRC(bDRCEnabled, iDRCLevel);

    pLAVFSettings->SetDTSHDFraming(bDTSHDFraming);

    pLAVFSettings->SetBitstreamingFallback(bBitstreamingFallback);
 
    pLAVFSettings->SetAutoAVSync(bAutoAVSync);

    pLAVFSettings->SetExpandMono(bExpandMono);

    pLAVFSettings->SetExpand61(bExpand61);

    pLAVFSettings->SetOutputStandardLayout(bOutputStandardLayout);

    pLAVFSettings->SetOutput51LegacyLayout(bOutput51Legacy);

    pLAVFSettings->SetMixingEnabled(bMixingEnabled);

    pLAVFSettings->SetMixingLayout(dwMixingLayout);

    pLAVFSettings->SetMixingFlags(dwMixingFlags);

    pLAVFSettings->SetMixingMode((LAVAudioMixingMode)dwMixingMode);

    pLAVFSettings->SetMixingLevels(dwMixingCenterLevel, dwMixingSurroundLevel, dwMixingLFELevel);

    pLAVFSettings->SetAudioDelay(bAudioDelayEnabled, iAudioDelay);

    for (int i = 0; i < Bitstream_NB; ++i) {
        pLAVFSettings->SetBitstreamConfig((LAVBitstreamCodec)i, bBitstream[i]);
    }

    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
        pLAVFSettings->SetSampleFormat((LAVAudioSampleFormat)i, bSampleFormats[i]);
    }

    pLAVFSettings->SetSampleConvertDithering(bSampleConvertDither);

    // The internal LAV Audio Decoder will not be registered to handle WMA formats
    // since the system decoder is preferred. However we can still enable those
    // formats internally so that they are used in low-merit mode.
    pLAVFSettings->SetFormatConfiguration(Codec_WMA2, TRUE);
    pLAVFSettings->SetFormatConfiguration(Codec_WMAPRO, TRUE);
    pLAVFSettings->SetFormatConfiguration(Codec_WMALL, TRUE);

    // Custom interface available only in patched build, will be removed after it's upstreamed
    if (CComQIPtr<ILAVAudioSettingsMPCHCCustom> pLAVFSettingsMPCHCCustom = pLAVFSettings) {
        pLAVFSettingsMPCHCCustom->SetPropertyPageCallback(PropertyPageCallback);
    }

    return true;
}
