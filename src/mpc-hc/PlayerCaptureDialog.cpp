/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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
#include "MainFrm.h"
#include "PlayerCaptureDialog.h"
#include "DSUtil.h"
#include "moreuuids.h"
#include "../filters/muxer/WavDest/WavDest.h"
#include "../filters/muxer/MatroskaMuxer/MatroskaMuxer.h"
#include "../filters/muxer/DSMMuxer/DSMMuxer.h"
#include "../filters/transform/BufferFilter/BufferFilter.h"
#include "CMPCThemeMsgBox.h"

static CString NormalizeMediaName(const CString& displayName)
{
    CString normalizedName(displayName);
    normalizedName.Replace(_T('\\'), _T('/'));
    return normalizedName;
}

static bool LoadMediaType(CStringW displayName, AM_MEDIA_TYPE** ppmt)
{
    if (!ppmt) {
        return false;
    }

    *ppmt = nullptr;

    CString storeName(NormalizeMediaName(CString(displayName)));
    BYTE* pData;
    UINT len;
    if (!AfxGetApp()->GetProfileBinary(IDS_R_CAPTURE _T("\\") + storeName, _T("MediaType"), &pData, &len)) {
        return false;
    }
    if (len != sizeof(AM_MEDIA_TYPE)) {
        delete [] pData;
        return false;
    }

    AM_MEDIA_TYPE* pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!pmt) {
        delete [] pData;
        return false;
    }
    memcpy(pmt, pData, len);
    delete [] pData;

    // the saved struct holds pointers from the process that wrote it
    pmt->cbFormat = 0;
    pmt->pbFormat = nullptr;
    pmt->pUnk = nullptr;

    if (!AfxGetApp()->GetProfileBinary(IDS_R_CAPTURE _T("\\") + storeName, _T("Format"), &pData, &len)) {
        CoTaskMemFree(pmt);
        return false;
    }
    if (!len) {
        delete [] pData;
        CoTaskMemFree(pmt);
        return false;
    }
    pmt->pbFormat = (BYTE*)CoTaskMemAlloc(len);
    if (!pmt->pbFormat) {
        delete [] pData;
        CoTaskMemFree(pmt);
        return false;
    }
    pmt->cbFormat = len;
    memcpy(pmt->pbFormat, pData, len);
    delete [] pData;

    *ppmt = pmt;
    return true;
}

static void SaveMediaType(CStringW displayName, AM_MEDIA_TYPE* pmt)
{
    if (displayName.IsEmpty() || !pmt) {
        return;
    }

    CString storeName(NormalizeMediaName(CString(displayName)));
    AfxGetApp()->WriteProfileBinary(IDS_R_CAPTURE _T("\\") + storeName, _T("MediaType"), (BYTE*)pmt, sizeof(AM_MEDIA_TYPE));
    AfxGetApp()->WriteProfileBinary(IDS_R_CAPTURE _T("\\") + storeName, _T("Format"), pmt->pbFormat, pmt->cbFormat);
}

static void LoadDefaultCodec(CAtlArray<Codec>& codecs, CComboBox& box, const GUID& cat)
{
    int len = box.GetCount();
    if (len >= 0) {
        box.SetCurSel(0);
    }

    if (cat == GUID_NULL) {
        return;
    }

    CString displayName = AfxGetApp()->GetProfileString(IDS_R_CAPTURE _T("\\") + CStringFromGUID(cat), _T("DisplayName"));

    for (int i = 0; i < len; i++) {
        int iSel = (int)box.GetItemData(i);
        if (iSel < 0) {
            continue;
        }

        Codec& c = codecs[iSel];
        if (displayName == c.displayName) {
            box.SetCurSel(i);
            if (!c.pBF) {
                c.pMoniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&c.pBF));
            }
            break;
        }
    }
}

static void SaveDefaultCodec(CAtlArray<Codec>& codecs, const CComboBox& box, const GUID& cat)
{
    if (cat == GUID_NULL) {
        return;
    }

    CString guid = CStringFromGUID(cat);

    AfxGetApp()->WriteProfileString(IDS_R_CAPTURE _T("\\") + guid, nullptr, nullptr);

    int iSel = box.GetCurSel();
    if (iSel < 0) {
        return;
    }
    iSel = (int)box.GetItemData(iSel);
    if (iSel < 0) {
        return;
    }

    Codec& codec = codecs[iSel];

    AfxGetApp()->WriteProfileString(IDS_R_CAPTURE _T("\\") + guid, _T("DisplayName"), CString(codec.displayName));
}

static void SetupDefaultCaps(AM_MEDIA_TYPE* pmt, VIDEO_STREAM_CONFIG_CAPS& caps)
{
    ZeroMemory(&caps, sizeof(caps));

    if (!pmt) {
        return;
    }

    if (!pmt->pbFormat) {
        return;
    }

    BITMAPINFOHEADER* bih = (pmt->formattype == FORMAT_VideoInfo && pmt->cbFormat >= sizeof(VIDEOINFOHEADER))
                            ? &((VIDEOINFOHEADER*)pmt->pbFormat)->bmiHeader
                            : (pmt->formattype == FORMAT_VideoInfo2 && pmt->cbFormat >= sizeof(VIDEOINFOHEADER2))
                            ? &((VIDEOINFOHEADER2*)pmt->pbFormat)->bmiHeader
                            : nullptr;
    if (!bih) {
        return;
    }

    caps.guid = GUID_NULL;
    caps.VideoStandard = 0;
    caps.InputSize.cx = bih->biWidth;
    caps.InputSize.cy = abs(bih->biHeight);
    caps.MinCroppingSize = caps.MaxCroppingSize = caps.InputSize;
    caps.CropGranularityX = caps.CropGranularityY = 1;
    caps.CropAlignX = caps.CropAlignY = 1;
    caps.MinOutputSize = CSize(64, 64);
    caps.MaxOutputSize = CSize(768, 576);
    caps.OutputGranularityX = 16;
    caps.OutputGranularityY = 1;
    caps.StretchTapsX = caps.StretchTapsY = 0;
    caps.ShrinkTapsX = caps.ShrinkTapsY = 0;
    caps.MinFrameInterval = 100000i64;
    caps.MaxFrameInterval = 100000000i64;
    caps.MinBitsPerSecond = caps.MaxBitsPerSecond = 0;
}

static void SetupDefaultCaps(AM_MEDIA_TYPE* pmt, AUDIO_STREAM_CONFIG_CAPS& caps)
{
    ZeroMemory(&caps, sizeof(caps));

    if (!pmt || pmt->formattype != FORMAT_WaveFormatEx
            || !pmt->pbFormat || pmt->cbFormat < sizeof(WAVEFORMATEX)) {
        return;
    }

    WAVEFORMATEX* wfe = (WAVEFORMATEX*)pmt->pbFormat;

    caps.guid = GUID_NULL;
    caps.MinimumChannels = caps.MaximumChannels = wfe->nChannels;
    caps.ChannelsGranularity = 1;
    caps.MinimumBitsPerSample = caps.MaximumBitsPerSample = wfe->wBitsPerSample;
    caps.BitsPerSampleGranularity = 1;
    caps.MinimumSampleFrequency = caps.MaximumSampleFrequency = wfe->nSamplesPerSec;
    caps.SampleFrequencyGranularity = 1;
}

