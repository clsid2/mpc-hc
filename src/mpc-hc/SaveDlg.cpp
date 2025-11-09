/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2017 see Authors.txt
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
#include "SaveDlg.h"
#include "../filters/Filters.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"

// CSaveDlg dialog

IMPLEMENT_DYNAMIC(CSaveDlg, CDialog)
CSaveDlg::CSaveDlg(CString in, CString out, CWnd* pParent /*=nullptr*/)
    : CMPCThemeResizableDialog(CSaveDlg::IDD, pParent)
    , m_in(in)
    , m_out(out)
    , m_nIDTimerEvent((UINT_PTR) - 1)
{
}

CSaveDlg::~CSaveDlg()
{
}

void CSaveDlg::DoDataExchange(CDataExchange* pDX)
{
    CMPCThemeResizableDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_ANIMATE1, m_anim);
    DDX_Control(pDX, IDC_PROGRESS1, m_progress);
    DDX_Control(pDX, IDC_REPORT, m_report);
    DDX_Control(pDX, IDC_STATIC1, m_from);
    DDX_Control(pDX, IDC_STATIC2, m_to);
    fulfillThemeReqs();
}


BEGIN_MESSAGE_MAP(CSaveDlg, CMPCThemeResizableDialog)
    ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
    ON_MESSAGE(WM_GRAPHNOTIFY, OnGraphNotify)
    ON_WM_TIMER()
END_MESSAGE_MAP()


// CSaveDlg message handlers

