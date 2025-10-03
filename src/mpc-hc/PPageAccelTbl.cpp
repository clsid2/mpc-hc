/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include "PPageAccelTbl.h"
#include "AppSettings.h"

#define DUP_KEY      (1<<12)
#define DUP_APPCMD   (1<<13)
#define DUP_RMCMD    (1<<14)
#define MASK_DUP     (DUP_KEY|DUP_APPCMD|DUP_RMCMD)

struct APP_COMMAND {
    UINT    appcmd;
    LPCTSTR cmdname;
};

static constexpr APP_COMMAND g_CommandList[] = {
    {0,                                 _T("")},
    {APPCOMMAND_MEDIA_PLAY_PAUSE,       _T("MEDIA_PLAY_PAUSE")},
    {APPCOMMAND_MEDIA_PLAY,             _T("MEDIA_PLAY")},
    {APPCOMMAND_MEDIA_PAUSE,            _T("MEDIA_PAUSE")},
    {APPCOMMAND_MEDIA_STOP,             _T("MEDIA_STOP")},
    {APPCOMMAND_MEDIA_NEXTTRACK,        _T("MEDIA_NEXTTRACK")},
    {APPCOMMAND_MEDIA_PREVIOUSTRACK,    _T("MEDIA_PREVIOUSTRACK")},
    {APPCOMMAND_MEDIA_FAST_FORWARD,     _T("MEDIA_FAST_FORWARD")},
    {APPCOMMAND_MEDIA_REWIND,           _T("MEDIA_REWIND")},
    {APPCOMMAND_MEDIA_CHANNEL_UP,       _T("MEDIA_CHANNEL_UP")},
    {APPCOMMAND_MEDIA_CHANNEL_DOWN,     _T("MEDIA_CHANNEL_DOWN")},
    {APPCOMMAND_MEDIA_RECORD,           _T("MEDIA_RECORD")},
    {APPCOMMAND_VOLUME_DOWN,            _T("VOLUME_DOWN")},
    {APPCOMMAND_VOLUME_UP,              _T("VOLUME_UP")},
    {APPCOMMAND_VOLUME_MUTE,            _T("VOLUME_MUTE")},
    {APPCOMMAND_LAUNCH_MEDIA_SELECT,    _T("LAUNCH_MEDIA_SELECT")},
    /*
    {APPCOMMAND_BROWSER_BACKWARD,       _T("BROWSER_BACKWARD")},
    {APPCOMMAND_BROWSER_FORWARD,        _T("BROWSER_FORWARD")},
    {APPCOMMAND_BROWSER_REFRESH,        _T("BROWSER_REFRESH")},
    {APPCOMMAND_BROWSER_STOP,           _T("BROWSER_STOP")},
    {APPCOMMAND_BROWSER_SEARCH,         _T("BROWSER_SEARCH")},
    {APPCOMMAND_BROWSER_FAVORITES,      _T("BROWSER_FAVORITES")},
    {APPCOMMAND_BROWSER_HOME,           _T("BROWSER_HOME")},
    */
    {APPCOMMAND_LAUNCH_APP1,            _T("LAUNCH_APP1")},
    {APPCOMMAND_LAUNCH_APP2,            _T("LAUNCH_APP2")},
    {APPCOMMAND_OPEN,                   _T("OPEN")},
    {APPCOMMAND_CLOSE,                  _T("CLOSE")},
    {APPCOMMAND_DELETE,                 _T("DELETE")},
    {MCE_DETAILS,                       _T("MCE_DETAILS")},
    {MCE_GUIDE,                         _T("MCE_GUIDE")},
    {MCE_TVJUMP,                        _T("MCE_TVJUMP")},
    {MCE_STANDBY,                       _T("MCE_STANDBY")},
    {MCE_OEM1,                          _T("MCE_OEM1")},
    {MCE_OEM2,                          _T("MCE_OEM2")},
    {MCE_MYTV,                          _T("MCE_MYTV")},
    {MCE_MYVIDEOS,                      _T("MCE_MYVIDEOS")},
    {MCE_MYPICTURES,                    _T("MCE_MYPICTURES")},
    {MCE_MYMUSIC,                       _T("MCE_MYMUSIC")},
    {MCE_RECORDEDTV,                    _T("MCE_RECORDEDTV")},
    {MCE_DVDANGLE,                      _T("MCE_DVDANGLE")},
    {MCE_DVDAUDIO,                      _T("MCE_DVDAUDIO")},
    {MCE_DVDMENU,                       _T("MCE_DVDMENU")},
    {MCE_DVDSUBTITLE,                   _T("MCE_DVDSUBTITLE")},
    {MCE_RED,                           _T("MCE_RED")},
    {MCE_GREEN,                         _T("MCE_GREEN")},
    {MCE_YELLOW,                        _T("MCE_YELLOW")},
    {MCE_BLUE,                          _T("MCE_BLUE")},
    {MCE_MEDIA_NEXTTRACK,               _T("MCE_MEDIA_NEXTTRACK")},
    {MCE_MEDIA_PREVIOUSTRACK,           _T("MCE_MEDIA_PREVIOUSTRACK")}
};

// CPPageAccelTbl dialog

IMPLEMENT_DYNAMIC(CPPageAccelTbl, CMPCThemePPageBase)
CPPageAccelTbl::CPPageAccelTbl()
    : CMPCThemePPageBase(CPPageAccelTbl::IDD, CPPageAccelTbl::IDD)
    , m_counter(0)
    , m_list(0)
    , m_fWinLirc(FALSE)
    , m_WinLircLink(_T("http://winlirc.sourceforge.net/"))
    , m_fUIce(FALSE)
    , m_UIceLink(L"https://web.archive.org/web/20160609195532/http://www.mediatexx.com/") // home site no longer works
    , m_nStatusTimerID(0)
    , filterTimerID(0)
    , sortDirection(HDF_SORTUP)
    , m_fGlobalMedia(FALSE)
{
}