template<class T>
static void SetupMediaTypes(IAMStreamConfig* pAMSC, CFormatArray<T>& tfa, CComboBox& type, CComboBox& dim, CMediaType& mt)
{
    tfa.RemoveAll();
    type.ResetContent();
    dim.ResetContent();
    type.EnableWindow(FALSE);
    dim.EnableWindow(FALSE);

    if (!pAMSC) {
        return;
    }

    AM_MEDIA_TYPE* pcurmt = nullptr;
    pAMSC->GetFormat(&pcurmt);

    int iCount = 0, iSize;
    if (SUCCEEDED(pAMSC->GetNumberOfCapabilities(&iCount, &iSize))
            && iSize == sizeof(T) && iCount > 0) {
        for (int i = 0; i < iCount; i++) {
            T caps;
            AM_MEDIA_TYPE* pmt = nullptr;
            if (SUCCEEDED(pAMSC->GetStreamCaps(i, &pmt, (BYTE*)&caps))) {
                tfa.AddFormat(pmt, caps);
            }
        }

        if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
            for (size_t i = 0, cnt = tfa.GetCount(); i < cnt; i++) {
                if (tfa[i]->GetCount() != 1) {
                    continue;
                }

                CFormatElem<T>* pfe = tfa[i]->GetAt(0);

                if (pfe->mt.formattype != FORMAT_VideoInfo
                        && pfe->mt.formattype != FORMAT_VideoInfo2) {
                    continue;
                }

                static SIZE presets[] = {
                    {160, 120}, {192, 144},
                    {320, 240}, {384, 288},
                    {480, 240}, {512, 288},
                    {480, 360}, {512, 384},
                    {640, 240}, {768, 288},
                    {640, 480}, {768, 576},
                    {704, 240}, {704, 288},
                    {704, 480}, {704, 576},
                    {720, 240}, {720, 288},
                    {720, 480}, {720, 576},
                    {768, 240}, {768, 288},
                    {768, 480}, {768, 576},
                };

                VIDEO_STREAM_CONFIG_CAPS* pcaps = (VIDEO_STREAM_CONFIG_CAPS*)&pfe->caps;
                BITMAPINFOHEADER bihCur;
                ExtractBIH(&pfe->mt, &bihCur);

                for (size_t j = 0; j < _countof(presets); j++) {
                    if (presets[j].cx == bihCur.biWidth
                            && presets[j].cy == abs(bihCur.biHeight)
                            || presets[j].cx < pcaps->MinOutputSize.cx
                            || presets[j].cx > pcaps->MaxOutputSize.cx
                            || presets[j].cy < pcaps->MinOutputSize.cy
                            || presets[j].cy > pcaps->MaxOutputSize.cy
                            || pcaps->OutputGranularityX > 0 && (presets[j].cx % pcaps->OutputGranularityX) > 0
                            || pcaps->OutputGranularityY > 0 && (presets[j].cy % pcaps->OutputGranularityY) > 0) {
                        continue;
                    }

                    CMediaType mtCap = pfe->mt;

                    if (mtCap.formattype == FORMAT_VideoInfo) {
                        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mtCap.pbFormat;
                        vih->bmiHeader.biWidth = presets[j].cx;
                        vih->bmiHeader.biHeight = presets[j].cy * (vih->bmiHeader.biHeight < 0 ? -1 : 1);
                        vih->bmiHeader.biSizeImage = presets[j].cx * presets[j].cy * vih->bmiHeader.biBitCount >> 3;

                        AM_MEDIA_TYPE* pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
                        CopyMediaType(pmt, &mtCap);
                        tfa.AddFormat(pmt, pcaps, sizeof(*pcaps));

                        if (presets[j].cx * 3 != presets[j].cy * 4) {
                            int extra = mtCap.cbFormat - sizeof(VIDEOINFOHEADER);
                            int bmiHeaderSize = sizeof(vih->bmiHeader) + extra;
                            BYTE* pbmiHeader = DEBUG_NEW BYTE[bmiHeaderSize];
                            memcpy(pbmiHeader, &vih->bmiHeader, bmiHeaderSize);
                            mtCap.ReallocFormatBuffer(FIELD_OFFSET(VIDEOINFOHEADER2, bmiHeader) + bmiHeaderSize);
                            VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)mtCap.pbFormat;
                            memcpy(&vih2->bmiHeader, pbmiHeader, bmiHeaderSize);
                            delete [] pbmiHeader;
                            vih2->dwInterlaceFlags = vih2->dwCopyProtectFlags = 0;
                            vih2->dwReserved1 = vih2->dwReserved2 = 0;
                            vih2->dwPictAspectRatioX = 4;
                            vih2->dwPictAspectRatioY = 3;

                            AM_MEDIA_TYPE* pmt2 = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
                            CopyMediaType(pmt2, &mtCap);
                            tfa.AddFormat(pmt2, pcaps, sizeof(*pcaps));
                        }
                    } else if (mtCap.formattype == FORMAT_VideoInfo2) {
                        VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)mtCap.pbFormat;
                        vih2->bmiHeader.biWidth = presets[j].cx;
                        vih2->bmiHeader.biHeight = presets[j].cy * (vih2->bmiHeader.biHeight < 0 ? -1 : 1);
                        vih2->bmiHeader.biSizeImage = presets[j].cx * presets[j].cy * vih2->bmiHeader.biBitCount >> 3;
                        vih2->dwPictAspectRatioX = 4;
                        vih2->dwPictAspectRatioY = 3;

                        AM_MEDIA_TYPE* pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
                        CopyMediaType(pmt, &mtCap);
                        tfa.AddFormat(pmt, pcaps, sizeof(*pcaps));
                    }
                }
            }
        }
    }

    if (tfa.IsEmpty()) {
        if (pcurmt && (pcurmt->majortype == MEDIATYPE_Video || pcurmt->majortype == MEDIATYPE_Audio)) {
            AM_MEDIA_TYPE* pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
            CopyMediaType(pmt, pcurmt);
            T caps;
            SetupDefaultCaps(pmt, caps);
            tfa.AddFormat(pmt, caps);
        } else {
            mt.majortype = GUID_NULL;
            if (pcurmt) {
                DeleteMediaType(pcurmt);
            }
            return;
        }
    }

    for (size_t i = 0, cnt = tfa.GetCount(); i < cnt; i++) {
        CFormat<T>* pf = tfa[i];
        int j = type.AddString(pf->name);
        type.SetItemData(j, (DWORD_PTR)pf);
    }

    CFormat<T>* pf = nullptr;
    CFormatElem<T>* pfeCurrent = nullptr;

    if (!pcurmt) {
        pf = tfa[0];
        pfeCurrent = pf->GetAt(0);
    } else if (!tfa.FindFormat(pcurmt, nullptr, &pf, &pfeCurrent) && !tfa.FindFormat(pcurmt, &pf)) {
        if (pcurmt) {
            DeleteMediaType(pcurmt);
        }
        return;
    }

    for (size_t i = 0, cnt = pf->GetCount(); i < cnt; i++) {
        CFormatElem<T>* pfe = pf->GetAt(i);
        int j = dim.AddString(tfa.MakeDimensionName(pfe));
        dim.SetItemData(j, (DWORD_PTR)pfe);
    }

    int iType = type.SetCurSel(type.FindStringExact(0, pf->name));
    if (iType < 0 && type.GetCount()) {
        type.SetCurSel(0);
    }
    int iDim = dim.SetCurSel(dim.FindStringExact(0, tfa.MakeDimensionName(pfeCurrent)));
    //  if (iDim < 0 && dim.GetCount()) dim.SetCurSel(iDim = 0);

    CorrectComboListWidth(type);
    CorrectComboListWidth(dim);

    if (iDim >= 0) {
        mt = ((CFormatElem<T>*)dim.GetItemData(iDim))->mt;
    } else if (pcurmt) {
        mt = *pcurmt;
    }

    type.EnableWindow(type.GetCount() > 0);
    dim.EnableWindow(dim.GetCount() > 0);

    if (pcurmt) {
        DeleteMediaType(pcurmt);
    }
}

template<class T>
static bool SetupDimension(CFormatArray<T>& tfa, CComboBox& type, CComboBox& dim)
{
    CString str;
    dim.GetWindowText(str);

    dim.ResetContent();
    dim.EnableWindow(FALSE);

    int iSel = type.GetCurSel();
    if (iSel < 0) {
        return false;
    }

    CFormat<T>* pf = (CFormat<T>*)type.GetItemData(iSel);

    for (int i = 0; i < (int)pf->GetCount(); i++) {
        CFormatElem<T>* pfe = pf->GetAt(i);
        dim.SetItemData(dim.AddString(tfa.MakeDimensionName(pfe)), (DWORD_PTR)pfe);
    }

    CorrectComboListWidth(dim);

    dim.SetCurSel(dim.FindStringExact(0, str));
    dim.EnableWindow(dim.GetCount() > 0);

    return (dim.GetCurSel() >= 0);
}

