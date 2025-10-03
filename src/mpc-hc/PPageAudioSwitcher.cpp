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
#include <cmath>
#include "mplayerc.h"
#include "MainFrm.h"
#include "PPageAudioSwitcher.h"
#include "CMPCTheme.h"


// CPPageAudioSwitcher dialog

#pragma warning(push)
#pragma warning(disable: 4351) // new behavior: elements of array 'array' will be default initialized
IMPLEMENT_DYNAMIC(CPPageAudioSwitcher, CMPCThemePPageBase)
CPPageAudioSwitcher::CPPageAudioSwitcher(IFilterGraph* pFG)
#if 1
    : CMPCThemePPageBase(CPPageAudioSwitcher::IDD, CPPageAudioSwitcher::IDD)
#else
    : CMPCThemePPageBase(CPPageAudioSwitcher::IDD, IDS_AUDIOSWITCHER)
#endif
    , m_pSpeakerToChannelMap()
    , m_dwChannelMask(0)
    , m_fEnableAudioSwitcher(FALSE)
    , m_fAudioNormalize(FALSE)
    , m_nAudioMaxNormFactor(400)
    , m_fAudioNormalizeRecover(FALSE)
    , m_AudioBoostPos(0)
    , m_fCustomChannelMapping(FALSE)
    , m_nChannels(0)
    , m_tAudioTimeShift(0)
    , m_fAudioTimeShift(FALSE)
{
    CComQIPtr<IAudioSwitcherFilter> pASF = FindFilter(__uuidof(CAudioSwitcherFilter), pFG);

    if (pASF) {
        pASF->GetInputSpeakerConfig(&m_dwChannelMask);
        m_nChannels = pASF->GetNumberOfInputChannels();
    }
}
#pragma warning(pop)

CPPageAudioSwitcher::~CPPageAudioSwitcher()
{
}

void CPPageAudioSwitcher::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK5, m_fAudioNormalize);
    DDX_Text(pDX, IDC_EDIT3, m_nAudioMaxNormFactor);
    DDX_Control(pDX, IDC_SPIN3, m_AudioMaxNormFactorSpin);
    DDX_Check(pDX, IDC_CHECK6, m_fAudioNormalizeRecover);
    DDX_Slider(pDX, IDC_SLIDER1, m_AudioBoostPos);
    DDX_Control(pDX, IDC_SLIDER1, m_AudioBoostCtrl);
    DDX_Check(pDX, IDC_CHECK1, m_fCustomChannelMapping);
    DDX_Control(pDX, IDC_EDIT1, m_nChannelsCtrl);
    DDX_Text(pDX, IDC_EDIT1, m_nChannels);
    if (m_nChannels > AS_MAX_CHANNELS) {
        m_nChannels = AS_MAX_CHANNELS;
        SetDlgItemText(IDC_EDIT1, L"18");
    }
    DDX_Control(pDX, IDC_SPIN1, m_nChannelsSpinCtrl);
    DDX_Control(pDX, IDC_LIST1, m_list);
    DDX_Check(pDX, IDC_CHECK2, m_fEnableAudioSwitcher);
    DDX_Control(pDX, IDC_CHECK1, m_fCustomChannelMappingCtrl);
    DDX_Control(pDX, IDC_EDIT2, m_tAudioTimeShiftCtrl);
    DDX_Control(pDX, IDC_SPIN2, m_tAudioTimeShiftSpin);
    DDX_Text(pDX, IDC_EDIT2, m_tAudioTimeShift);
    DDX_Check(pDX, IDC_CHECK4, m_fAudioTimeShift);
    DDX_Control(pDX, IDC_CHECK4, m_fAudioTimeShiftCtrl);
}