CPPageAccelTbl::~CPPageAccelTbl()
{
}

BOOL CPPageAccelTbl::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN
            && (pMsg->hwnd == m_WinLircEdit.m_hWnd || pMsg->hwnd == m_UIceEdit.m_hWnd)) {
        OnApply();
        return TRUE;
    }

    return __super::PreTranslateMessage(pMsg);
}

void CPPageAccelTbl::UpdateKeyDupFlags()
{
    for (int row = 0; row < m_list.GetItemCount(); row++) {
        auto itemData = (ITEMDATA*)m_list.GetItemData(row);
        const wmcmd& wc = m_wmcmds.GetAt(itemData->index);

        itemData->flag &= ~DUP_KEY;

        if (wc.key) {
            POSITION pos = m_wmcmds.GetHeadPosition();
            for (; pos; m_wmcmds.GetNext(pos)) {
                if (itemData->index == pos) { continue; }

                if (wc.key == m_wmcmds.GetAt(pos).key && (wc.fVirt & (FCONTROL | FALT | FSHIFT)) == (m_wmcmds.GetAt(pos).fVirt & (FCONTROL | FALT | FSHIFT))) {
                    itemData->flag |= DUP_KEY;
                    break;
                }
            }
        }
    }
}

void CPPageAccelTbl::UpdateAppcmdDupFlags()
{
    for (int row = 0; row < m_list.GetItemCount(); row++) {
        auto itemData = (ITEMDATA*)m_list.GetItemData(row);
        const wmcmd& wc = m_wmcmds.GetAt(itemData->index);

        itemData->flag &= ~DUP_APPCMD;

        if (wc.appcmd) {
            POSITION pos = m_wmcmds.GetHeadPosition();
            for (; pos; m_wmcmds.GetNext(pos)) {
                if (itemData->index == pos) { continue; }

                if (wc.appcmd == m_wmcmds.GetAt(pos).appcmd) {
                    itemData->flag |= DUP_APPCMD;
                    break;
                }
            }
        }
    }
}

void CPPageAccelTbl::UpdateRmcmdDupFlags()
{
    for (int row = 0; row < m_list.GetItemCount(); row++) {
        auto itemData = (ITEMDATA*)m_list.GetItemData(row);
        const wmcmd& wc = m_wmcmds.GetAt(itemData->index);

        itemData->flag &= ~DUP_RMCMD;

        if (wc.rmcmd.GetLength()) {
            POSITION pos = m_wmcmds.GetHeadPosition();
            for (; pos; m_wmcmds.GetNext(pos)) {
                if (itemData->index == pos) { continue; }

                if (wc.rmcmd.CompareNoCase(m_wmcmds.GetAt(pos).rmcmd) == 0) {
                    itemData->flag |= DUP_RMCMD;
                    break;
                }
            }
        }
    }
}

void CPPageAccelTbl::UpdateAllDupFlags()
{
    UpdateKeyDupFlags();
    UpdateAppcmdDupFlags();
    UpdateRmcmdDupFlags();
}

void CPPageAccelTbl::SetupList(bool allowResize)
{
    for (int row = 0; row < m_list.GetItemCount(); row++) {
        wmcmd& wc = m_wmcmds.GetAt(((ITEMDATA*)m_list.GetItemData(row))->index);

        CString hotkey;
        HotkeyModToString(wc.key, wc.fVirt, hotkey);
        m_list.SetItemText(row, COL_KEY, hotkey);

        CString id;
        id.Format(_T("%u"), wc.cmd);
        m_list.SetItemText(row, COL_ID, id);

        m_list.SetItemText(row, COL_APPCMD, MakeAppCommandLabel(wc.appcmd));

        m_list.SetItemText(row, COL_RMCMD, CString(wc.rmcmd));

        CString repcnt;
        repcnt.Format(_T("%d"), wc.rmrepcnt);
        m_list.SetItemText(row, COL_RMREPCNT, repcnt);
    }

    UpdateAllDupFlags();

    if (allowResize) {
        for (int nCol = COL_CMD; nCol <= COL_RMREPCNT; nCol++) {
            m_list.SetColumnWidth(nCol, LVSCW_AUTOSIZE);
            int contentSize = m_list.GetColumnWidth(nCol);
            m_list.SetColumnWidth(nCol, LVSCW_AUTOSIZE_USEHEADER);
            if (contentSize > m_list.GetColumnWidth(nCol)) {
                m_list.SetColumnWidth(nCol, LVSCW_AUTOSIZE);
            }
        }
        for (int nCol = COL_CMD; nCol <= COL_RMREPCNT; nCol++) {
            int contentSize = m_list.GetColumnWidth(nCol);
            m_list.SetColumnWidth(nCol, contentSize);
        }
    }
}

CString CPPageAccelTbl::MakeAccelModLabel(BYTE fVirt)
{
    CString str;
    if (fVirt & FCONTROL) {
        if (!str.IsEmpty()) {
            str += _T(" + ");
        }
        str += _T("Ctrl");
    }
    if (fVirt & FALT) {
        if (!str.IsEmpty()) {
            str += _T(" + ");
        }
        str += _T("Alt");
    }
    if (fVirt & FSHIFT) {
        if (!str.IsEmpty()) {
            str += _T(" + ");
        }
        str += _T("Shift");
    }
    if (str.IsEmpty()) {
        str.LoadString(IDS_AG_NONE);
    }
    return str;
}

CString CPPageAccelTbl::MakeAccelShortcutLabel(UINT id)
{
    CList<wmcmd>& wmcmds = AfxGetAppSettings().wmcmds;
    POSITION pos = wmcmds.GetHeadPosition();
    while (pos) {
        ACCEL& a = wmcmds.GetNext(pos);
        if (a.cmd == id) {
            return (MakeAccelShortcutLabel(a));
        }
    }

    return _T("");
}