static void InitCodecList(CAtlArray<Codec>& codecs, CComboBox& box, const GUID& cat)
{
    codecs.RemoveAll();
    box.ResetContent();
    box.EnableWindow(FALSE);

    box.SetItemData(box.AddString(_T("Uncompressed")), (DWORD_PTR) - 1);

    BeginEnumSysDev(cat, pMoniker) {
        Codec c;

        c.pMoniker = pMoniker;
        /*
        CComPtr<IBaseFilter> pBF;
        if (FAILED(pMoniker->BindToObject(0, 0, IID_PPV_ARGS(&pBF))) || !pBF) {
            continue;
        }
        c.pBF = pBF;
        */
        CComHeapPtr<OLECHAR> strName;
        if (FAILED(pMoniker->GetDisplayName(nullptr, nullptr, &strName))) {
            continue;
        }

        c.displayName = strName;

        CComPtr<IPropertyBag> pPB;
        pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPB));

        CComVariant var;
        if (!pPB || FAILED(pPB->Read(_T("FriendlyName"), &var, nullptr))) {
            continue;
        }

        c.friendlyName = var.bstrVal;

        CStringW str = CStringW(c.displayName).MakeLower();
        if (str.Find(L"@device:dmo:") == 0) {
            c.friendlyName = _T("(DMO) ") + c.friendlyName;
        } else if (str.Find(L"@device:sw:") == 0) {
            c.friendlyName = _T("(DS) ") + c.friendlyName;
        } else if (str.Find(L"@device:cm:") == 0) {
            c.friendlyName = _T("(VfW) ") + c.friendlyName;
        }

        box.SetItemData(box.AddString(c.friendlyName), (DWORD_PTR)codecs.Add(c));
    }
    EndEnumSysDev;

    box.EnableWindow(box.GetCount() > 1);

    CorrectComboListWidth(box);

    LoadDefaultCodec(codecs, box, cat);
}

static int ShowPPage(CAtlArray<Codec>& codecs, const CComboBox& box, HWND hWnd = nullptr)
{
    int iSel = box.GetCurSel();
    if (iSel < 0) {
        return -1;
    }

    iSel = (int)box.GetItemData(iSel);
    if (iSel < 0) {
        return -1;
    }

    Codec& c = codecs[iSel];

    if (!c.pBF) {
        c.pMoniker->BindToObject(nullptr, nullptr, IID_PPV_ARGS(&c.pBF));
    }

    if (CComQIPtr<ISpecifyPropertyPages> pSPP = c.pBF) {
        CAUUID caGUID;
        caGUID.pElems = nullptr;
        if (SUCCEEDED(pSPP->GetPages(&caGUID))) {
            IUnknown* lpUnk = nullptr;
            pSPP.QueryInterface(&lpUnk);
            OleCreatePropertyFrame(
                hWnd, 0, 0, CStringW(c.friendlyName),
                1, (IUnknown**)&lpUnk,
                caGUID.cElems, caGUID.pElems,
                0, 0, nullptr);
            lpUnk->Release();

            if (caGUID.pElems) {
                CoTaskMemFree(caGUID.pElems);
            }
        }
    } else if (CComQIPtr<IAMVfwCompressDialogs> pAMVfWCD = c.pBF) {
        if (pAMVfWCD->ShowDialog(VfwCompressDialog_QueryConfig, nullptr) == S_OK) {
            pAMVfWCD->ShowDialog(VfwCompressDialog_Config, hWnd);
        }
    }

    return iSel;
}

// CPlayerCaptureDialog dialog

//IMPLEMENT_DYNAMIC(CPlayerCaptureDialog, CMPCThemeResizableDialog)
CPlayerCaptureDialog::CPlayerCaptureDialog(CMainFrame* pMainFrame, CPlayerCaptureBar* playerCaptureBar)
    : CMPCThemeResizableDialog(CPlayerCaptureDialog::IDD, nullptr)
    , m_pMainFrame(pMainFrame)
    , playerCaptureBar(playerCaptureBar)
    , m_bInitialized(false)
    , m_vidfps(0)
    , m_nVidBuffers(0)
    , m_nAudBuffers(0)
    , m_nRecordTimerID(0)
    , m_fSepAudio(FALSE)
    , m_muxtype(0)
    , m_fEnableOgm(FALSE)
    , m_fVidOutput(TRUE)
    , m_fVidPreview(FALSE)
    , m_fAudOutput(TRUE)
    , m_fAudPreview(FALSE)
    , m_pVidBuffer(nullptr)
    , m_pAudBuffer(nullptr)
{
    EmptyVideo();
    EmptyAudio();
}

CPlayerCaptureDialog::~CPlayerCaptureDialog()
{
    EmptyVideo();
    EmptyAudio();
}

BOOL CPlayerCaptureDialog::Create(CWnd* pParent)
{
    return __super::Create(IDD, pParent);
}

void CPlayerCaptureDialog::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO4, m_vidinput);
    DDX_Control(pDX, IDC_COMBO1, m_vidtype);
    DDX_Control(pDX, IDC_COMBO5, m_viddimension);
    DDX_Control(pDX, IDC_SPIN1, m_vidhor);
    DDX_Control(pDX, IDC_SPIN2, m_vidver);
    DDX_Control(pDX, IDC_EDIT1, m_vidhoredit);
    DDX_Control(pDX, IDC_EDIT2, m_vidveredit);
    DDX_Control(pDX, IDC_EDIT3, m_vidfpsedit);
    DDX_Control(pDX, IDC_BUTTON1, m_vidsetres);
    DDX_Control(pDX, IDC_COMBO3, m_audinput);
    DDX_Control(pDX, IDC_COMBO2, m_audtype);
    DDX_Control(pDX, IDC_COMBO6, m_auddimension);
    DDX_Control(pDX, IDC_COMBO7, m_vidcodec);
    DDX_Control(pDX, IDC_COMBO9, m_vidcodectype);
    DDX_Control(pDX, IDC_COMBO10, m_vidcodecdimension);
    DDX_Check(pDX, IDC_CHECK1, m_fVidOutput);
    DDX_Control(pDX, IDC_CHECK1, m_vidoutput);
    DDX_Check(pDX, IDC_CHECK2, m_fVidPreview);
    DDX_Control(pDX, IDC_CHECK2, m_vidpreview);
    DDX_Control(pDX, IDC_COMBO8, m_audcodec);
    DDX_Control(pDX, IDC_COMBO12, m_audcodectype);
    DDX_Control(pDX, IDC_COMBO11, m_audcodecdimension);
    DDX_Check(pDX, IDC_CHECK3, m_fAudOutput);
    DDX_Control(pDX, IDC_CHECK3, m_audoutput);
    DDX_Check(pDX, IDC_CHECK4, m_fAudPreview);
    DDX_Control(pDX, IDC_CHECK4, m_audpreview);
    DDX_Text(pDX, IDC_EDIT4, m_file);
    DDX_Control(pDX, IDC_BUTTON2, m_recordbtn);
    DDX_Control(pDX, IDC_BUTTON3, m_openFile);
    DDX_Text(pDX, IDC_EDIT5, m_nVidBuffers);
    DDX_Text(pDX, IDC_EDIT6, m_nAudBuffers);
    DDX_Check(pDX, IDC_CHECK5, m_fSepAudio);
    DDX_CBIndex(pDX, IDC_COMBO14, m_muxtype);
    DDX_Control(pDX, IDC_COMBO14, m_muxctrl);
    fulfillThemeReqs();
}

