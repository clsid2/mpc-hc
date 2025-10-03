/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
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

#include "MediaFormats.h"
#include <atlsync.h>

#include <array>
#include <memory>
#include <mutex>
#include <thread>

class CFileAssoc
{
public:
    enum reg_state_t {
        NOT_REGISTERED,
        SOME_REGISTERED,
        ALL_REGISTERED
    };
    enum autoplay_t {
        AP_VIDEO,
        AP_MUSIC,
        AP_AUDIOCD,
        AP_DVDMOVIE
    };

    class IconLib
    {
    public:
        typedef int(*GetIconIndexFunc)(LPCTSTR);
        typedef UINT(*GetIconLibVersionFunc)();

        IconLib() = delete;
        IconLib(const IconLib&) = delete;
        IconLib& operator=(const IconLib&) = delete;
        IconLib(GetIconIndexFunc fnGetIconIndex, GetIconLibVersionFunc fnGetIconLibVersion, HMODULE hLib);
        ~IconLib();

        int GetIconIndex(const CString& str) const;
        UINT GetVersion() const;
        void SaveVersion() const;

    protected:
        const GetIconIndexFunc m_fnGetIconIndex;
        const GetIconLibVersionFunc m_fnGetIconLibVersion;
        const HMODULE m_hLib;
    };

    CFileAssoc();
    CFileAssoc(const CFileAssoc&) = delete;
    CFileAssoc& operator=(const CFileAssoc&) = delete;
    ~CFileAssoc();

    void LoadAAR();

    std::shared_ptr<const IconLib> GetIconLib() const;

    void SetNoRecentDocs(bool bNoRecentDocs, bool bUpdateAssocs = false);

    bool RegisterApp();

    bool Register(CString ext, CString strLabel, bool bRegister, bool bRegisterContextMenuEntries, bool bAssociatedWithIcon);
    bool IsRegistered(CString ext) const;
    bool HasEnqueueContextMenuEntry(CString strExt) const;

    bool Register(const CMediaFormatCategory& mfc, bool bRegister, bool bRegisterContextMenuEntries, bool bAssociatedWithIcon);
    reg_state_t IsRegistered(const CMediaFormatCategory& mfc) const;
    reg_state_t HasAnyEnqueueContextMenuEntries(const CMediaFormatCategory& mfc) const;

    bool RegisterFolderContextMenuEntries(bool bRegister);
    bool AreRegisteredFolderContextMenuEntries() const;

    bool RegisterAutoPlay(autoplay_t ap, bool bRegister);
    bool IsAutoPlayRegistered(autoplay_t ap) const;

    bool GetAssociatedExtensions(const CMediaFormats& mf, CAtlList<CString>& exts) const;
    bool GetAssociatedExtensionsFromRegistry(CAtlList<CString>& exts);

    bool ReAssocIcons(const CAtlList<CString>& exts);

    void CheckIconsAssoc();

    bool ShowWindowsAssocDialog() const;

protected:
    struct Handler {
        CString verb;
        CString cmd;
        UINT action;

        Handler()
            : action(0) {}
        Handler(const CString& verb, const CString& cmd, UINT action)
            : verb(verb), cmd(cmd), action(action) {}
    };

    bool SetFileAssociation(CString strExt, CString strProgID, bool bRegister);

    void CheckIconsAssocThread();

    const CString m_iconLibPath;
    const CString m_strRegisteredAppName;
    const CString m_strOldAssocKey;
    const CString m_strRegisteredAppKey;
    const CString m_strRegAppFileAssocKey;

    const CString m_strOpenCommand;
    const CString m_strEnqueueCommand;

    bool m_bNoRecentDocs;

    CComPtr<IApplicationAssociationRegistration> m_pAAR;

    std::mutex m_checkIconsAssocMutex;
    ATL::CEvent m_checkIconsAssocInactiveEvent;

    std::array<Handler, 4> m_handlers;
};