CString CPPageAccelTbl::MakeAccelShortcutLabel(const ACCEL& a)
{
    if (!a.key) {
        return _T("");
    }

    // Reference page for Virtual-Key Codes: http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.100%29.aspx
    CString str;

    switch (a.key) {
        case VK_LBUTTON:
            str = _T("LBtn");
            break;
        case VK_RBUTTON:
            str = _T("RBtn");
            break;
        case VK_CANCEL:
            str = _T("Cancel");
            break;
        case VK_MBUTTON:
            str = _T("MBtn");
            break;
        case VK_XBUTTON1:
            str = _T("X1Btn");
            break;
        case VK_XBUTTON2:
            str = _T("X2Btn");
            break;
        case VK_BACK:
            str = _T("Back");
            break;
        case VK_TAB:
            str = _T("Tab");
            break;
        case VK_CLEAR:
            str = _T("Clear");
            break;
        case VK_RETURN:
            str = _T("Enter");
            break;
        case VK_SHIFT:
            str = _T("Shift");
            break;
        case VK_CONTROL:
            str = _T("Ctrl");
            break;
        case VK_MENU:
            str = _T("Alt");
            break;
        case VK_PAUSE:
            str = _T("Pause");
            break;
        case VK_CAPITAL:
            str = _T("Capital");
            break;
        //  case VK_KANA: str = _T("Kana"); break;
        //  case VK_HANGEUL: str = _T("Hangeul"); break;
        case VK_HANGUL:
            str = _T("Hangul");
            break;
        case VK_JUNJA:
            str = _T("Junja");
            break;
        case VK_FINAL:
            str = _T("Final");
            break;
        //  case VK_HANJA: str = _T("Hanja"); break;
        case VK_KANJI:
            str = _T("Kanji");
            break;
        case VK_ESCAPE:
            str = _T("Escape");
            break;
        case VK_CONVERT:
            str = _T("Convert");
            break;
        case VK_NONCONVERT:
            str = _T("Non Convert");
            break;
        case VK_ACCEPT:
            str = _T("Accept");
            break;
        case VK_MODECHANGE:
            str = _T("Mode Change");
            break;
        case VK_SPACE:
            str = _T("Space");
            break;
        case VK_PRIOR:
            str = _T("PgUp");
            break;
        case VK_NEXT:
            str = _T("PgDn");
            break;
        case VK_END:
            str = _T("End");
            break;
        case VK_HOME:
            str = _T("Home");
            break;
        case VK_LEFT:
            str = _T("Left");
            break;
        case VK_UP:
            str = _T("Up");
            break;
        case VK_RIGHT:
            str = _T("Right");
            break;
        case VK_DOWN:
            str = _T("Down");
            break;
        case VK_SELECT:
            str = _T("Select");
            break;
        case VK_PRINT:
            str = _T("Print");
            break;
        case VK_EXECUTE:
            str = _T("Execute");
            break;
        case VK_SNAPSHOT:
            str = _T("Snapshot");
            break;
        case VK_INSERT:
            str = _T("Insert");
            break;
        case VK_DELETE:
            str = _T("Delete");
            break;
        case VK_HELP:
            str = _T("Help");
            break;
        case VK_LWIN:
            str = _T("LWin");
            break;
        case VK_RWIN:
            str = _T("RWin");
            break;
        case VK_APPS:
            str = _T("Apps");
            break;
        case VK_SLEEP:
            str = _T("Sleep");
            break;
        case VK_NUMPAD0:
            str = _T("Numpad 0");
            break;
        case VK_NUMPAD1:
            str = _T("Numpad 1");
            break;
        case VK_NUMPAD2:
            str = _T("Numpad 2");
            break;
        case VK_NUMPAD3:
            str = _T("Numpad 3");
            break;
        case VK_NUMPAD4:
            str = _T("Numpad 4");
            break;
        case VK_NUMPAD5:
            str = _T("Numpad 5");
            break;
        case VK_NUMPAD6:
            str = _T("Numpad 6");
            break;
        case VK_NUMPAD7:
            str = _T("Numpad 7");
            break;
        case VK_NUMPAD8:
            str = _T("Numpad 8");
            break;
        case VK_NUMPAD9:
            str = _T("Numpad 9");
            break;
        case VK_MULTIPLY:
            str = _T("Multiply");
            break;
        case VK_ADD:
            str = _T("Add");
            break;
        case VK_SEPARATOR:
            str = _T("Separator");
            break;
        case VK_SUBTRACT:
            str = _T("Subtract");
            break;
        case VK_DECIMAL:
            str = _T("Decimal");
            break;
        case VK_DIVIDE:
            str = _T("Divide");
            break;
        case VK_F1:
            str = _T("F1");
            break;
        case VK_F2:
            str = _T("F2");
            break;
        case VK_F3:
            str = _T("F3");
            break;
        case VK_F4:
            str = _T("F4");
            break;
        case VK_F5:
            str = _T("F5");
            break;
        case VK_F6:
            str = _T("F6");
            break;
        case VK_F7:
            str = _T("F7");
            break;
        case VK_F8:
            str = _T("F8");
            break;
        case VK_F9:
            str = _T("F9");
            break;
        case VK_F10:
            str = _T("F10");
            break;
        case VK_F11:
            str = _T("F11");
            break;
        case VK_F12:
            str = _T("F12");
            break;
        case VK_F13:
            str = _T("F13");
            break;
        case VK_F14:
            str = _T("F14");
            break;
        case VK_F15:
            str = _T("F15");
            break;
        case VK_F16:
            str = _T("F16");
            break;
        case VK_F17:
            str = _T("F17");
            break;
        case VK_F18:
            str = _T("F18");
            break;
        case VK_F19:
            str = _T("F19");
            break;
        case VK_F20:
            str = _T("F20");
            break;
        case VK_F21:
            str = _T("F21");
            break;
        case VK_F22:
            str = _T("F22");
            break;
        case VK_F23:
            str = _T("F23");
            break;
        case VK_F24:
            str = _T("F24");
            break;
        case VK_NUMLOCK:
            str = _T("Numlock");
            break;
        case VK_SCROLL:
            str = _T("Scroll");
            break;
        //  case VK_OEM_NEC_EQUAL: str = _T("OEM NEC Equal"); break;
        case VK_OEM_FJ_JISHO:
            str = _T("OEM FJ Jisho");
            break;
        case VK_OEM_FJ_MASSHOU:
            str = _T("OEM FJ Msshou");
            break;
        case VK_OEM_FJ_TOUROKU:
            str = _T("OEM FJ Touroku");
            break;
        case VK_OEM_FJ_LOYA:
            str = _T("OEM FJ Loya");
            break;
        case VK_OEM_FJ_ROYA:
            str = _T("OEM FJ Roya");
            break;
        case VK_LSHIFT:
            str = _T("LShift");
            break;
        case VK_RSHIFT:
            str = _T("RShift");
            break;
        case VK_LCONTROL:
            str = _T("LCtrl");
            break;
        case VK_RCONTROL:
            str = _T("RCtrl");
            break;
        case VK_LMENU:
            str = _T("LAlt");
            break;
        case VK_RMENU:
            str = _T("RAlt");
            break;
        case VK_BROWSER_BACK:
            str = _T("Browser Back");
            break;
        case VK_BROWSER_FORWARD:
            str = _T("Browser Forward");
            break;
        case VK_BROWSER_REFRESH:
            str = _T("Browser Refresh");
            break;
        case VK_BROWSER_STOP:
            str = _T("Browser Stop");
            break;
        case VK_BROWSER_SEARCH:
            str = _T("Browser Search");
            break;
        case VK_BROWSER_FAVORITES:
            str = _T("Browser Favorites");
            break;
        case VK_BROWSER_HOME:
            str = _T("Browser Home");
            break;
        case VK_VOLUME_MUTE:
            str = _T("Volume Mute");
            break;
        case VK_VOLUME_DOWN:
            str = _T("Volume Down");
            break;
        case VK_VOLUME_UP:
            str = _T("Volume Up");
            break;
        case VK_MEDIA_NEXT_TRACK:
            str = _T("Media Next Track");
            break;
        case VK_MEDIA_PREV_TRACK:
            str = _T("Media Prev Track");
            break;
        case VK_MEDIA_STOP:
            str = _T("Media Stop");
            break;
        case VK_MEDIA_PLAY_PAUSE:
            str = _T("Media Play/Pause");
            break;
        case VK_LAUNCH_MAIL:
            str = _T("Launch Mail");
            break;
        case VK_LAUNCH_MEDIA_SELECT:
            str = _T("Launch Media Select");
            break;
        case VK_LAUNCH_APP1:
            str = _T("Launch App1");
            break;
        case VK_LAUNCH_APP2:
            str = _T("Launch App2");
            break;
        case VK_OEM_1:
            str = _T("OEM 1");
            break;
        case VK_OEM_PLUS:
            str = _T("Plus");
            break;
        case VK_OEM_COMMA:
            str = _T("Comma");
            break;
        case VK_OEM_MINUS:
            str = _T("Minus");
            break;
        case VK_OEM_PERIOD:
            str = _T("Period");
            break;
        case VK_OEM_2:
            str = _T("OEM 2");
            break;
        case VK_OEM_3:
            str = _T("`");
            break;
        case VK_OEM_4:
            str = _T("[");
            break;
        case VK_OEM_5:
            str = _T("OEM 5");
            break;
        case VK_OEM_6:
            str = _T("]");
            break;
        case VK_OEM_7:
            str = _T("OEM 7");
            break;
        case VK_OEM_8:
            str = _T("OEM 8");
            break;
        case VK_OEM_AX:
            str = _T("OEM AX");
            break;
        case VK_OEM_102:
            str = _T("OEM 102");
            break;
        case VK_ICO_HELP:
            str = _T("ICO Help");
            break;
        case VK_ICO_00:
            str = _T("ICO 00");
            break;
        case VK_PROCESSKEY:
            str = _T("Process Key");
            break;
        case VK_ICO_CLEAR:
            str = _T("ICO Clear");
            break;
        case VK_PACKET:
            str = _T("Packet");
            break;
        case VK_OEM_RESET:
            str = _T("OEM Reset");
            break;
        case VK_OEM_JUMP:
            str = _T("OEM Jump");
            break;
        case VK_OEM_PA1:
            str = _T("OEM PA1");
            break;
        case VK_OEM_PA2:
            str = _T("OEM PA2");
            break;
        case VK_OEM_PA3:
            str = _T("OEM PA3");
            break;
        case VK_OEM_WSCTRL:
            str = _T("OEM WSCtrl");
            break;
        case VK_OEM_CUSEL:
            str = _T("OEM CUSEL");
            break;
        case VK_OEM_ATTN:
            str = _T("OEM ATTN");
            break;
        case VK_OEM_FINISH:
            str = _T("OEM Finish");
            break;
        case VK_OEM_COPY:
            str = _T("OEM Copy");
            break;
        case VK_OEM_AUTO:
            str = _T("OEM Auto");
            break;
        case VK_OEM_ENLW:
            str = _T("OEM ENLW");
            break;
        case VK_OEM_BACKTAB:
            str = _T("OEM Backtab");
            break;
        case VK_ATTN:
            str = _T("ATTN");
            break;
        case VK_CRSEL:
            str = _T("CRSEL");
            break;
        case VK_EXSEL:
            str = _T("EXSEL");
            break;
        case VK_EREOF:
            str = _T("EREOF");
            break;
        case VK_PLAY:
            str = _T("Play");
            break;
        case VK_ZOOM:
            str = _T("Zoom");
            break;
        case VK_NONAME:
            str = _T("Noname");
            break;
        case VK_PA1:
            str = _T("PA1");
            break;
        case VK_OEM_CLEAR:
            str = _T("OEM Clear");
            break;
        case 0x07:
        case 0x0E:
        case 0x0F:
        case 0x16:
        case 0x1A:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
        case 0x40:
            str.Format(_T("Undefined (0x%02x)"), (TCHAR)a.key);
            break;
        case 0x0A:
        case 0x0B:
        case 0x5E:
        case 0xB8:
        case 0xB9:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD6:
        case 0xD7:
        case 0xE0:
            str.Format(_T("Reserved (0x%02x)"), (TCHAR)a.key);
            break;
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xE8:
            str.Format(_T("Unassigned (0x%02x)"), (TCHAR)a.key);
            break;
        case 0xFF:
            str = _T("Multimedia keys");
            break;
        default:
            str.Format(_T("%c"), (TCHAR)a.key);
            break;
    }

    if (a.fVirt & (FCONTROL | FALT | FSHIFT)) {
        str = MakeAccelModLabel(a.fVirt) + _T(" + ") + str;
    }

    str.Replace(_T(" + "), _T("+"));

    return str;
}