BOOL CPlayerCaptureDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN) {
            CWnd* pFocused = GetFocus();
            if (pFocused && pFocused->m_hWnd == m_vidfpsedit.m_hWnd) {
                UpdateGraph();
            }
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CPlayerCaptureDialog::InitControls()
{
    if (!m_bInitialized) {
        m_bInitialized = true;

        InitCodecList(m_pVidEncArray, m_vidcodec, CLSID_VideoCompressorCategory);
        UpdateVideoCodec();

        InitCodecList(m_pAudEncArray, m_audcodec, CLSID_AudioCompressorCategory);
        UpdateAudioCodec();

        m_fEnableOgm = IsCLSIDRegistered(CLSID_OggMux);

        m_nVidBuffers = AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("VidBuffers"), 50);
        m_nAudBuffers = AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("AudBuffers"), 50);
        m_fVidOutput = !!AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("VidOutput"), TRUE);
        m_fAudOutput = !!AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("AudOutput"), TRUE);
        m_fVidPreview = AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("VidPreview"), TRUE);
        m_fAudPreview = AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("AudPreview"), TRUE);
        m_muxtype = AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("FileFormat"), 0);
        m_file = AfxGetApp()->GetProfileString(IDS_R_CAPTURE, _T("FileName"));
        m_fSepAudio = AfxGetApp()->GetProfileInt(IDS_R_CAPTURE, _T("SepAudio"), TRUE);

        CString dir = m_file.Left(m_file.ReverseFind('\\'));
        // Overwrite m_file if it isn't a valid path
        if (!PathFileExists(dir) || dir.IsEmpty()) {
            m_file.Empty();
            HRESULT hr = SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, 0, m_file.GetBuffer(2048));
            m_file.ReleaseBuffer();
            if (SUCCEEDED(hr)) {
                m_file.Append(_T("\\MPC-HC Capture"));
                if (!PathFileExists(m_file)) {
                    VERIFY(CreateDirectory(m_file, nullptr));
                }
            } else {
                // Use current directory
                m_file.ReleaseBufferSetLength(GetCurrentDirectory(2048, dir.GetBuffer(2048)));
            }
            m_file.AppendFormat(_T("\\%s_capture_[time].avi"), AfxGetApp()->m_pszExeName);
        }

        m_muxctrl.AddString(_T("AVI"));
        m_muxctrl.AddString(_T("Ogg Media"));
        m_muxctrl.AddString(_T("Matroska"));
        m_muxctrl.AddString(_T("DirectShow Media"));

        UpdateData(FALSE);

        OnChangeFileType();
    }
}

void CPlayerCaptureDialog::EmptyVideo()
{
    // first save channel from previous session
    if (m_pAMTuner && !m_vidDisplayName.IsEmpty()) {
        long lChannel = 0, lVivSub = 0, lAudSub = 0;
        m_pAMTuner->get_Channel(&lChannel, &lVivSub, &lAudSub);
        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE _T("\\") + CString(m_vidDisplayName), _T("Channel"), lChannel);
    }

    m_vfa.RemoveAll();

    m_pAMXB = nullptr;
    m_pAMTuner = nullptr;
    m_pAMVSC = nullptr;
    m_pAMVfwCD = nullptr;

    if (IsWindow(m_hWnd)) {
        m_vidinput.ResetContent();
        m_vidinput.EnableWindow(FALSE);
        m_vidtype.ResetContent();
        m_vidtype.EnableWindow(FALSE);
        m_viddimension.ResetContent();
        m_viddimension.EnableWindow(FALSE);
        m_vidhor.EnableWindow(FALSE);
        m_vidver.EnableWindow(FALSE);
        m_vidhoredit.EnableWindow(FALSE);
        m_vidveredit.EnableWindow(FALSE);
        m_vidfpsedit.EnableWindow(FALSE);
        m_vidfps = 0;
        m_vidsetres.EnableWindow(FALSE);
        UpdateData(FALSE);
    }
}

void CPlayerCaptureDialog::EmptyAudio()
{
    m_afa.RemoveAll();

    m_pAMASC = nullptr;
    m_pAMAIM.RemoveAll();

    if (IsWindow(m_hWnd)) {
        m_audinput.ResetContent();
        m_audinput.EnableWindow(FALSE);
        m_audtype.ResetContent();
        m_audtype.EnableWindow(FALSE);
        m_auddimension.ResetContent();
        m_auddimension.EnableWindow(FALSE);
        UpdateData(FALSE);
    }
}

void CPlayerCaptureDialog::UpdateMediaTypes()
{
    UpdateData();

    // fps

    CString fps;
    m_vidfpsedit.GetWindowText(fps);
    if (!fps.IsEmpty()) {
        float ffps;
        _stscanf_s(fps, _T("%f"), &ffps);
        if (ffps > 0) {
            m_vidfps = ffps;
        }
    }

    // video

    {
        AM_MEDIA_TYPE* pmt = nullptr;
        VIDEO_STREAM_CONFIG_CAPS* pcaps = nullptr;

        int i = m_viddimension.GetCurSel();
        if (i >= 0) {
            AM_MEDIA_TYPE* pmtcur = &((CVidFormatElem*)m_viddimension.GetItemData(i))->mt;
            if (pmtcur) {
                pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
                CopyMediaType(pmt, pmtcur);
                pcaps = &((CVidFormatElem*)m_viddimension.GetItemData(i))->caps;
            }
        } else if (m_pAMVSC) {
            m_pAMVSC->GetFormat(&pmt);
        }

        if (pmt) {
            if (m_vidfps > 0) {
                REFERENCE_TIME atpf = (REFERENCE_TIME)(10000000.0 / m_vidfps);

                if (pcaps) {
                    // FIXME: some drivers do not set the interval right and they still accept the preferable but unfortunately out-of-range fps
                    //                  atpf = min(max(atpf, pcaps->MinFrameInterval), pcaps->MaxFrameInterval);
                }

                if (pmt->formattype == FORMAT_VideoInfo) {
                    ((VIDEOINFOHEADER*)pmt->pbFormat)->AvgTimePerFrame = atpf;
                } else if (pmt->formattype == FORMAT_VideoInfo2) {
                    ((VIDEOINFOHEADER2*)pmt->pbFormat)->AvgTimePerFrame = atpf;
                }
            }

            CSize vidSize(m_vidhor.GetPos32(), m_vidver.GetPos32());
            if (vidSize.cx && vidSize.cy) {
                BITMAPINFOHEADER* bih = (pmt->formattype == FORMAT_VideoInfo)
                    ? &((VIDEOINFOHEADER*)pmt->pbFormat)->bmiHeader
                    : (pmt->formattype == FORMAT_VideoInfo2)
                    ? &((VIDEOINFOHEADER2*)pmt->pbFormat)->bmiHeader
                    : nullptr;
                if (bih) {
                    bih->biWidth = vidSize.cx;
                    bih->biHeight = vidSize.cy;
                    bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount >> 3;
                }
            }
            SaveMediaType(m_vidDisplayName, pmt);

            m_mtv = *pmt;
            DeleteMediaType(pmt);
        }
    }

    // audio

    {
        AM_MEDIA_TYPE* pmt = nullptr;

        int i = m_auddimension.GetCurSel();
        if (i >= 0) {
            AM_MEDIA_TYPE* pmtcur = &((CAudFormatElem*)m_auddimension.GetItemData(i))->mt;
            if (pmtcur) {
                pmt = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
                CopyMediaType(pmt, pmtcur);
            }
        } else if (m_pAMASC) {
            m_pAMASC->GetFormat(&pmt);
        }

        if (pmt) {
            SaveMediaType(m_audDisplayName, pmt);

            m_mta = *pmt;
            DeleteMediaType(pmt);
        }
    }
}

void CPlayerCaptureDialog::UpdateUserDefinableControls()
{
    int iSel = m_viddimension.GetCurSel();
    if (iSel < 0) {
        return;
    }

    CVidFormatElem* pvfe = (CVidFormatElem*)m_viddimension.GetItemData(iSel);
    if (!pvfe) {
        return;
    }

    if (!m_pAMVSC) {
        return;
    }

    AM_MEDIA_TYPE* pmt = nullptr;
    m_pAMVSC->GetFormat(&pmt);
    if (!pmt) {
        return;
    }

    BITMAPINFOHEADER* bih = (pmt->formattype == FORMAT_VideoInfo)
                            ? &((VIDEOINFOHEADER*)pmt->pbFormat)->bmiHeader
                            : (pmt->formattype == FORMAT_VideoInfo2)
                            ? &((VIDEOINFOHEADER2*)pmt->pbFormat)->bmiHeader
                            : nullptr;

    if (!bih) {
        return;
    }

    UDACCEL ua[3] = {{0, 0}, {2, 0}, {4, 0}};

    int w = m_vidhor.GetPos32(), h = m_vidver.GetPos32();
    UNREFERENCED_PARAMETER(w);
    UNREFERENCED_PARAMETER(h);

    m_vidhor.SetRange32(pvfe->caps.MinOutputSize.cx, pvfe->caps.MaxOutputSize.cx);
    /*
    // FIXME: bt8x8 drivers seem to crop the right side in yuv2 mode if the width is not dividable by 64
    if (bih->biCompression == mmioFOURCC('Y','U','Y','2')) {
        pvfe->caps.OutputGranularityX = 64;
    }
    */
    ua[0].nInc = pvfe->caps.OutputGranularityX;
    ua[1].nInc = pvfe->caps.OutputGranularityX * 2;
    ua[2].nInc = pvfe->caps.OutputGranularityX * 4;
    m_vidhor.SetAccel(3, ua);

    m_vidver.SetRange32(pvfe->caps.MinOutputSize.cy, pvfe->caps.MaxOutputSize.cy);
    ua[0].nInc = pvfe->caps.OutputGranularityY;
    ua[1].nInc = pvfe->caps.OutputGranularityY * 2;
    ua[2].nInc = pvfe->caps.OutputGranularityY * 4;
    m_vidver.SetAccel(3, ua);

    m_vidhor.SetPos32(bih->biWidth);
    m_vidver.SetPos32(abs(bih->biHeight));

    CString fps;
    fps.Format(_T("%.4f"), (float)(10000000.0 / ((VIDEOINFOHEADER*)pmt->pbFormat)->AvgTimePerFrame));
    m_vidfpsedit.SetWindowText(fps);

    DeleteMediaType(pmt);
}