BOOL CSaveDlg::OnInitDialog()
{
    CMPCThemeResizableDialog::OnInitDialog();

    // We can't use m_anim.Open(IDR_AVI_FILECOPY) since we want to load the AVI from the main executable
    m_anim.SendMessage(ACM_OPEN, (WPARAM)AfxGetInstanceHandle(), (LPARAM)IDR_AVI_FILECOPY);
    m_anim.Play(0, UINT(-1), UINT(-1));

    m_from.SetWindowText(m_in);
    m_to.SetWindowText(m_out);

    m_progress.SetRange(0, 100);
    CMPCThemeUtil::fulfillThemeReqs(&m_progress);

    if (FAILED(pGB.CoCreateInstance(CLSID_FilterGraph)) || !(pMC = pGB) || !(pME = pGB) || !(pMS = pGB)
            || FAILED(pME->SetNotifyWindow((OAHWND)m_hWnd, WM_GRAPHNOTIFY, 0x2B00B1E5))) {
        m_report.SetWindowText(_T("Error"));
        return FALSE;
    }

    HRESULT hr;

    CStringW fnw = m_in;
    CComPtr<IFileSourceFilter> pReader;

#if INTERNAL_SOURCEFILTER_CDDA
    if (!pReader && m_in.Mid(m_in.ReverseFind('.') + 1).MakeLower() == _T("cda")) {
        hr = S_OK;
        CComPtr<IUnknown> pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW CCDDAReader(nullptr, &hr);
        if (FAILED(hr) || !(pReader = pUnk) || FAILED(pReader->Load(fnw, nullptr))) {
            pReader.Release();
        }
    }
#endif

#if INTERNAL_SOURCEFILTER_CDXA
    if (!pReader) {
        hr = S_OK;
        CComPtr<IUnknown> pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW CCDXAReader(nullptr, &hr);
        if (FAILED(hr) || !(pReader = pUnk) || FAILED(pReader->Load(fnw, nullptr))) {
            pReader.Release();
        }
    }
#endif

#if INTERNAL_SOURCEFILTER_VTS
    if (!pReader /*&& ext == _T("ifo")*/) {
        hr = S_OK;
        CComPtr<IUnknown> pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW CVTSReader(nullptr, &hr);
        if (FAILED(hr) || !(pReader = pUnk) || FAILED(pReader->Load(fnw, nullptr))) {
            pReader.Release();
        } else {
            CPath pout(m_out);
            if (pout.RenameExtension(_T(".ifo"))) {
                CopyFile(m_in, pout, FALSE);
            } else {
                m_report.SetWindowText(_T("Path is too long"));
                return FALSE;
            }
        }
    }
#endif

#if INTERNAL_SOURCEFILTER_RFS
    if (!pReader) {
        hr = S_OK;
        CComPtr<IUnknown> pUnk = (IUnknown*)(INonDelegatingUnknown*)DEBUG_NEW CRARFileSource(nullptr, &hr);
        if (FAILED(hr) || !(pReader = pUnk) || FAILED(pReader->Load(fnw, nullptr))) {
            pReader.Release();
        }
    }
#endif

    if (!pReader) {
        CComPtr<IUnknown> pUnk;
        hr = pUnk.CoCreateInstance(CLSID_AsyncReader);
        if (FAILED(hr) || !(pReader = pUnk) || FAILED(pReader->Load(fnw, nullptr))) {
            pReader.Release();
        }
    }

    if (!pReader) {
        CComPtr<IUnknown> pUnk;
        hr = pUnk.CoCreateInstance(CLSID_URLReader);
        if (CComQIPtr<IBaseFilter> pSrc = pUnk) { // url reader has to be in the graph to load the file
            hr = pGB->AddFilter(pSrc, fnw);
            if (FAILED(hr) || !(pReader = pUnk) || FAILED(hr = pReader->Load(fnw, nullptr))) {
                pReader.Release();
                pGB->RemoveFilter(pSrc);
            }
        }
    }

    CComQIPtr<IBaseFilter> pSrc = pReader;
    if (FAILED(pGB->AddFilter(pSrc, _T("Source")))) {
        m_report.SetWindowText(_T("Sorry, can't save this file, press cancel"));
        return FALSE;
    }

    CComQIPtr<IBaseFilter> pMid = DEBUG_NEW CStreamDriveThruFilter(nullptr, &hr);
    if (FAILED(pGB->AddFilter(pMid, L"StreamDriveThru"))) {
        m_report.SetWindowText(_T("Error"));
        return FALSE;
    }

    CComQIPtr<IBaseFilter> pDst;
    pDst.CoCreateInstance(CLSID_FileWriter);
    CComQIPtr<IFileSinkFilter2> pFSF = pDst;
    pFSF->SetFileName(CStringW(m_out), nullptr);
    pFSF->SetMode(AM_FILE_OVERWRITE);
    if (FAILED(pGB->AddFilter(pDst, L"File Writer"))) {
        m_report.SetWindowText(_T("Error"));
        return FALSE;
    }

    hr = pGB->ConnectDirect(
             GetFirstPin((pSrc), PINDIR_OUTPUT),
             GetFirstPin((pMid), PINDIR_INPUT), nullptr);

    if (FAILED(hr)) {
        CString err;
        err.Format(_T("Error Connect pSrc / pMid: 0x%x"), hr);
        m_report.SetWindowText(err);
        return FALSE;
    }

    hr = pGB->ConnectDirect(
             GetFirstPin((pMid), PINDIR_OUTPUT),
             GetFirstPin((pDst), PINDIR_INPUT), nullptr);
    if (FAILED(hr)) {
        CString err;
        err.Format(_T("Error Connect pMid / pDst: 0x%x"), hr);
        m_report.SetWindowText(err);
        return FALSE;
    }

    pMS = pMid;

    pMC->Run();

    m_nIDTimerEvent = SetTimer(1, 500, nullptr);

    AddAnchor(IDC_PROGRESS1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_STATIC1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_STATIC2, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CSaveDlg::OnBnClickedCancel()
{
    if (pMC) {
        pMC->Stop();
    }

    OnCancel();
}

LRESULT CSaveDlg::OnGraphNotify(WPARAM wParam, LPARAM lParam)
{
    LONG evCode = 0;
    LONG_PTR evParam1 = 0, evParam2 = 0;
    while (pME && SUCCEEDED(pME->GetEvent(&evCode, &evParam1, &evParam2, 0))) {
        HRESULT hr = pME->FreeEventParams(evCode, evParam1, evParam2);
        UNREFERENCED_PARAMETER(hr);

        if (EC_COMPLETE == evCode) {
            EndDialog(IDOK);
        } else if (EC_ERRORABORT == evCode) {
            TRACE(_T("CSaveDlg::OnGraphNotify / EC_ERRORABORT, hr = %08x\n"), (HRESULT)evParam1);
            m_report.SetWindowText(_T("Copying unexpectedly terminated!"));
        }
    }

    return 0;
}

static unsigned int AdaptUnit(double& val, size_t unitsNb)
{
    unsigned int unit = 0;

    while (val > 1024 && unit < unitsNb) {
        val /= 1024;
        unit++;
    }

    return unit;
}

void CSaveDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nIDTimerEvent && pGB && pMS) {
        CString str;
        REFERENCE_TIME pos = 0, dur = 0;
        pMS->GetCurrentPosition(&pos);
        pMS->GetDuration(&dur);
        REFERENCE_TIME time = 0;
        CComQIPtr<IMediaSeeking>(pGB)->GetCurrentPosition(&time);
        REFERENCE_TIME speed = time > 0 ? pos * 10000000 / time : 0;
        static UINT sizeUnits[] = { IDS_SIZE_UNIT_K, IDS_SIZE_UNIT_M, IDS_SIZE_UNIT_G };
        static UINT speedUnits[] = { IDS_SPEED_UNIT_K, IDS_SPEED_UNIT_M, IDS_SPEED_UNIT_G };

        double dPos = pos / 1024.;
        unsigned int unitPos = AdaptUnit(dPos, _countof(sizeUnits));
        double dDur = dur / 1024.;
        unsigned int unitDur = AdaptUnit(dDur, _countof(sizeUnits));
        double dSpeed = speed / 1024.;
        unsigned int unitSpeed = AdaptUnit(dSpeed, _countof(speedUnits));

        str.Format(_T("%.2lf %s / %.2lf %s, %.2lf %s, %I64d s"),
                   dPos, ResStr(sizeUnits[unitPos]).GetString(), dDur, ResStr(sizeUnits[unitDur]).GetString(),
                   dSpeed, ResStr(speedUnits[unitSpeed]).GetString(), speed > 0 ? (dur - pos) / speed : 0);
        m_report.SetWindowText(str);

        m_progress.SetPos(dur > 0 ? (int)(100 * pos / dur) : 0);
    }

    CMPCThemeResizableDialog::OnTimer(nIDEvent);
}