CString CPPageAccelTbl::MakeAppCommandLabel(UINT id)
{
    for (int i = 0; i < _countof(g_CommandList); i++) {
        if (g_CommandList[i].appcmd == id) {
            return CString(g_CommandList[i].cmdname);
        }
    }
    return id == 0 ? _T("") : _T("Invalid");
}

void CPPageAccelTbl::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT1, m_WinLircAddr);
    DDX_Control(pDX, IDC_EDIT1, m_WinLircEdit);
    DDX_Control(pDX, IDC_STATICLINK, m_WinLircLink);
    DDX_Check(pDX, IDC_CHECK1, m_fWinLirc);
    DDX_Text(pDX, IDC_EDIT2, m_UIceAddr);
    DDX_Control(pDX, IDC_EDIT2, m_UIceEdit);
    DDX_Control(pDX, IDC_EDIT3, filterEdit);
    DDX_Control(pDX, IDC_STATICLINK2, m_UIceLink);
    DDX_Check(pDX, IDC_CHECK9, m_fUIce);
    DDX_Check(pDX, IDC_CHECK2, m_fGlobalMedia);
}

BEGIN_MESSAGE_MAP(CPPageAccelTbl, CPPageBase)
    ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_LIST1, OnBeginListLabelEdit)
    ON_NOTIFY(LVN_DOLABELEDIT, IDC_LIST1, OnDoListLabelEdit)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST1, OnEndListLabelEdit)
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_LIST1, OnListColumnClick)
    ON_EN_CHANGE(IDC_EDIT3, OnChangeFilterEdit)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedSelectAll)
    ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedReset)
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_NOTIFY(NM_CUSTOMDRAW, IDC_LIST1, OnCustomdrawList)
END_MESSAGE_MAP()