BEGIN_MESSAGE_MAP(CPPageAudioSwitcher, CMPCThemePPageBase)
    ON_NOTIFY(NM_CLICK, IDC_LIST1, OnNMClickList1)
    ON_BN_CLICKED(IDC_CHECK1, OnClickCheck1)
    ON_WM_DRAWITEM()
    ON_EN_CHANGE(IDC_EDIT1, OnEnChangeEdit1)
    ON_UPDATE_COMMAND_UI(IDC_STATIC6, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_SLIDER1, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_CHECK5, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_STATIC4, OnUpdateNormalize)
    ON_UPDATE_COMMAND_UI(IDC_STATIC5, OnUpdateNormalize)
    ON_UPDATE_COMMAND_UI(IDC_EDIT3, OnUpdateNormalize)
    ON_UPDATE_COMMAND_UI(IDC_SPIN3, OnUpdateNormalize)
    ON_UPDATE_COMMAND_UI(IDC_CHECK6, OnUpdateNormalize)
    ON_UPDATE_COMMAND_UI(IDC_EDIT2, OnUpdateTimeShift)
    ON_UPDATE_COMMAND_UI(IDC_SPIN2, OnUpdateTimeShift)
    ON_UPDATE_COMMAND_UI(IDC_CHECK3, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_CHECK4, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_CHECK1, OnUpdateAudioSwitcher)
    ON_UPDATE_COMMAND_UI(IDC_EDIT1, OnUpdateChannelMapping)
    ON_UPDATE_COMMAND_UI(IDC_SPIN1, OnUpdateChannelMapping)
    ON_UPDATE_COMMAND_UI(IDC_LIST1, OnUpdateChannelMapping)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdateChannelMapping)
    ON_UPDATE_COMMAND_UI(IDC_STATIC2, OnUpdateChannelMapping)
    ON_UPDATE_COMMAND_UI(IDC_STATIC3, OnUpdateChannelMapping)
    ON_WM_HSCROLL()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()


void CPPageAudioSwitcher::SetChannelMappingSW(int v)
{
    GetDlgItem(IDC_EDIT1)->ShowWindow(v);
    GetDlgItem(IDC_SPIN1)->ShowWindow(v);
    GetDlgItem(IDC_LIST1)->ShowWindow(v);
    GetDlgItem(IDC_STATIC_CM1)->ShowWindow(v);
    GetDlgItem(IDC_STATIC_CM2)->ShowWindow(v);
    GetDlgItem(IDC_STATIC_CM3)->ShowWindow(v);
}

// CPPageAudioSwitcher message handlers

