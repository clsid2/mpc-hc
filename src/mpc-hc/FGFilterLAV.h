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

#pragma once

#include <string>

DEFINE_GUID(GUID_LAVSplitter, 0x171252A0, 0x8820, 0x4AFE, 0x9D, 0xF8, 0x5C, 0x92, 0xB2, 0xD6, 0x6B, 0x04);
DEFINE_GUID(GUID_LAVSplitterSource, 0xB98D13E7, 0x55DB, 0x4385, 0xA3, 0x3D, 0x09, 0xFD, 0x1B, 0xA2, 0x63, 0x38);
DEFINE_GUID(GUID_LAVVideo, 0xEE30215D, 0x164F, 0x4A92, 0xA4, 0xEB, 0x9D, 0x4C, 0x13, 0x39, 0x0F, 0x9F);
DEFINE_GUID(GUID_LAVAudio, 0xE8E73B6B, 0x4CB3, 0x44A4, 0xBE, 0x99, 0x4F, 0x7B, 0xCB, 0x96, 0xE4, 0x91);

#include "FGFilter.h"
#include "ComPropertySheet.h"

#include "LAVFilters/src/include/LAVSplitterSettings.h"
#include "LAVFilters/src/include/LAVVideoSettings.h"
#include "LAVFilters/src/include/LAVAudioSettings.h"

#define IDS_R_INTERNAL_LAVSPLITTER           IDS_R_INTERNAL_FILTERS  _T("\\LAVSplitter")
#define IDS_R_INTERNAL_LAVVIDEO              IDS_R_INTERNAL_FILTERS  _T("\\LAVVideo")
#define IDS_R_INTERNAL_LAVVIDEO_OUTPUTFORMAT IDS_R_INTERNAL_LAVVIDEO _T("\\OutputFormat")
#define IDS_R_INTERNAL_LAVVIDEO_HWACCEL      IDS_R_INTERNAL_LAVVIDEO _T("\\HWAccel")
#define IDS_R_INTERNAL_LAVAUDIO              IDS_R_INTERNAL_FILTERS  _T("\\LAVAudio")

class CFGFilterLAV : public CFGFilterFile
{
protected:
    static CList<const IBaseFilter*> s_instances;
    static QWORD lav_version;
    bool isPreview;

    CFGFilterLAV(const CLSID& clsid, CString path, CStringW name, bool bAddLowMeritSuffix, UINT64 merit, bool bIsPreview);

public:
    enum LAVFILTER_TYPE {
        INVALID = -1,
        SPLITTER,
        SPLITTER_SOURCE,
        VIDEO_DECODER,
        AUDIO_DECODER
    };

    static CString GetFilterPath(LAVFILTER_TYPE filterType);
    static bool CheckVersion(CString filtersPath);
    static CString GetVersion(LAVFILTER_TYPE filterType = INVALID);

    static CFGFilterLAV* CreateFilter(LAVFILTER_TYPE filterType, UINT64 merit = MERIT64_DO_USE, bool bAddLowMeritSuffix = false, bool bIsPreview = false);
    static CFGFilterLAV* CreateFilterPreview(LAVFILTER_TYPE filterType, UINT64 merit = MERIT64_DO_USE, bool bAddLowMeritSuffix = false);

    static bool IsInternalInstance(IBaseFilter* pBF, LAVFILTER_TYPE* pLAVFilterType = nullptr);
    static void ResetInternalInstances() {
        s_instances.RemoveAll();
    }

    template<LAVFILTER_TYPE filterType, typename filterClass, typename filterInterface, int iIgnoredPage>
    static void ShowPropertyPages(CWnd* pParendWnd) {
        CAutoPtr<CFGFilterLAV> pLAVFilter(CFGFilterLAV::CreateFilter(filterType));

        if (pLAVFilter) {
            CComPtr<IBaseFilter> pBF;
            CInterfaceList<IUnknown, &IID_IUnknown> pUnks;

            if (SUCCEEDED(pLAVFilter->Create(&pBF, pUnks))) {
                if (CComQIPtr<ISpecifyPropertyPages> pSPP = pBF) {
                    CComPropertySheet ps(IDS_PROPSHEET_PROPERTIES, pParendWnd);
                    ps.AddPages(pSPP, true, iIgnoredPage);
                    ps.DoModal();

                    if (CComQIPtr<filterInterface> pLAVFSettings = pBF) {
                        filterClass::Settings settings;
                        if (settings.GetSettings(pLAVFSettings)) { // Get current settings
                            settings.SaveSettings(); // Save them to the registry/ini
                        }
                    }
                }
            }
        }
    }

    static HRESULT PropertyPageCallback(IBaseFilter* pFilter);
};

class CFGFilterLAVSplitterBase : public CFGFilterLAV
{
protected:
    CAtlList<CStringA> m_enabledFormats, m_disabledFormats;

    CFGFilterLAVSplitterBase(CString path, const CLSID& clsid, CStringW name, bool bAddLowMeritSuffix, UINT64 merit, bool bIsPreview);