// CPPageAccelTbl message handlers

static WNDPROC OldControlProc;

static LRESULT CALLBACK ControlProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN) {
        if ((LOWORD(wParam) == 'A' || LOWORD(wParam) == 'a') && (GetKeyState(VK_CONTROL) < 0)) {
            CPlayerListCtrl* pList = (CPlayerListCtrl*)CWnd::FromHandle(control);

            for (int i = 0, j = pList->GetItemCount(); i < j; i++) {
                pList->SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
            }

            return 0;
        }
    }

    return CallWindowProc(OldControlProc, control, message, wParam, lParam); // call control's own windowproc
}

BOOL CPPageAccelTbl::OnInitDialog()
{
    __super::OnInitDialog();

    CAppSettings& s = AfxGetAppSettings();

    m_wmcmds.RemoveAll();
    m_wmcmds.AddTail(&s.wmcmds);
    m_fWinLirc = s.fWinLirc;
    m_WinLircAddr = s.strWinLircAddr;
    m_fUIce = s.fUIce;
    m_UIceAddr = s.strUIceAddr;
    m_fGlobalMedia = s.fGlobalMedia;

    CString text;
    text.Format(IDS_STRING_COLON, _T("WinLIRC"));
    m_WinLircLink.SetWindowText(text);
    text.Format(IDS_STRING_COLON, _T("uICE"));
    m_UIceLink.SetWindowText(text);

    UpdateData(FALSE);

    CRect r;
    GetDlgItem(IDC_PLACEHOLDER)->GetWindowRect(r);
    ScreenToClient(r);

    m_list.CreateEx(
        WS_EX_CLIENTEDGE,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |  WS_TABSTOP | LVS_REPORT | LVS_AUTOARRANGE | LVS_SHOWSELALWAYS,
        r, this, IDC_LIST1);

    //m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES );
    m_list.setAdditionalStyles(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    m_list.setAdditionalStyles(WS_CLIPCHILDREN, false);
    m_list.setColorInterface(this);

    //this list was created dynamically but lives in a dialog.  if we don't inherit the parent font,
    //it will be scaled by text zoom settings, which looks bad in an unscaled dialog
    CFont* curDialogFont = GetFont();
    if (curDialogFont && curDialogFont->m_hObject) {
        m_list.SetFont(curDialogFont);
    }

    for (int i = 0, j = m_list.GetHeaderCtrl()->GetItemCount(); i < j; i++) {
        m_list.DeleteColumn(0);
    }
    m_list.InsertColumn(COL_CMD, ResStr(IDS_AG_COMMAND), LVCFMT_LEFT, 80);
    m_list.InsertColumn(COL_KEY, ResStr(IDS_AG_KEY), LVCFMT_LEFT, 80);
    m_list.InsertColumn(COL_ID, _T("ID"), LVCFMT_LEFT, 40);
    m_list.InsertColumn(COL_APPCMD, ResStr(IDS_AG_APP_COMMAND), LVCFMT_LEFT, 120);
    m_list.InsertColumn(COL_RMCMD, _T("RemoteCmd"), LVCFMT_LEFT, 80);
    m_list.InsertColumn(COL_RMREPCNT, _T("RepCnt"), LVCFMT_CENTER, 60);

    POSITION pos = m_wmcmds.GetHeadPosition();
    for (; pos; m_wmcmds.GetNext(pos)) {
        int row = m_list.InsertItem(m_list.GetItemCount(), m_wmcmds.GetAt(pos).GetName(), COL_CMD);
        auto itemData = std::make_unique<ITEMDATA>();
        itemData->index = pos;
        m_list.SetItemData(row, (DWORD_PTR)itemData.get());
        m_pItemsData.push_back(std::move(itemData));
    }

    SetupList();

    m_list.SetColumnWidth(COL_CMD, LVSCW_AUTOSIZE);
    m_list.SetColumnWidth(COL_KEY, LVSCW_AUTOSIZE);
    m_list.SetColumnWidth(COL_ID, LVSCW_AUTOSIZE_USEHEADER);

    // subclass the keylist control
    OldControlProc = (WNDPROC)SetWindowLongPtr(m_list.m_hWnd, GWLP_WNDPROC, (LONG_PTR)ControlProc);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageAccelTbl::OnApply()
{
    AfxGetMyApp()->UnregisterHotkeys();
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.wmcmds.RemoveAll();
    s.wmcmds.AddTail(&m_wmcmds);

    if (s.hAccel) {
        DestroyAcceleratorTable(s.hAccel);
    }

    CAtlArray<ACCEL> pAccel;
    pAccel.SetCount(ACCEL_LIST_SIZE);
    int accel_count = 0;
    POSITION pos = m_wmcmds.GetHeadPosition();
    for (int i = 0; pos; i++) {
        ACCEL x = m_wmcmds.GetNext(pos);
        if (x.key > 0) {
            pAccel[accel_count] = x;
            accel_count++;
        }
    }
    s.hAccel = CreateAcceleratorTable(pAccel.GetData(), accel_count);

    CFrameWnd* parent = GetParentFrame();
    if (parent) {
        parent->m_hAccelTable = s.hAccel;
    }

    s.fWinLirc = !!m_fWinLirc;
    s.strWinLircAddr = m_WinLircAddr;
    if (s.fWinLirc) {
        s.WinLircClient.Connect(m_WinLircAddr);
    }
    s.fUIce = !!m_fUIce;
    s.strUIceAddr = m_UIceAddr;
    if (s.fUIce) {
        s.UIceClient.Connect(m_UIceAddr);
    }
    s.fGlobalMedia = !!m_fGlobalMedia;

    AfxGetMyApp()->RegisterHotkeys();

    return __super::OnApply();
}

void CPPageAccelTbl::OnBnClickedSelectAll()
{
    m_list.SetFocus();

    for (int i = 0, j = m_list.GetItemCount(); i < j; i++) {
        m_list.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
    }
}

void CPPageAccelTbl::OnBnClickedReset()
{
    m_list.SetFocus();

    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (!pos) {
        return;
    }

    while (pos) {
        int ni = m_list.GetNextSelectedItem(pos);
        POSITION pi = ((ITEMDATA*)m_list.GetItemData(ni))->index;
        wmcmd& wc = m_wmcmds.GetAt(pi);
        wc.Restore();
    }

    SetupList();

    SetModified();
}

void CPPageAccelTbl::OnChangeFilterEdit()
{
    KillTimer(filterTimerID);
    filterTimerID = SetTimer(2, 100, NULL);
}

void  CPPageAccelTbl::FilterList()
{
    CString filter;
    filterEdit.GetWindowText(filter);
    LANGID langid = AfxGetAppSettings().language;
    filter = NormalizeUnicodeStrForSearch(filter, langid);

    m_list.SetRedraw(false);
    m_list.DeleteAllItems();
    m_pItemsData.clear();

    POSITION pos = m_wmcmds.GetHeadPosition();
    for (; pos; ) {
        CString hotkey, id, name, sname;

        wmcmd& wc = m_wmcmds.GetAt(pos);

        HotkeyModToString(wc.key, wc.fVirt, hotkey);
        id.Format(_T("%u"), wc.cmd);
        sname = wc.GetName();

        sname = NormalizeUnicodeStrForSearch(sname, langid);
        id = NormalizeUnicodeStrForSearch(id, langid);
        hotkey = NormalizeUnicodeStrForSearch(hotkey, langid);

        if (filter.IsEmpty() || sname.Find(filter) != -1 || hotkey.Find(filter) != -1 || id.Find(filter) != -1) {
            int row = m_list.InsertItem(m_list.GetItemCount(), wc.GetName(), COL_CMD);
            auto itemData = std::make_unique<ITEMDATA>();
            itemData->index = pos;
            m_list.SetItemData(row, (DWORD_PTR)itemData.get());
            m_pItemsData.push_back(std::move(itemData));
        }
        m_wmcmds.GetNext(pos);
    }
    SetupList(false);
    m_list.SetRedraw(true);
    m_list.RedrawWindow();
}

void CPPageAccelTbl::GetCustomTextColors(INT_PTR nItem, int iSubItem, COLORREF& clrText, COLORREF& clrTextBk, bool& overrideSelectedBG) {
    auto itemData = (ITEMDATA*)m_list.GetItemData(nItem);
    auto dup = itemData->flag;
    if (iSubItem == COL_CMD && dup
        || iSubItem == COL_KEY && (dup & DUP_KEY)
        || iSubItem == COL_APPCMD && (dup & DUP_APPCMD)
        || iSubItem == COL_RMCMD && (dup & DUP_RMCMD)) {
        if (AppIsThemeLoaded()) {
            clrTextBk = CMPCTheme::ListCtrlErrorColor;
            overrideSelectedBG = true;
        } else {
            clrTextBk = RGB(255, 130, 120);
        }
    } else {
        if (AppIsThemeLoaded()) {
            clrTextBk = CMPCTheme::ContentBGColor;
        } else {
            clrTextBk = GetSysColor(COLOR_WINDOW);
        }
    }
}

void CPPageAccelTbl::GetCustomGridColors(int nItem, COLORREF& horzGridColor, COLORREF& vertGridColor) {
    horzGridColor = CMPCTheme::ListCtrlGridColor;
    vertGridColor = CMPCTheme::ListCtrlGridColor;
}

void CPPageAccelTbl::OnCustomdrawList(NMHDR* pNMHDR, LRESULT* pResult) {
    //this custom draw is used only in classic mode
    *pResult = CDRF_DODEFAULT;
    if (!AppNeedsThemedControls()) {
        NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);

        if (CDDS_PREPAINT == pLVCD->nmcd.dwDrawStage) {
            *pResult = CDRF_NOTIFYITEMDRAW;
        } else if (CDDS_ITEMPREPAINT == pLVCD->nmcd.dwDrawStage) {
            *pResult = CDRF_NOTIFYSUBITEMDRAW;
        } else if ((CDDS_ITEMPREPAINT | CDDS_SUBITEM) == pLVCD->nmcd.dwDrawStage) {
            bool ignore;
            GetCustomTextColors(pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, pLVCD->clrText, pLVCD->clrTextBk, ignore);
            *pResult = CDRF_DODEFAULT;
        }
    }
}

void CPPageAccelTbl::OnBeginListLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (pItem->iItem < 0) {
        return;
    }

    if (pItem->iSubItem == COL_KEY || pItem->iSubItem == COL_APPCMD || pItem->iSubItem == COL_RMCMD || pItem->iSubItem == COL_RMREPCNT) {
        *pResult = TRUE;
    }
}

