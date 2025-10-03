/*
* (C) 2015-2017 see Authors.txt
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

#include "CMPCThemePPageBase.h"
#include "resource.h"
#include "EventDispatcher.h"
#include <utility>
#include <memory>
#include <map>
#include <deque>
#include "CMPCThemeComboBox.h"
#include "CMPCThemeSpinButtonCtrl.h"
#include "CMPCThemePlayerListCtrl.h"

class SettingsBase
{
    CString name;
    CString toolTipText;

public:
    SettingsBase(CString name, CString toolTipText)
        : name(name)
        , toolTipText(toolTipText) {
    }
    virtual ~SettingsBase() = default;

    CString GetToolTipText() const {
        return toolTipText;
    }
    CString GetName() const {
        return name;
    }
    virtual bool IsDefault() const PURE;
    virtual void ResetDefault() PURE;
    virtual void Apply() PURE;
};

class SettingsBool : public SettingsBase
{
    const bool defaultValue;
    bool currentValue;
    bool& settingReference;

public:
    SettingsBool(CString name, bool defaultValue, bool& settingReference, CString toolTipText)
        : SettingsBase(name, toolTipText)
        , defaultValue(defaultValue)
        , currentValue(settingReference)
        , settingReference(settingReference) {
    }

    bool IsDefault() const {
        return currentValue == defaultValue;
    }
    void ResetDefault() {
        SetValue(defaultValue);
    }
    void SetValue(bool value) {
        currentValue = value;
    }
    bool GetValue() const {
        return currentValue;
    }
    void Apply() {
        settingReference = currentValue;
    }
    void Toggle() {
        currentValue = !currentValue;
    }
};

class SettingsInt : public SettingsBase
{
    const int defaultValue;
    int currentValue;
    int& settingReference;
    std::pair<int, int> range;

public:
    SettingsInt(CString name, int defaultValue, int& settingReference, std::pair<int, int> range, CString toolTipText)
        : SettingsBase(name, toolTipText)
        , defaultValue(defaultValue)
        , currentValue(settingReference)
        , settingReference(settingReference)
        , range(std::move(range)) {
    }

    bool IsDefault() const {
        return currentValue == defaultValue;
    }
    void ResetDefault() {
        SetValue(defaultValue);
    }
    void SetValue(int value) {
        currentValue = value;
    }
    int GetValue() const {
        return currentValue;
    }
    void Apply() {
        settingReference = currentValue;
    }
    std::pair<int, int> GetRange() const {
        return range;
    }
};

class SettingsCombo : public SettingsInt
{
    std::deque<CString> list;

public:
    SettingsCombo(CString name, int defaultValue, int& settingReference, std::deque<CString> list, CString toolTipText)
        : SettingsInt(name, defaultValue, settingReference, std::make_pair(0, (int)list.size() - 1), toolTipText)
        , list(std::move(list)) {
    }

    std::deque<CString> GetList() const {
        return list;
    }
};

class SettingsCString : public SettingsBase
{
    const CString defaultValue;
    CString currentValue;
    CString& settingReference;

public:
    SettingsCString(CString name, CString defaultValue, CString& settingReference, CString toolTipText)
        : SettingsBase(name, toolTipText)
        , defaultValue(defaultValue)
        , currentValue(settingReference)
        , settingReference(settingReference) {
    }

    bool IsDefault() const {
        return currentValue == defaultValue;
    }
    void ResetDefault() {
        SetValue(defaultValue);
    }
    void SetValue(const CString& value) {
        currentValue = value;
    }
    CString GetValue() const {
        return currentValue;
    }
    void Apply() {
        settingReference = currentValue;
    }
};

class CPPageAdvanced : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageAdvanced)
public:
    CPPageAdvanced();
    virtual ~CPPageAdvanced() = default;
    virtual void DoDPIChanged();

private:
    enum { IDD = IDD_PPAGEADVANCED };

    enum ADVANCED_SETTINGS {
        RECENT_FILES_NB,
        FILE_POS_LONGER,
        FILE_POS_AUDIO,
        FILE_POS_PLAYLIST,
        FILE_POS_TRACK_SELECTION,
        FULLSCREEN_SEPARATE_CONTROLS,
        COVER_SIZE_LIMIT,
        BLOCK_VSFILTER,
        BLOCK_RDP,
        LOOP_FOLDER_NEXT_FILE,
        OSD_TRANSPARENCY,
        OSD_BORDER,
        USE_YDL,
        YDL_MAX_HEIGHT,
        YDL_VIDEO_FORMAT,
        YDL_AUDIO_FORMAT,
        YDL_AUDIO_ONLY,
        YDL_EXEPATH,
        YDL_COMMAND_LINE,
        YDL_SUBS_PREFERENCE,
        USE_AUTOMATIC_CAPTIONS,
        SAVEIMAGE_POSITION,
        SAVEIMAGE_CURRENTTIME,
        SNAPSHOTSUBTITLES,
        SNAPSHOTKEEPVIDEOEXTENSION,
        ADD_LANGCODE_WHEN_SAVE_SUBTITLES,
        USE_TITLE_IN_RECENT_FILE_LIST,
        MOUSE_LEFTUP_DELAY,
        LOCK_NOPAUSE,
        PREVENT_DISPLAY_SLEEP,
        RELOAD_AFTER_LONG_PAUSE,
        INACCURATE_FASTSEEK,
        STILL_VIDEO_DURATION,
        STREAMPOSPOLLER_INTERVAL,
        REDIR_OPEN_TO_APPEND,
#if !defined(_DEBUG) && USE_DRDUMP_CRASH_REPORTER
        CRASHREPORTER,
#endif
        LOGGING,
        FULLSCREEN_DELAY,
        AUTO_DOWNLOAD_SCORE_MOVIES,
        AUTO_DOWNLOAD_SCORE_SERIES,
        OPEN_REC_PANEL_WHEN_OPENING_DEVICE,
        ALWAYS_USE_SHORT_MENU,
        USE_FREETYPE,
        USE_MEDIAINFO_LOAD_FILE_DURATION,
        CAPTURE_DEINTERLACE,
        PAUSE_WHILE_DRAGGING_SEEKBAR,
        CONFIRM_FILE_DELETE,
        LIBASS_FOR_SRT,
        SHOW_VOLUME_PERCENTAGE,
    };

    enum {
        COL_NAME,
        COL_VALUE,
        COL_DUMMYCOL //use to block value column from oversizing it
    };

    CFont m_fontBold;
    CMPCThemeComboBox m_comboBox;
    CMPCThemeSpinButtonCtrl m_spinButtonCtrl;

    std::map<ADVANCED_SETTINGS, std::shared_ptr<SettingsBase>> m_hiddenOptions;

    int m_lastSelectedItem = -1;

    CString m_strTrue;
    CString m_strFalse;

    void InitSettings();
    bool IsDefault(ADVANCED_SETTINGS) const;
    inline const int GetListSelectionMark() const {
        const int iItem = m_list.GetSelectionMark();
        if (iItem != m_lastSelectedItem) {
            return -1;
        }
        return iItem;
    };
    CRect editRect;

protected:
    CMPCThemePlayerListCtrl m_list;

    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual BOOL OnApply() override;
    void initBoldFont();


    afx_msg void OnBnClickedDefaultButton();
    afx_msg void OnUpdateDefaultButton(CCmdUI* pCmdUI);
    afx_msg void OnNMDblclk(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnGetInfoTipList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLvnItemchangedList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedRadio1();
    afx_msg void OnBnClickedRadio2();
    afx_msg void OnCbnSelchangeCombobox();
    afx_msg void OnEnChangeEdit();

    DECLARE_MESSAGE_MAP()
};