void CPlayerCaptureDialog::UpdateVideoCodec()
{
    int iSel = m_vidcodec.GetCurSel();
    if (iSel >= 0) {
        iSel = (int)m_vidcodec.GetItemData(iSel);
    }

    m_pVidEnc = iSel < 0 ? nullptr : m_pVidEncArray[iSel].pBF;
    m_pVidEncMoniker = iSel < 0 ? nullptr : m_pVidEncArray[iSel].pMoniker;

    // CString displayName = iSel < 0 ? _T("") : CString(m_pVidEncArray[iSel].displayName.m_str);
    CComQIPtr<IAMStreamConfig> pAMSC = GetFirstPin(m_pVidEnc, PINDIR_OUTPUT);

    SetupMediaTypes(pAMSC, m_vcfa, m_vidcodectype, m_vidcodecdimension, m_mtcv);

    SaveDefaultCodec(m_pVidEncArray, m_vidcodec, CLSID_VideoCompressorCategory);

    // SaveMediaType(displayName, &m_mtcv);
}

void CPlayerCaptureDialog::UpdateAudioCodec()
{
    int iSel = m_audcodec.GetCurSel();
    if (iSel >= 0) {
        iSel = (int)m_audcodec.GetItemData(iSel);
    }

    m_pAudEnc = iSel < 0 ? nullptr : m_pAudEncArray[iSel].pBF;
    m_pAudEncMoniker = iSel < 0 ? nullptr : m_pAudEncArray[iSel].pMoniker;

    // CString displayName = iSel < 0 ? _T("") : CString(m_pAudEncArray[iSel].displayName.m_str);
    CComQIPtr<IAMStreamConfig> pAMSC = GetFirstPin(m_pAudEnc, PINDIR_OUTPUT);

    SetupMediaTypes(pAMSC, m_acfa, m_audcodectype, m_audcodecdimension, m_mtca);

    SaveDefaultCodec(m_pAudEncArray, m_audcodec, CLSID_AudioCompressorCategory);

    // SaveMediaType(displayName, &m_mtca);
}

void CPlayerCaptureDialog::UpdateMuxer()
{
    m_pMux = nullptr;
    m_pAudMux = nullptr;

    UpdateData();

    HRESULT hr;

    if (m_muxtype == 0) {
        m_pMux.CoCreateInstance(CLSID_AviDest);
    } else if (m_muxtype == 1) {
        m_pMux.CoCreateInstance(CLSID_OggMux);
    } else if (m_muxtype == 2) {
        m_pMux = DEBUG_NEW CMatroskaMuxerFilter(nullptr, &hr);
    } else if (m_muxtype == 3) {
        m_pMux = DEBUG_NEW CDSMMuxerFilter(nullptr, &hr);
    } else {
        return;
    }

    if (m_fSepAudio) {
        m_pAudMux = DEBUG_NEW CWavDestFilter(nullptr, &hr);
    }
}

void CPlayerCaptureDialog::UpdateOutputControls()
{
    UpdateData();

    m_recordbtn.EnableWindow(!m_file.IsEmpty() && (m_pAMVSC && m_fVidOutput || m_pAMASC && m_fAudOutput));
    m_vidcodec.EnableWindow(TRUE);
    m_audcodec.EnableWindow(TRUE);
}

void CPlayerCaptureDialog::UpdateGraph()
{
    UpdateMediaTypes();

    m_pMainFrame->BuildGraphVideoAudio(m_fVidPreview, false, m_fAudPreview, false);

    UpdateUserDefinableControls();
}

void CPlayerCaptureDialog::EnableControls(CWnd* pWnd, bool fEnable)
{
    if (fEnable) {
        for (CWnd* pChild = pWnd->GetWindow(GW_CHILD); pChild; pChild = pChild->GetNextWindow()) {
            BOOL fEnabled;
            if (m_wndenabledmap.Lookup(pChild->m_hWnd, fEnabled)) {
                pChild->EnableWindow(fEnabled);
            }
            EnableControls(pChild, fEnable);
        }

        if (pWnd->m_hWnd == m_hWnd) {
            m_wndenabledmap.RemoveAll();
        }

        m_recordbtn.SetWindowText(ResStr(IDS_RECORD_START));
    } else {
        if (pWnd->m_hWnd == m_hWnd) {
            m_wndenabledmap.RemoveAll();
        }

        for (CWnd* pChild = pWnd->GetWindow(GW_CHILD); pChild; pChild = pChild->GetNextWindow()) {
            m_wndenabledmap[pChild->m_hWnd] = pChild->IsWindowEnabled();
            pChild->EnableWindow(FALSE);
            EnableControls(pChild, fEnable);
        }

        m_recordbtn.EnableWindow(TRUE);
        m_recordbtn.SetWindowText(ResStr(IDS_RECORD_STOP));
    }
}

void CPlayerCaptureDialog::SetupVideoControls(
    CStringW displayName,
    IAMStreamConfig* pAMSC, IAMCrossbar* pAMXB, IAMTVTuner* pAMTuner)
{
    if (!this) {
        ASSERT(false);
        return;
    }

    EmptyVideo();

    m_vidDisplayName = displayName;
    m_pAMXB = pAMXB;
    m_pAMTuner = pAMTuner;
    m_pAMVSC = pAMSC;

    if (pAMXB || pAMTuner || pAMSC) {
        UpdateVideoControls();
    }
}

void CPlayerCaptureDialog::SetupVideoControls(
    CStringW displayName,
    IAMStreamConfig* pAMSC, IAMVfwCaptureDialogs* pAMVfwCD)
{
    EmptyVideo();

    m_vidDisplayName = displayName;
    m_pAMVSC = pAMSC;
    m_pAMVfwCD = pAMVfwCD;

    if (pAMSC || pAMVfwCD) {
        UpdateVideoControls();
    }
}