static BYTE s_mods[] = {0, FALT, FCONTROL, FSHIFT, FCONTROL | FALT, FCONTROL | FSHIFT, FALT | FSHIFT, FCONTROL | FALT | FSHIFT};

void CPPageAccelTbl::OnDoListLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    if (pItem->iItem < 0) {
        *pResult = FALSE;
        return;
    }

    *pResult = TRUE;

    
    wmcmd& wc = m_wmcmds.GetAt(((ITEMDATA*)m_list.GetItemData(pItem->iItem))->index);

    CAtlList<CString> sl;
    int nSel = -1;

    auto createHotkey = [&](auto virt, auto key) {
        m_list.ShowInPlaceWinHotkey(pItem->iItem, pItem->iSubItem);
        CWinHotkeyCtrl* pWinHotkey = (CWinHotkeyCtrl*)m_list.GetDlgItem(IDC_WINHOTKEY1);
        UINT cod = 0, mod = 0;
        if (virt & FALT) {
            mod |= MOD_ALT;
        }
        if (virt & FCONTROL) {
            mod |= MOD_CONTROL;
        }
        if (virt & FSHIFT) {
            mod |= MOD_SHIFT;
        }
        cod = key;
        pWinHotkey->SetWinHotkey(cod, mod);
    };

    switch (pItem->iSubItem) {
        case COL_KEY:
            createHotkey(wc.fVirt, wc.key);
            break;
        case COL_APPCMD:
            for (int i = 0; i < _countof(g_CommandList); i++) {
                sl.AddTail(g_CommandList[i].cmdname);
                if (wc.appcmd == g_CommandList[i].appcmd) {
                    nSel = i;
                }
            }

            m_list.ShowInPlaceComboBox(pItem->iItem, pItem->iSubItem, sl, nSel);
            break;
        case COL_RMCMD:
            m_list.ShowInPlaceEdit(pItem->iItem, pItem->iSubItem);
            break;
        case COL_RMREPCNT:
            m_list.ShowInPlaceEdit(pItem->iItem, pItem->iSubItem);
            break;
        default:
            *pResult = FALSE;
            break;
    }
}