    void SetEnabledDisabledFormats(CComQIPtr<ILAVFSettings> pLAVFSettings);

public:
    struct Settings {
        BOOL bTrayIcon;
        std::wstring prefAudioLangs;
        std::wstring prefSubLangs;
        std::wstring subtitleAdvanced;
        LAVSubtitleMode subtitleMode;
        BOOL bPGSForcedStream;
        BOOL bPGSOnlyForced;
        int iVC1Mode;
        BOOL bSubstreams;

        BOOL bMatroskaExternalSegments;

        BOOL bStreamSwitchReselectSubs;

        BOOL bStreamSwitchRemoveAudio;
        BOOL bImpairedAudio;
        BOOL bPreferHighQualityAudio;
        DWORD dwQueueMaxPackets;
        DWORD dwQueueMaxMemSize;
        DWORD dwNetworkAnalysisDuration;

        void LoadSettings();
        void SaveSettings();

        bool GetSettings(CComQIPtr<ILAVFSettings> pLAVFSettings);
        bool SetSettings(CComQIPtr<ILAVFSettings> pLAVFSettings);
    };

    static const CString filename;

    virtual HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);

    static void ShowPropertyPages(CWnd* pParendWnd);

    void AddEnabledFormat(CStringA format) {
        m_enabledFormats.AddTail(format);
    }

    void AddDisabledFormat(CStringA format) {
        m_disabledFormats.AddTail(format);
    }
};

class CFGFilterLAVSplitter : public CFGFilterLAVSplitterBase
{
public:
    CFGFilterLAVSplitter(CString path, UINT64 merit = MERIT64_DO_USE, bool bAddLowMeritSuffix = false, bool bIsPreview = false);
};

class CFGFilterLAVSplitterSource : public CFGFilterLAVSplitterBase
{
public:
    CFGFilterLAVSplitterSource(CString path, UINT64 merit = MERIT64_DO_USE, bool bAddLowMeritSuffix = false, bool bIsPreview = false);
};

class CFGFilterLAVVideo : public CFGFilterLAV
{
public:
    struct Settings {
        BOOL bTrayIcon;
        DWORD dwStreamAR;
        DWORD dwNumThreads;
        BOOL bPixFmts[LAVOutPixFmt_NB];
        DWORD dwRGBRange;
        DWORD dwHWAccel;
        BOOL bHWFormats[HWCodec_NB];
        DWORD dwHWAccelResFlags;
        DWORD dwHWDeintMode;
        DWORD dwHWDeintOutput;
        DWORD dwDeintFieldOrder;
        LAVDeintMode deintMode;
        DWORD dwSWDeintMode;
        DWORD dwSWDeintOutput;
        DWORD dwDitherMode;
        DWORD dwHWAccelDeviceDXVA2;
        DWORD dwHWAccelDeviceDXVA2Desc;
        DWORD dwHWAccelDeviceD3D11;
        DWORD dwHWAccelDeviceD3D11Desc;
        BOOL bHWAccelCUVIDXVA;

        void LoadSettings();
        void SaveSettings();

        bool GetSettings(CComQIPtr<ILAVVideoSettings> pLAVFSettings);
        bool SetSettings(CComQIPtr<ILAVVideoSettings> pLAVFSettings);
    };

    static const CString filename;

    CFGFilterLAVVideo(CString path, UINT64 merit = MERIT64_DO_USE, bool bAddLowMeritSuffix = false, bool bIsPreview = false);

    virtual HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);

    static void ShowPropertyPages(CWnd* pParendWnd);

    static LPCTSTR GetUserFriendlyDecoderName(const LPCWSTR decoderName);
};

class CFGFilterLAVAudio : public CFGFilterLAV
{
public:
    struct Settings {
        BOOL bTrayIcon;
        BOOL bDRCEnabled;
        int iDRCLevel;
        BOOL bBitstream[Bitstream_NB];
        BOOL bDTSHDFraming;
        BOOL bBitstreamingFallback;
        BOOL bAutoAVSync;
        BOOL bExpandMono;
        BOOL bExpand61;
        BOOL bOutputStandardLayout;
        BOOL bOutput51Legacy;
        BOOL bAllowRawSPDIF;
        BOOL bSampleFormats[SampleFormat_NB];
        BOOL bSampleConvertDither;
        BOOL bAudioDelayEnabled;
        int  iAudioDelay;

        BOOL bMixingEnabled;
        DWORD dwMixingLayout;
        DWORD dwMixingFlags;
        DWORD dwMixingMode;
        DWORD dwMixingCenterLevel;
        DWORD dwMixingSurroundLevel;
        DWORD dwMixingLFELevel;

        void LoadSettings();
        void SaveSettings();

        bool GetSettings(CComQIPtr<ILAVAudioSettings> pLAVFSettings);
        bool SetSettings(CComQIPtr<ILAVAudioSettings> pLAVFSettings);
    };

    static const CString filename;

    CFGFilterLAVAudio(CString path, UINT64 merit = MERIT64_DO_USE, bool bAddLowMeritSuffix = false, bool bIsPreview = false);

    virtual HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);

    static void ShowPropertyPages(CWnd* pParendWnd);
};