BOOL CPPageAudioSwitcher::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    m_fEnableAudioSwitcher = s.fEnableAudioSwitcher;
    m_fAudioNormalize = s.fAudioNormalize;
    m_nAudioMaxNormFactor = s.nAudioMaxNormFactor;
    m_AudioMaxNormFactorSpin.SetRange32(100, 1000);
    m_fAudioNormalizeRecover = s.fAudioNormalizeRecover;
    m_AudioBoostCtrl.SetRange(0, 300);
    m_AudioBoostCtrl.SetPageSize(10);
    m_AudioBoostPos = s.nAudioBoost;
    m_fAudioTimeShift = s.fAudioTimeShift;
    m_tAudioTimeShift = s.iAudioTimeShift;
    m_tAudioTimeShiftSpin.SetRange32(-1000 * 60 * 60 * 24, 1000 * 60 * 60 * 24);
    m_fCustomChannelMapping = s.fCustomChannelMapping;
    memcpy(m_pSpeakerToChannelMap, s.pSpeakerToChannelMap, sizeof(s.pSpeakerToChannelMap));

    m_nChannels = std::clamp(s.nSpeakerChannels, 1, AS_MAX_CHANNELS);
    m_nChannelsSpinCtrl.SetRange(1, AS_MAX_CHANNELS);

    m_list.setAdditionalStyles(0); //cleans up styles if necessary for mpc theme
    m_list.InsertColumn(0, _T(""), LVCFMT_LEFT, 100);
    m_list.InsertItem(0, _T(""));
    m_list.InsertItem(1, ResStr(IDS_FRONT_LEFT));
    m_list.InsertItem(2, ResStr(IDS_FRONT_RIGHT));
    m_list.InsertItem(3, ResStr(IDS_FRONT_CENTER));
    m_list.InsertItem(4, ResStr(IDS_LOW_FREQUENCY));
    m_list.InsertItem(5, ResStr(IDS_BACK_LEFT));
    m_list.InsertItem(6, ResStr(IDS_BACK_RIGHT));
    m_list.InsertItem(7, ResStr(IDS_FRONT_LEFT_OF_CENTER));
    m_list.InsertItem(8, ResStr(IDS_FRONT_RIGHT_OF_CENTER));
    m_list.InsertItem(9, ResStr(IDS_BACK_CENTER));
    m_list.InsertItem(10, ResStr(IDS_SIDE_LEFT));
    m_list.InsertItem(11, ResStr(IDS_SIDE_RIGHT));
    m_list.InsertItem(12, ResStr(IDS_TOP_CENTER));
    m_list.InsertItem(13, ResStr(IDS_TOP_FRONT_LEFT));
    m_list.InsertItem(14, ResStr(IDS_TOP_FRONT_CENTER));
    m_list.InsertItem(15, ResStr(IDS_TOP_FRONT_RIGHT));
    m_list.InsertItem(16, ResStr(IDS_TOP_BACK_LEFT));
    m_list.InsertItem(17, ResStr(IDS_TOP_BACK_CENTER));
    m_list.InsertItem(18, ResStr(IDS_TOP_BACK_RIGHT));
    m_list.SetColumnWidth(0, LVSCW_AUTOSIZE);

    for (int i = 1; i <= AS_MAX_CHANNELS; i++) {
        m_list.InsertColumn(i, _T(""), LVCFMT_CENTER, 16);
        CString n;
        n.Format(_T("%d"), i);
        m_list.SetItemText(0, i, n);
        //      m_list.SetColumnWidth(i, LVSCW_AUTOSIZE);
        //      m_list.SetColumnWidth(i, m_list.GetColumnWidth(i)*8/10);
    }

    EnableThemedDialogTooltips(this);
    m_tooltip.Create(this);
    m_tooltip.Activate(TRUE);

    CorrectComboBoxHeaderWidth(GetDlgItem(IDC_CHECK5));

    if (!m_fCustomChannelMapping) {
        SetChannelMappingSW(SW_HIDE);
    }

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageAudioSwitcher::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.fEnableAudioSwitcher = !!m_fEnableAudioSwitcher;
    s.fAudioNormalize = !!m_fAudioNormalize;
    if (m_nAudioMaxNormFactor > 1000) {
        m_nAudioMaxNormFactor = 1000;
    } else if (m_nAudioMaxNormFactor < 100) {
        m_nAudioMaxNormFactor = 100;
    }
    s.nAudioMaxNormFactor = m_nAudioMaxNormFactor;
    s.fAudioNormalizeRecover = !!m_fAudioNormalizeRecover;
    s.nAudioBoost = m_AudioBoostPos;
    s.fAudioTimeShift = !!m_fAudioTimeShift;
    s.iAudioTimeShift = m_tAudioTimeShift;
    s.fCustomChannelMapping = !!m_fCustomChannelMapping;
    memcpy(s.pSpeakerToChannelMap, m_pSpeakerToChannelMap, sizeof(m_pSpeakerToChannelMap));
    s.nSpeakerChannels = std::clamp(m_nChannels, 1, AS_MAX_CHANNELS);

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_AUDIO_SWITCHER);
    }

    return __super::OnApply();
}

void CPPageAudioSwitcher::OnClickCheck1()
{
    UpdateData();

    if (m_fCustomChannelMapping) {
        if (IDNO == AfxMessageBox(_T("WARNING: Channel mapping is not the same as channel mixing. Normal users should never enable this functionality.\n\nIf you want to do channel mixing (such as 5.1 -> stereo) you need to go here:\nInternal Filters > Audio Decoder > Mixing\n\nAre you really sure that you want to enable channel mapping?"), MB_ICONEXCLAMATION | MB_YESNO, 0)) {
            m_fCustomChannelMapping = false;
            UpdateData(FALSE);
            SetChannelMappingSW(SW_HIDE);
        } else {
            SetChannelMappingSW(SW_SHOW);
        }
    } else {
        SetChannelMappingSW(SW_HIDE);
    }
}

void CPPageAudioSwitcher::OnNMClickList1(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pNMHDR;

    if (lpnmlv->iItem > 0 && lpnmlv->iSubItem > 0 && lpnmlv->iSubItem <= m_nChannels) {
        UpdateData();
        m_pSpeakerToChannelMap[m_nChannels - 1][lpnmlv->iItem - 1] ^= 1 << (lpnmlv->iSubItem - 1);
        m_list.RedrawItems(lpnmlv->iItem, lpnmlv->iItem);
        SetModified();

        if (GetKeyState(VK_SHIFT) & 0x8000) {
            OnApply();
        }
    }

    *pResult = 0;
}