int CPPageAccelTbl::CompareFunc(LPARAM lParam1, LPARAM lParam2)
{
    int result;

    CString strItem1 = m_list.GetItemText(static_cast<int>(lParam1), sortColumn);
    CString strItem2 = m_list.GetItemText(static_cast<int>(lParam2), sortColumn);
    if (sortColumn == COL_ID || sortColumn == COL_RMREPCNT) {
        wmcmd& wc1 = m_wmcmds.GetAt(((ITEMDATA*)m_list.GetItemData(static_cast<int>(lParam1)))->index);
        wmcmd& wc2 = m_wmcmds.GetAt(((ITEMDATA*)m_list.GetItemData(static_cast<int>(lParam2)))->index);

        result = wc1.cmd == wc2.cmd ? 0 : (wc1.cmd < wc2.cmd ? -1 : 1);
    } else {
        result = strItem1.Compare(strItem2);
    }

    if (sortDirection == HDF_SORTUP) {
        return result;
    } else {
        return -result;
    }
}

static int CALLBACK StaticCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    CPPageAccelTbl* ppAccelTbl = (CPPageAccelTbl*)lParamSort;
    return ppAccelTbl->CompareFunc(lParam1, lParam2);
}

void CPPageAccelTbl::UpdateHeaderSort(int column, int sort)
{
    CHeaderCtrl* hdr = m_list.GetHeaderCtrl();
    HDITEMW hItem = { 0 };
    hItem.mask = HDI_FORMAT;
    if (hdr->GetItem(column, &hItem)) {
        if (sort == HDF_SORTUP) {
            hItem.fmt |= HDF_SORTUP;
            hItem.fmt &= ~HDF_SORTDOWN;
        } else if (sort == HDF_SORTDOWN) {
            hItem.fmt |= HDF_SORTDOWN;
            hItem.fmt &= ~HDF_SORTUP;
        } else { //no sort
            hItem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        }
        hdr->SetItem(column, &hItem);
    }
}