void CPlayerCaptureDialog::UpdateVideoControls()
{
    ASSERT(!m_pAMXB || !m_pAMVfwCD);

    // crossbar

    if (m_pAMXB) {
        long OutputPinCount, InputPinCount;
        if (SUCCEEDED(m_pAMXB->get_PinCounts(&OutputPinCount, &InputPinCount))) {
            for (int i = 0; i < InputPinCount; i++) {
                long PinIndexRelated, PhysicalType;
                if (FAILED(m_pAMXB->get_CrossbarPinInfo(TRUE, i, &PinIndexRelated, &PhysicalType))) {
                    continue;
                }

                if (PhysicalType >= PhysConn_Audio_Tuner) {
                    continue;
                }

                CString str;
                switch (PhysicalType) {
                    case PhysConn_Video_Tuner:
                        str = _T("Tuner");
                        break;
                    case PhysConn_Video_Composite:
                        str = _T("Composite");
                        break;
                    case PhysConn_Video_SVideo:
                        str = _T("SVideo");
                        break;
                    case PhysConn_Video_RGB:
                        str = _T("RGB");
                        break;
                    case PhysConn_Video_YRYBY:
                        str = _T("YRYBY");
                        break;
                    case PhysConn_Video_SerialDigital:
                        str = _T("SerialDigital");
                        break;
                    case PhysConn_Video_ParallelDigital:
                        str = _T("ParallelDigital");
                        break;
                    case PhysConn_Video_SCSI:
                        str = _T("SCSI");
                        break;
                    case PhysConn_Video_AUX:
                        str = _T("AUX");
                        break;
                    case PhysConn_Video_1394:
                        str = _T("1394");
                        break;
                    case PhysConn_Video_USB:
                        str = _T("USB");
                        break;
                    case PhysConn_Video_VideoDecoder:
                        str = _T("VideoDecoder");
                        break;
                    case PhysConn_Video_VideoEncoder:
                        str = _T("VideoEncoder");
                        break;
                    case PhysConn_Video_SCART:
                        str = _T("SCART");
                        break;
                    default:
                        str.Format(_T("PhysicalType %ld"), PhysicalType);
                        break;
                }

                m_vidinput.SetItemData(m_vidinput.AddString(str), i);
            }
        }

        if (m_vidinput.GetCount() > 0) {
            m_vidinput.EnableWindow(TRUE);

            if (SUCCEEDED(m_pAMXB->get_PinCounts(&OutputPinCount, &InputPinCount))) {
                for (int i = 0; i < OutputPinCount; i++) {
                    long InputPinIndex;
                    if (S_OK == m_pAMXB->get_IsRoutedTo(i, &InputPinIndex)) {
                        for (int j = 0; j < m_vidinput.GetCount(); j++) {
                            if (m_vidinput.GetItemData(j) == (DWORD_PTR)InputPinIndex) {
                                m_vidinput.SetCurSel(j);
                                i = OutputPinCount;
                                break;
                            }
                        }
                    }
                }
            }
            OnVideoInput();
        }
    }

    // VFW capture dialog

    if (m_pAMVfwCD) {
        if (S_OK == m_pAMVfwCD->HasDialog(VfwCaptureDialog_Source)) {
            m_vidinput.SetItemData(m_vidinput.AddString(_T("Source")), (DWORD_PTR)VfwCaptureDialog_Source);
        }
        if (S_OK == m_pAMVfwCD->HasDialog(VfwCaptureDialog_Format)) {
            m_vidinput.SetItemData(m_vidinput.AddString(_T("Format")), (DWORD_PTR)VfwCaptureDialog_Format);
        }
        if (S_OK == m_pAMVfwCD->HasDialog(VfwCaptureDialog_Display)) {
            m_vidinput.SetItemData(m_vidinput.AddString(_T("Display")), (DWORD_PTR)VfwCaptureDialog_Display);
        }

        if (m_vidinput.GetCount() > 0) {
            m_vidinput.EnableWindow(TRUE);
            m_vidinput.SetCurSel(0);
        }
    }

    // tuner

    if (m_pAMTuner) {
        // TODO:...
    }

    // streamconfig

    if (m_pAMVSC) {
        AM_MEDIA_TYPE* pmt;
        if (LoadMediaType(m_vidDisplayName, &pmt)) {
            m_pAMVSC->SetFormat(pmt);
            DeleteMediaType(pmt);
        }

        SetupMediaTypes(m_pAMVSC, m_vfa, m_vidtype, m_viddimension, m_mtv);
    }

    if (m_vidtype.GetCount() > 0) {
        m_vidfpsedit.EnableWindow(TRUE);
        m_vidhor.EnableWindow(TRUE);
        m_vidver.EnableWindow(TRUE);
        m_vidhoredit.EnableWindow(TRUE);
        m_vidveredit.EnableWindow(TRUE);
        m_vidsetres.EnableWindow(TRUE);
    }

    {
        m_vidoutput.EnableWindow(TRUE);
        m_vidpreview.EnableWindow(TRUE);
    }

    UpdateMediaTypes();

    UpdateUserDefinableControls();

    UpdateOutputControls();
}

void CPlayerCaptureDialog::SetupAudioControls(
    CStringW displayName,
    IAMStreamConfig* pAMSC, const CInterfaceArray<IAMAudioInputMixer>& pAMAIM)
{
    if (!this) {
        return;
    }

    EmptyAudio();

    m_audDisplayName = displayName;
    m_pAMASC = pAMSC;
    if (!pAMAIM.IsEmpty()) {
        m_pAMAIM.Copy(pAMAIM);
    }

    if (pAMSC || !pAMAIM.IsEmpty()) {
        UpdateAudioControls();
    }
}

void CPlayerCaptureDialog::UpdateAudioControls()
{
    // input selection

    if (!m_pAMAIM.IsEmpty()) {
        size_t iSel = SIZE_T_MAX;

        for (size_t i = 0; i < m_pAMAIM.GetCount(); i++) {
            CComQIPtr<IPin> pPin = m_pAMAIM[i];
            m_audinput.SetItemData(m_audinput.AddString(CString(GetPinName(pPin))), i);

            BOOL fEnable;
            if (SUCCEEDED(m_pAMAIM[i]->get_Enable(&fEnable)) && fEnable) {
                iSel = i;
            }
        }

        if (m_audinput.GetCount() > 0) {
            for (int i = 0; i < m_audinput.GetCount(); i++) {
                if (m_audinput.GetItemData(i) == iSel) {
                    m_audinput.SetCurSel(i);
                    break;
                }
            }

            m_audinput.EnableWindow(TRUE);
        }
    }

    // stream config

    if (m_pAMASC) {
        AM_MEDIA_TYPE* pmt;
        if (LoadMediaType(m_audDisplayName, &pmt)) {
            m_pAMASC->SetFormat(pmt);
            DeleteMediaType(pmt);
        }

        SetupMediaTypes(m_pAMASC, m_afa, m_audtype, m_auddimension, m_mta);
    }

    //if (!m_audtype.IsEmpty())
    {
        m_audoutput.EnableWindow(TRUE);
        m_audpreview.EnableWindow(TRUE);
    }

    UpdateMediaTypes();

    UpdateUserDefinableControls();

    UpdateOutputControls();
}

bool CPlayerCaptureDialog::IsTunerActive()
{
    int iSel = m_vidinput.GetCurSel();
    if (iSel < 0) {
        return false;
    }
    iSel = (int)m_vidinput.GetItemData(iSel);
    if (iSel < 0) {
        return false;
    }

    long pinIndexRelated = -1, physicalType = -1;
    return (m_pAMXB
            && SUCCEEDED(m_pAMXB->get_CrossbarPinInfo(TRUE, iSel, &pinIndexRelated, &physicalType))
            && physicalType == PhysConn_Video_Tuner);
}

bool CPlayerCaptureDialog::SetVideoInput(int input)
{
    if (!m_pAMXB || input < 0) {
        return false;
    }

    for (int i = 0; i < m_vidinput.GetCount(); i++) {
        if (m_vidinput.GetItemData(i) == (DWORD_PTR)input) {
            m_vidinput.SetCurSel(i);
            OnVideoInput();
            return true;
        }
    }

    return false;
}

bool CPlayerCaptureDialog::SetVideoChannel(int channel)
{
    if (!m_pAMTuner || channel < 0) {
        return false;
    }

    return SUCCEEDED(m_pAMTuner->put_Channel(channel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT));
}

bool CPlayerCaptureDialog::SetAudioInput(int input)
{
    if (input < 0) {
        return false;
    }

    for (int i = 0; i < m_audinput.GetCount(); i++) {
        if (m_audinput.GetItemData(i) == (DWORD_PTR)input) {
            m_audinput.SetCurSel(i);
            OnAudioInput();
            return true;
        }
    }

    return false;
}

int CPlayerCaptureDialog::GetVideoInput() const
{
    int i = m_vidinput.GetCurSel();
    if (i < 0) {
        return -1;
    }
    return (int)m_vidinput.GetItemData(i);
}

int CPlayerCaptureDialog::GetVideoChannel()
{
    long lChannel = -1, lVivSub = -1, lAudSub = -1;
    return m_pAMTuner && SUCCEEDED(m_pAMTuner->get_Channel(&lChannel, &lVivSub, &lAudSub)) ? lChannel : -1;
}