void CPPageAudioSwitcher::OnEnChangeEdit1()
{
    if (IsWindow(m_list.m_hWnd)) {
        UpdateData();
        m_list.Invalidate();
        SetModified();
    }
}

void CPPageAudioSwitcher::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl != IDC_LIST1) {
        return;
    }

    //  if (lpDrawItemStruct->itemID == 0)
    //      UpdateData();

    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

    pDC->SetBkMode(TRANSPARENT);

    COLORREF frameClr, textClr, textDisabledClr, textNAClr;
    COLORREF oldBkColor = pDC->GetBkColor();
    if (AppNeedsThemedControls()) {
        frameClr = CMPCTheme::AudioSwitcherGridColor;
        textClr = CMPCTheme::TextFGColor;
        textDisabledClr = CMPCTheme::ContentTextDisabledFGColorFade;
        textNAClr = CMPCTheme::ContentTextDisabledFGColorFade2;
        pDC->SetBkColor(CMPCTheme::ContentBGColor);
        CRect bgRect = lpDrawItemStruct->rcItem;
        pDC->FillSolidRect(bgRect, CMPCTheme::ContentBGColor);
    } else {
        frameClr = 0xe0e0e0;
        textClr = 0;
        textDisabledClr = 0xb0b0b0;
        textNAClr = 0xe0e0e0;
    }

    CPen p(PS_INSIDEFRAME, 1, frameClr);
    CPen* old = pDC->SelectObject(&p);

    pDC->MoveTo(lpDrawItemStruct->rcItem.left, lpDrawItemStruct->rcItem.bottom - 1);
    pDC->LineTo(lpDrawItemStruct->rcItem.right, lpDrawItemStruct->rcItem.bottom - 1);

    CHeaderCtrl* pHeader = m_list.GetHeaderCtrl();
    int nColumnCount = pHeader->GetItemCount();

    for (int i = 0; i < nColumnCount; i++) {
        CRect r, rb;
        m_list.GetSubItemRect(lpDrawItemStruct->itemID, i, LVIR_BOUNDS, rb);
        m_list.GetSubItemRect(lpDrawItemStruct->itemID, i, LVIR_LABEL, r);

        pDC->MoveTo(r.right - 1, r.top);
        pDC->LineTo(r.right - 1, r.bottom - 1);

        CSize s = pDC->GetTextExtent(m_list.GetItemText(lpDrawItemStruct->itemID, i));

        if (i == 0) {
            r.left = rb.left;

            if (lpDrawItemStruct->itemID == 0) {
                pDC->MoveTo(0, 0);
                pDC->LineTo(r.right, r.bottom - 1);
            } else {
                pDC->SetTextColor(m_list.IsWindowEnabled() ? textClr : textDisabledClr);
                pDC->TextOut(r.left + 1, (r.top + r.bottom - s.cy) / 2, m_list.GetItemText(lpDrawItemStruct->itemID, i));
            }
        } else {
            pDC->SetTextColor(i > m_nChannels ? textNAClr : (!m_list.IsWindowEnabled() ? textDisabledClr : textClr));

            if (lpDrawItemStruct->itemID == 0) {
                pDC->TextOut((r.left + r.right - s.cx) / 2, (r.top + r.bottom - s.cy) / 2, m_list.GetItemText(lpDrawItemStruct->itemID, i));
            } else {
                if (m_dwChannelMask & (1 << (lpDrawItemStruct->itemID - 1))) {
                    int nBitsSet = 0;

                    for (int j = 1; j <= (1 << (lpDrawItemStruct->itemID - 1)); j <<= 1) {
                        if (m_dwChannelMask & j) {
                            nBitsSet++;
                        }
                    }

                    if (nBitsSet == i) {
                        COLORREF tmp = pDC->GetTextColor();

                        pDC->SetTextColor(textNAClr);
                        CFont f;
                        f.CreatePointFont(MulDiv(100, 96, pDC->GetDeviceCaps(LOGPIXELSX)), _T("Marlett"));
                        CFont* old2 = pDC->SelectObject(&f);
                        UNREFERENCED_PARAMETER(old2);
                        s = pDC->GetTextExtent(_T("g"));
                        pDC->TextOut((r.left + r.right - s.cx) / 2, (r.top + r.bottom - s.cy) / 2, _T("g"));

                        pDC->SetTextColor(tmp);
                    }
                }

                if (m_pSpeakerToChannelMap[m_nChannels - 1][lpDrawItemStruct->itemID - 1] & (1 << (i - 1))) {
                    CFont f;
                    f.CreatePointFont(MulDiv(100, 96, pDC->GetDeviceCaps(LOGPIXELSX)), _T("Marlett"));
                    CFont* old2 = pDC->SelectObject(&f);
                    s = pDC->GetTextExtent(_T("a"));
                    pDC->TextOut((r.left + r.right - s.cx) / 2, (r.top + r.bottom - s.cy) / 2, _T("a"));
                    pDC->SelectObject(old2);
                }
            }
        }
    }

    pDC->SetBkColor(oldBkColor);
    pDC->SelectObject(old);
}