void CPPageAccelTbl::OnListColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
    int colToSort = pNMListView->iSubItem;
    if (colToSort == sortColumn) {
        sortDirection = sortDirection == HDF_SORTUP ? HDF_SORTDOWN : HDF_SORTUP;
    } else {
        if (sortColumn != -1) {
            UpdateHeaderSort(sortColumn, 0); //clear old sort
        }
        sortColumn = colToSort;
        sortDirection = HDF_SORTUP;
    }
    m_list.SortItemsEx(StaticCompareFunc, (LPARAM)this);
    UpdateHeaderSort(sortColumn, sortDirection);
}

void CPPageAccelTbl::OnEndListLabelEdit(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    LV_ITEM* pItem = &pDispInfo->item;

    *pResult = FALSE;

    if (!m_list.m_fInPlaceDirty) {
        return;
    }

    if (pItem->iItem < 0) {
        return;
    }
    wmcmd& wc = m_wmcmds.GetAt(((ITEMDATA*)m_list.GetItemData(pItem->iItem))->index);

    auto updateHotkey = [&](auto &virt, auto &key) {
        UINT cod, mod;
        CWinHotkeyCtrl* pWinHotkey = (CWinHotkeyCtrl*)m_list.GetDlgItem(IDC_WINHOTKEY1);
        pWinHotkey->GetWinHotkey(&cod, &mod);
        ASSERT(cod < WORD_MAX);
        key = (WORD)cod;
        virt = FVIRTKEY;
        if (mod & MOD_ALT) {
            virt |= FALT;
        }
        if (mod & MOD_CONTROL) {
            virt |= FCONTROL;
        }
        if (mod & MOD_SHIFT) {
            virt |= FSHIFT;
        }
 
        CString str;
        HotkeyToString(key, mod, str);
        m_list.SetItemText(pItem->iItem, pItem->iSubItem, str);

        *pResult = TRUE;
        UpdateKeyDupFlags();
    };

    WORD discard;
    switch (pItem->iSubItem) {
        case COL_KEY:
            updateHotkey(wc.fVirt, wc.key);
            break;
        case COL_APPCMD: {
            ptrdiff_t i = pItem->lParam;
            if (i >= 0 && i < _countof(g_CommandList)) {
                wc.appcmd = g_CommandList[i].appcmd;
                m_list.SetItemText(pItem->iItem, COL_APPCMD, pItem->pszText);
                *pResult = TRUE;
                UpdateAppcmdDupFlags();
            }
        }
        break;
        case COL_RMCMD: {
            CString cmd = pItem->pszText;
            cmd.Trim();
            cmd.Replace(_T(' '), ('_'));
            m_list.SetItemText(pItem->iItem, pItem->iSubItem, cmd);
            wc.rmcmd = cmd;
            *pResult = TRUE;
            UpdateRmcmdDupFlags();
            break;
        }
        case COL_RMREPCNT:
            CString str = pItem->pszText;
            wc.rmrepcnt = _tcstol(str.Trim(), nullptr, 10);
            str.Format(_T("%d"), wc.rmrepcnt);
            m_list.SetItemText(pItem->iItem, pItem->iSubItem, str);
            *pResult = TRUE;
            break;
    }

    if (*pResult) {
        m_list.RedrawWindow();
        SetModified();
    }
}

void CPPageAccelTbl::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nStatusTimerID) {
        UpdateData();

        CAppSettings& s = AfxGetAppSettings();

        if (m_fWinLirc) {
            CString addr;
            m_WinLircEdit.GetWindowText(addr);
            s.WinLircClient.Connect(addr);
        }

        m_WinLircEdit.Invalidate();

        if (m_fUIce) {
            CString addr;
            m_UIceEdit.GetWindowText(addr);
            s.UIceClient.Connect(addr);
        }

        m_UIceEdit.Invalidate();

        m_counter++;
    } else if (nIDEvent == filterTimerID) {
        KillTimer(filterTimerID);
        FilterList();
    } else {
        __super::OnTimer(nIDEvent);
    }
}

HBRUSH CPPageAccelTbl::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);

    const CAppSettings& s = AfxGetAppSettings();
    if (AppIsThemeLoaded()) {
        return hbr; //should have already been handled inside themed ctlcolor
    }
    int status = -1;

    if (*pWnd == m_WinLircEdit) {
        status = s.WinLircClient.GetStatus();
    } else if (*pWnd == m_UIceEdit) {
        status = s.UIceClient.GetStatus();
    }

    if (status == 0 || status == 2 && (m_counter & 1)) {
        pDC->SetTextColor(0x0000ff);
    } else if (status == 1) {
        pDC->SetTextColor(0x008000);
    }

    return hbr;
}

BOOL CPPageAccelTbl::OnSetActive()
{
    m_nStatusTimerID = SetTimer(1, 1000, nullptr);

    return CPPageBase::OnSetActive();
}

BOOL CPPageAccelTbl::OnKillActive()
{
    KillTimer(m_nStatusTimerID);
    m_nStatusTimerID = 0;

    return CPPageBase::OnKillActive();
}

void CPPageAccelTbl::OnCancel()
{
    CAppSettings& s = AfxGetAppSettings();

    if (!s.fWinLirc) {
        s.WinLircClient.DisConnect();
    }
    if (!s.fUIce) {
        s.UIceClient.DisConnect();
    }

    __super::OnCancel();
}