int CPlayerCaptureDialog::GetAudioInput() const
{
    int i = m_audinput.GetCurSel();
    if (i < 0) {
        return -1;
    }
    return (int)m_audinput.GetItemData(i);
}

BEGIN_MESSAGE_MAP(CPlayerCaptureDialog, CMPCThemeResizableDialog)
    ON_WM_DESTROY()
    ON_CBN_SELCHANGE(IDC_COMBO4, OnVideoInput)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnVideoType)
    ON_CBN_SELCHANGE(IDC_COMBO5, OnVideoDimension)
    ON_BN_CLICKED(IDC_BUTTON1, OnOverrideVideoDimension)
    ON_CBN_SELCHANGE(IDC_COMBO3, OnAudioInput)
    ON_CBN_SELCHANGE(IDC_COMBO2, OnAudioType)
    ON_CBN_SELCHANGE(IDC_COMBO6, OnAudioDimension)
    ON_BN_CLICKED(IDC_CHECK1, OnRecordVideo)
    ON_CBN_SELCHANGE(IDC_COMBO7, OnVideoCodec)
    ON_CBN_SELCHANGE(IDC_COMBO9, OnVideoCodecType)
    ON_CBN_SELCHANGE(IDC_COMBO10, OnVideoCodecDimension)
    ON_BN_CLICKED(IDC_CHECK3, OnRecordAudio)
    ON_CBN_SELCHANGE(IDC_COMBO8, OnAudioCodec)
    ON_CBN_SELCHANGE(IDC_COMBO12, OnAudioCodecType)
    ON_CBN_SELCHANGE(IDC_COMBO11, OnAudioCodecDimension)
    ON_BN_CLICKED(IDC_BUTTON3, OnOpenFile)
    ON_BN_CLICKED(IDC_BUTTON2, OnRecord)
    ON_EN_CHANGE(IDC_EDIT5, OnChangeVideoBuffers)
    ON_EN_CHANGE(IDC_EDIT6, OnChangeAudioBuffers)
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_CHECK2, OnBnClickedVidAudPreview)
    ON_BN_CLICKED(IDC_CHECK4, OnBnClickedVidAudPreview)
    ON_BN_CLICKED(IDC_CHECK5, OnBnClickedAudioToWav)
    ON_CBN_SELCHANGE(IDC_COMBO14, OnChangeFileType)
END_MESSAGE_MAP()


// CPlayerCaptureDialog message handlers

BOOL CPlayerCaptureDialog::OnInitDialog()
{
    __super::OnInitDialog();

    return FALSE;  // return FALSE so that the dialog does not steal focus
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CPlayerCaptureDialog::OnDestroy()
{
    if (m_bInitialized) {
        UpdateData();

        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("VidOutput"), m_fVidOutput);
        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("AudOutput"), m_fAudOutput);
        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("VidPreview"), m_fVidPreview);
        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("AudPreview"), m_fAudPreview);
        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("FileFormat"), m_muxtype);
        AfxGetApp()->WriteProfileString(IDS_R_CAPTURE, _T("FileName"), m_file);
        AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("SepAudio"), m_fSepAudio);

        m_bInitialized = false;
    }

    __super::OnDestroy();
}

void CPlayerCaptureDialog::OnVideoInput()
{
    int iSel = m_vidinput.GetCurSel();
    if (iSel < 0) {
        return;
    }
    iSel = (int)m_vidinput.GetItemData(iSel);
    if (iSel < 0) {
        return;
    }

    if (m_pAMXB) {
        long PinIndexRelated, PhysicalType;
        if (FAILED(m_pAMXB->get_CrossbarPinInfo(TRUE, iSel, &PinIndexRelated, &PhysicalType))) {
            return;
        }

        long OutputPinCount, InputPinCount;
        if (FAILED(m_pAMXB->get_PinCounts(&OutputPinCount, &InputPinCount))) {
            return;
        }

        for (int i = 0; i < OutputPinCount; i++) {
            if (S_OK == m_pAMXB->CanRoute(i, iSel)) {
                m_pAMXB->Route(i, iSel);
                break;
            }
        }

        if (PinIndexRelated >= 0) {
            for (int i = 0; i < OutputPinCount; i++) {
                if (S_OK == m_pAMXB->CanRoute(i, PinIndexRelated)) {
                    m_pAMXB->Route(i, PinIndexRelated);
                    break;
                }
            }
        }
    } else if (m_pAMVfwCD) {
        if (S_OK == m_pAMVfwCD->HasDialog(iSel)) {
            HRESULT hr = m_pAMVfwCD->ShowDialog(iSel, m_hWnd);

            if (VFW_E_NOT_STOPPED == hr) {
                m_pMainFrame->SendMessage(WM_COMMAND, ID_PLAY_STOP);
                hr = m_pAMVfwCD->ShowDialog(iSel, m_hWnd);
                m_pMainFrame->SendMessage(WM_COMMAND, ID_PLAY_PLAY);
            }

            if (VFW_E_CANNOT_CONNECT == hr) {
                UpdateGraph();
            }
        }
    }
}

void CPlayerCaptureDialog::OnVideoType()
{
    if (SetupDimension(m_vfa, m_vidtype, m_viddimension)) {
        OnVideoDimension();
    }
}

void CPlayerCaptureDialog::OnVideoDimension()
{
    int iSel = m_viddimension.GetCurSel();
    if (iSel < 0) {
        return;
    }

    CVidFormatElem* pvfe = (CVidFormatElem*)m_viddimension.GetItemData(iSel);
    if (!pvfe) {
        return;
    }

    BITMAPINFOHEADER* bih = (pvfe->mt.formattype == FORMAT_VideoInfo)
                            ? &((VIDEOINFOHEADER*)pvfe->mt.pbFormat)->bmiHeader
                            : (pvfe->mt.formattype == FORMAT_VideoInfo2)
                            ? &((VIDEOINFOHEADER2*)pvfe->mt.pbFormat)->bmiHeader
                            : nullptr;
    if (!bih) {
        return;
    }

    m_vidhor.SetRange32(0, UD_MAXVAL);
    m_vidver.SetRange32(0, UD_MAXVAL);
    m_vidhor.SetPos32(bih->biWidth);
    m_vidver.SetPos32(abs(bih->biHeight));
    CString fps;
    fps.Format(_T("%.4f"), (float)(10000000.0 / ((VIDEOINFOHEADER*)pvfe->mt.pbFormat)->AvgTimePerFrame));
    m_vidfpsedit.SetWindowText(fps);

    UpdateGraph();
}

void CPlayerCaptureDialog::OnOverrideVideoDimension()
{
    UpdateGraph();
}

void CPlayerCaptureDialog::OnAudioInput()
{
    int iSel = m_audinput.GetCurSel();

    for (int i = 0; i < (int)m_pAMAIM.GetCount(); i++) {
        m_pAMAIM[m_audinput.GetItemData(i)]->put_Enable(i == iSel ? TRUE : FALSE);
    }
}

void CPlayerCaptureDialog::OnAudioType()
{
    if (SetupDimension(m_afa, m_audtype, m_auddimension)) {
        OnAudioDimension();
    }
}

void CPlayerCaptureDialog::OnAudioDimension()
{
    UpdateGraph();
}

void CPlayerCaptureDialog::OnRecordVideo()
{
    UpdateOutputControls();
}

void CPlayerCaptureDialog::OnVideoCodec()
{
    ShowPPage(m_pVidEncArray, m_vidcodec, m_hWnd);
    UpdateVideoCodec();
}

void CPlayerCaptureDialog::OnVideoCodecType()
{
    if (SetupDimension(m_vcfa, m_vidcodectype, m_vidcodecdimension)) {
        OnVideoCodecDimension();
    }
}

void CPlayerCaptureDialog::OnVideoCodecDimension()
{
    int i = m_vidcodecdimension.GetCurSel();
    if (i >= 0) {
        m_mtcv = ((CVidFormatElem*)m_vidcodecdimension.GetItemData(i))->mt;

        // we have to recreate the encoder, otherwise it will accept the new media type for only the first time
        m_pVidEnc = nullptr;
        m_pVidEncMoniker->BindToObject(0, 0, IID_PPV_ARGS(&m_pVidEnc));
    }
}

void CPlayerCaptureDialog::OnRecordAudio()
{
    UpdateOutputControls();
}