void CPPageAudioSwitcher::OnUpdateAudioSwitcher(CCmdUI* pCmdUI)
{
    //  UpdateData();
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2)/*m_fEnableAudioSwitcher*/);
}

void CPPageAudioSwitcher::OnUpdateNormalize(CCmdUI* pCmdUI)
{
    //  UpdateData();
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2)/*m_fEnableAudioSwitcher*/
                   && IsDlgButtonChecked(IDC_CHECK5)/*m_fNormalize*/);
}

void CPPageAudioSwitcher::OnUpdateTimeShift(CCmdUI* pCmdUI)
{
    //  UpdateData();
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2)/*m_fEnableAudioSwitcher*/
                   && IsDlgButtonChecked(IDC_CHECK4)/*m_fAudioTimeShift)*/);
}

void CPPageAudioSwitcher::OnUpdateChannelMapping(CCmdUI* pCmdUI)
{
    //  UpdateData();
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK2)/*m_fEnableAudioSwitcher*/
                   && IsDlgButtonChecked(IDC_CHECK1)/*m_fCustomChannelMapping*/);
}

void CPPageAudioSwitcher::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar == nullptr) {
        return;
    }
    if (*pScrollBar == m_AudioBoostCtrl) {
        CAppSettings& s = AfxGetAppSettings();
        int cur = m_AudioBoostPos;
        UpdateData();
        if (!s.bAudioBoostWarned && m_AudioBoostPos > cur && m_AudioBoostPos > 20) {
            s.bAudioBoostWarned = true;
            AfxMessageBox(_T("WARNING: Boosting audio volume can in some situations have a negative effect on audio quality due to overflow.\n\nThere are two better and safer methods for increasing volume:\n1) Enabling mixing in the internal audio decoder settings.\nInternal Filters > Audio Decoder > Mixing\nThis is recommended if you have stereo speakers or headphones.\nTo increase loudness of voices, set center mix level to 1.0 in the mixing settings.\n2) Enable Normalize"), MB_ICONEXCLAMATION | MB_OK, 0);
        }
        ((CMainFrame*)GetParentFrame())->SetVolumeBoost(m_AudioBoostPos); // nice shortcut...
    }
    RedrawDialogTooltipIfVisible(); //if the scroll is caused by a wheel or arrows, the default tooltip may be active due to hover, in which case, we want to update

    SetModified();

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CPPageAudioSwitcher::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    UINT_PTR nID = pNMHDR->idFrom;
    if (pTTT->uFlags & TTF_IDISHWND) {
        nID = ::GetDlgCtrlID((HWND)nID);
    }

    bool bRet = false;

    static CString strTipText;

    switch (nID) {
        case IDC_SLIDER1:
            strTipText.Format(IDS_BOOST, m_AudioBoostCtrl.GetPos());
            bRet = true;
            break;
        case IDC_EDIT2:
        case IDC_SPIN2:
            strTipText.LoadString(IDS_TIME_SHIFT_TOOLTIP);
            bRet = true;
            break;
    }

    if (bRet) {
        pTTT->lpszText = (LPWSTR)(LPCWSTR)strTipText;
        PlaceThemedDialogTooltip(nID);
    }

    return bRet;
}

void CPPageAudioSwitcher::OnCancel()
{
    const CAppSettings& s = AfxGetAppSettings();

    if ((UINT)m_AudioBoostPos != s.nAudioBoost) {
        ((CMainFrame*)GetParentFrame())->SetVolumeBoost(s.nAudioBoost);
    }

    __super::OnCancel();
}