void CPlayerCaptureDialog::OnAudioCodec()
{
    ShowPPage(m_pAudEncArray, m_audcodec, m_hWnd);
    UpdateAudioCodec();
}

void CPlayerCaptureDialog::OnAudioCodecType()
{
    if (SetupDimension(m_acfa, m_audcodectype, m_audcodecdimension)) {
        OnAudioCodecDimension();
    }
}

void CPlayerCaptureDialog::OnAudioCodecDimension()
{
    int i = m_audcodecdimension.GetCurSel();
    if (i >= 0) {
        m_mtca = ((CAudFormatElem*)m_audcodecdimension.GetItemData(i))->mt;

        // we have to recreate the encoder, otherwise it will accept the new media type for only the first time
        m_pAudEnc = nullptr;
        m_pAudEncMoniker->BindToObject(0, 0, IID_PPV_ARGS(&m_pAudEnc));
        /*
                SaveMediaType(
                    CString(m_pAudEncArray[m_audcodec.GetItemData(m_audcodec.GetCurSel())].displayName.m_str),
                    &m_mtca);
        */
    }
}

void CPlayerCaptureDialog::OnOpenFile()
{
    CFileDialog fd(FALSE, nullptr, nullptr,
                   OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR,
                   _T("Media files (*.avi,*.ogm,*.mkv,*.dsm)|*.avi;*.ogm;*.mkv;*.dsm|"), this, 0);

    playerCaptureBar->OnEnterMenuLoop(false);

    if (fd.DoModal() == IDOK) {
        CString str = fd.GetPathName();

        CString ext = str.Mid(str.ReverseFind('.') + 1).MakeLower();
        if (ext == _T("avi")) {
            m_muxtype = 0;
        } else if (ext == _T("ogm")) {
            m_muxtype = 1;
        } else if (ext == _T("mkv")) {
            m_muxtype = 2;
        } else if (ext == _T("dsm")) {
            m_muxtype = 3;
        } else {
            if (m_muxtype == 0) {
                str += _T(".avi");
            } else if (m_muxtype == 1) {
                str += _T(".ogm");
            } else if (m_muxtype == 2) {
                str += _T(".mkv");
            } else if (m_muxtype == 3) {
                str += _T(".dsm");
            }
        }

        m_file = str;

        UpdateData(FALSE);
    }
    playerCaptureBar->OnExitMenuLoop(false);
    GetDlgItem(IDC_EDIT4)->SetFocus(); //this is going to keep the bar from auto-hiding after exiting the modal dialog
    UpdateOutputControls();
}

void CPlayerCaptureDialog::OnRecord()
{
    UpdateData();

    if (!m_pMainFrame->m_fCapturing) {
        UpdateMuxer();

        CComQIPtr<IFileSinkFilter2> pFSF = m_pMux;
        if (pFSF) {
            m_pDst = m_pMux;
        } else {
            m_pDst = nullptr;
            m_pDst.CoCreateInstance(CLSID_FileWriter);
            pFSF = m_pDst;
        }

        // Replace [time] with current timestamp in the file name.
        CString file = m_file;
        CString sTime = CTime::GetCurrentTime().Format(_T("%Y-%m-%d_%H-%M-%S"));
        file.Replace(_T("[time]"), sTime);

        if (!pFSF
                || FAILED(pFSF->SetFileName(CStringW(file), nullptr))
                || FAILED(pFSF->SetMode(AM_FILE_OVERWRITE))) {
            CMPCThemeMsgBox::MessageBoxW(this, ResStr(IDS_CAPTURE_ERROR_OUT_FILE), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
            return;
        }

        CString audfn = file.Left(file.ReverseFind('.') + 1);
        if (m_fSepAudio && m_fAudOutput && m_pAudMux && !audfn.IsEmpty()) {
            audfn += _T("wav");

            CComQIPtr<IFileSinkFilter2> pFSFAudioMux = m_pAudMux;
            if (pFSFAudioMux) {
                m_pAudDst = m_pAudMux;
            } else {
                m_pAudDst = nullptr;
                m_pAudDst.CoCreateInstance(CLSID_FileWriter);
                pFSFAudioMux = m_pAudDst;
            }

            if (!pFSFAudioMux
                    || FAILED(pFSFAudioMux->SetFileName(CStringW(audfn), nullptr))
                    || FAILED(pFSFAudioMux->SetMode(AM_FILE_OVERWRITE))) {
                CMPCThemeMsgBox::MessageBoxW(this, ResStr(IDS_CAPTURE_ERROR_AUD_OUT_FILE), ResStr(IDS_CAPTURE_ERROR), MB_ICONERROR | MB_OK);
                return;
            }
        }

        m_pVidBuffer = m_fVidOutput && m_nVidBuffers > 0 && m_muxtype != 2 && m_muxtype != 3 ? DEBUG_NEW CBufferFilter(nullptr, nullptr) : nullptr;
        if (CComQIPtr<IBufferFilter> pVB = m_pVidBuffer) {
            pVB->SetBuffers(m_nVidBuffers);
            pVB->SetPriority(THREAD_PRIORITY_NORMAL);
        }

        m_pAudBuffer = m_fAudOutput && m_nAudBuffers > 0 && m_muxtype != 2 && m_muxtype != 3 ? DEBUG_NEW CBufferFilter(nullptr, nullptr) : nullptr;
        if (CComQIPtr<IBufferFilter> pAB = m_pAudBuffer) {
            pAB->SetBuffers(m_nAudBuffers);
            pAB->SetPriority(THREAD_PRIORITY_ABOVE_NORMAL);
        }

        EnableControls(this, false);

        m_pMainFrame->StartCapture();

        m_nRecordTimerID = SetTimer(1, 100, nullptr);
    } else {
        KillTimer(m_nRecordTimerID);
        m_nRecordTimerID = 0;

        m_pMainFrame->StopCapture();
        EnableControls(this, true);

        m_pVidBuffer = nullptr;
        m_pAudBuffer = nullptr;
    }
}

void CPlayerCaptureDialog::OnChangeVideoBuffers()
{
    UpdateData();
    AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("VidBuffers"), std::max(m_nVidBuffers, 0));
}

void CPlayerCaptureDialog::OnChangeAudioBuffers()
{
    UpdateData();
    AfxGetApp()->WriteProfileInt(IDS_R_CAPTURE, _T("AudBuffers"), std::max(m_nAudBuffers, 0));
}

void CPlayerCaptureDialog::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nRecordTimerID) {
        if (m_pMainFrame->m_fCapturing) {
            ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
            if (GetDiskFreeSpaceEx(m_file.Left(m_file.ReverseFind('\\') + 1), &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes)
                    && FreeBytesAvailable.QuadPart < 1024i64 * 1024 * 10) {
                OnRecord();
            }
        }
    } else {
        __super::OnTimer(nIDEvent);
    }
}

void CPlayerCaptureDialog::OnBnClickedVidAudPreview()
{
    UpdateData();
    UpdateGraph();
}

void CPlayerCaptureDialog::OnBnClickedAudioToWav()
{
    //  UpdateMuxer();
}

void CPlayerCaptureDialog::OnChangeFileType()
{
    UpdateData();

    CString ext = m_file.Mid(m_file.ReverseFind('.') + 1).MakeLower();

    if (m_muxtype == 0 && ext != _T("avi")) {
        m_file = m_file.Left(m_file.GetLength() - 4) + _T(".avi");
    } else if (m_muxtype == 1 && ext != _T("ogm")) {
        m_file = m_file.Left(m_file.GetLength() - 4) + _T(".ogm");
    } else if (m_muxtype == 2 && ext != _T("mkv")) {
        m_file = m_file.Left(m_file.GetLength() - 4) + _T(".mkv");
    } else if (m_muxtype == 3 && ext != _T("dsm")) {
        m_file = m_file.Left(m_file.GetLength() - 4) + _T(".dsm");
    }

    UpdateData(FALSE);

    GetDlgItem(IDC_EDIT5)->EnableWindow(m_muxtype != 2 && m_muxtype != 3);
    GetDlgItem(IDC_EDIT6)->EnableWindow(m_muxtype != 2 && m_muxtype != 3);

    m_recordbtn.EnableWindow(m_muxtype != 1 || m_fEnableOgm);
}
