/*
 * (C) 2016-2017 see Authors.txt
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

#include "SubtitlesProvidersUtils.h"
#include "../Subtitles/SubtitleHelpers.h"
#include "base64/base64.h"
#include "VersionInfo.h"

class CMainFrame;
struct SubtitlesInfo;
class SubtitlesThread;
class SubtitlesTask;
class SubtitlesProvider;
class SubtitlesProviders;

using SubtitlesList = std::list<SubtitlesInfo>;

enum SubtitlesProviderFlags {
    SPF_SEARCH = 0x00000000,
    SPF_LOGIN  = 0x00000001,
    SPF_HASH   = 0x00000002,
};

enum SubtitlesProviderLogin {
    SPL_UNDEFINED  = 0x00000000,
    SPL_FAILED     = 0x00000001,
    SPL_ANONYMOUS  = 0x00000002,
    SPL_REGISTERED = 0x00000004,
};

enum SRESULT {
    SR_UNDEFINED,
    SR_SUCCEEDED,
    SR_FAILED,
    SR_ABORTED,
    // Specific to search only
    SR_TOOMANY,
};

enum SubtitlesThreadType {
    STT_UNDEFINED = 0x00000000,
    STT_SEARCH    = 0x00000001,
    STT_DOWNLOAD  = 0x00000002,
    STT_MANUALSEARCH = 0x00000008
};


struct SubtitlesInfo {
    SubtitlesInfo()
        : fileProvider(nullptr)
        , uid(UINT_ERROR)
        , score(0ul)
        , fileSize(ULONGLONG_ERROR)
        , year(INT_ERROR)
        , seasonNumber(INT_ERROR)
        , episodeNumber(INT_ERROR)
        , discNumber(INT_ERROR)
        , hearingImpaired(Subtitle::HI_UNKNOWN)
        , discCount(INT_ERROR)
        , downloadCount(INT_ERROR)
        , corrected(0)
        , frameRate(-1.0)
        , framesNumber(INT_ERROR)
        , lengthMs(ULONGLONG_ERROR) {}
    bool operator<(const SubtitlesInfo& rhs) const { return score > rhs.score; }
    HRESULT GetFileInfo(const std::string& sFileName = std::string());
    void Download(bool bActivate);
    void OpenUrl() const;
    std::shared_ptr<SubtitlesProvider> Provider() const { return fileProvider; }
    void Provider(std::shared_ptr<SubtitlesProvider> pProvider) { fileProvider = pProvider; }
    DWORD Score() const { return score; }
    void Set(std::shared_ptr<SubtitlesProvider> pProvider, BYTE nLanguage, BYTE nHearingImpaired, SHORT nScore) {
        static UINT i(0);
        // run twice to check whether i has reached MAXUINT32 which is invalid
        if (uid == UINT_ERROR) {
            uid = ++i;
            if (uid == UINT_ERROR) {
                uid = ++i;
            }
        }
        fileProvider = pProvider;
        score = MAKELONG(nScore + 0x10, MAKEWORD(nHearingImpaired, nLanguage));
    }

    std::string DisplayTitle() const {
        std::string _title(title);
        if (!title2.empty()) {
            _title.append(": " + title2);
        }
        if (year != -1) {
            _title.append(" (" + std::to_string(year) + ")");
        }
        return _title;
    }

    std::string NormalizeString(std::string sTitle) const {
        // remove ' and ' from string and replace '!?&:\' with ' ' to get more accurate results
        sTitle = std::regex_replace(sTitle, std::regex(" and ", SubtitlesProvidersUtils::RegexFlags), " ");
        sTitle = std::regex_replace(sTitle, std::regex(" *[!?&:] *", SubtitlesProvidersUtils::RegexFlags), " ");
        sTitle = std::regex_replace(sTitle, std::regex("'", SubtitlesProvidersUtils::RegexFlags), "");

        return sTitle;
    }

    std::string NormalizeTitle() const { return NormalizeString(title); }
    UINT UID() const { return uid; }

private:
    std::shared_ptr<SubtitlesProvider> fileProvider;
    UINT uid;
    DWORD score;
public:
    // file properties
    std::wstring filePathW;
    std::string filePath;
    std::string fileName;
    std::string fileExtension;
    ULONGLONG fileSize;
    std::string fileContents;
    std::string fileHash;
    CComQIPtr<IAsyncReader> pAsyncReader;

    // file name properties
    std::string title;
    std::string country;
    int year;
    std::string episode;
    int seasonNumber;
    int episodeNumber;
    std::string title2;
    std::string resolution;
    std::string format;
    std::string audioCodec;
    std::string videoCodec;
    std::string releaseGroup;
    int discNumber;

    // subtitles properties
    std::string id;
    std::string imdbid;
    std::string languageCode;
    std::string languageName;
    std::string url;
    std::list<std::string> releaseNames;
    int hearingImpaired;
    int discCount;
    int downloadCount;
    int corrected;

    // video properties
    double frameRate;
    int framesNumber;
    ULONGLONG lengthMs;

    CString manualSearchString;
};


class CWinThreadProc
{
public:
    CWinThreadProc() : m_pThread(nullptr), m_bAbort(false) {}
    virtual ~CWinThreadProc() = default;

    operator CWinThread* () const { return m_pThread; }

    bool IsThreadRunning() const { return m_pThread != nullptr; }
    volatile bool& IsThreadAborting() { return m_bAbort; }

    CWinThread* CreateThread() {
        if (!IsThreadRunning()) {
            m_pThread = AfxBeginThread(_ThreadProc, this);
        }
        return m_pThread;
    }
    void AbortThread() {
        if (IsThreadRunning()) {
            CAutoLock tlock(&m_csThreadLock);
            m_bAbort = true;
        }
    }
    void WaitThread() const {
        if (IsThreadRunning()) {
            ::WaitForSingleObjectEx(*m_pThread, INFINITE, TRUE);
        }
    }

protected:
    CCritSec m_csThreadLock;

private:
    static UINT _ThreadProc(LPVOID pThreadParams) {
        CWinThreadProc* pThread = static_cast<CWinThreadProc*>(pThreadParams);
        pThread->ThreadProc();
        return 0;
    }
    virtual void ThreadProc() PURE;

    CWinThread* m_pThread;
    volatile bool m_bAbort;
};


class SubtitlesThread final : public CWinThreadProc
{
    friend class SubtitlesProvider;
    friend class SubtitlesTask;
public:
    SubtitlesThread(SubtitlesTask* pTask, const SubtitlesInfo& pFileInfo, std::shared_ptr<SubtitlesProvider> pProvider)
        : m_pTask(pTask)
        , m_pFileInfo(pFileInfo)
    { m_pFileInfo.Provider(pProvider); }

private:
    void Set(SubtitlesInfo& pSubtitlesInfo);

    void ThreadProc() override;
    void Search();
    void Download(SubtitlesInfo& pFileInfo, BOOL bActivate);
    void Download();

    void CheckAbortAndThrow() {
        if (IsThreadAborting()) {
            throw E_ABORT;
        }
    }

    SubtitlesTask* m_pTask;
    SubtitlesInfo m_pFileInfo;
    SubtitlesList m_pSubtitlesList;
};


class SubtitlesTask final : public CWinThreadProc
{
    friend class SubtitlesThread;
public:
    // Search
    SubtitlesTask(CMainFrame* pMainFrame, bool bAutoDownload, const std::list<std::string>& sLanguages);
    SubtitlesTask(CMainFrame* pMainFrame, bool bAutoDownload, const std::list<std::string>& sLanguages, CString manualSearch);
    // Download
    SubtitlesTask(CMainFrame* pMainFrame, SubtitlesInfo& pSubtitlesInfo, bool bActivate);

    SubtitlesThreadType Type() const { return m_nType; };
    BYTE GetLangPriority(const std::string& sLanguage) {
        return m_LangPriority.count(sLanguage) ? m_LangPriority[sLanguage] : 0;
    }

    void InsertThread(SubtitlesThread* pThread) {
        CAutoLock tlock(&m_csThreadLock);
        m_pThreads.push_back(pThread);
    }

    void RemoveThread(SubtitlesThread* pThread) {
        CAutoLock tlock(&m_csThreadLock);
        m_pThreads.remove(pThread);
        delete pThread;
    }

    void Abort() {
        CAutoLock tlock(&m_csThreadLock);
        for (auto& iter : m_pThreads) {
            iter->AbortThread();
        }
        AbortThread();
    }

private:
    void ThreadProc() override;

    CMainFrame* m_pMainFrame;
    std::list<SubtitlesThread*> m_pThreads;
    CCritSec m_csDownload;

    SubtitlesInfo m_pFileInfo;
    bool m_bActivate;
    SubtitlesThreadType m_nType;
    bool m_bAutoDownload;
    std::unordered_map<std::string, bool> m_AutoDownload;
    std::unordered_map<std::string, BYTE> m_LangPriority;

    CString manualSearch;
};

class SubtitlesProvider
{
public:
    SubtitlesProvider(SubtitlesProviders* pOwner);
    virtual ~SubtitlesProvider() = default;

public: // implemented
    virtual std::string Name() const PURE;
    virtual std::string DisplayName() const PURE;
    virtual std::string Url() const PURE;
    virtual const std::set<std::string>& Languages() const PURE;
    virtual bool Flags(DWORD dwFlags) const PURE;
    virtual int Icon() const PURE;
    virtual SRESULT Search(const SubtitlesInfo& pFileInfo) PURE;
    virtual SRESULT Download(SubtitlesInfo& pSubtitlesInfo) PURE;

protected: // overridden
    virtual void Initialize() {}
    virtual void Uninitialize() { LogOut(); }
public: // overridden
    virtual bool NeedLogin() { return !(m_nLoggedIn & (SPL_REGISTERED | SPL_ANONYMOUS)); }
    virtual SRESULT Login(const std::string&, const std::string&) { return SR_UNDEFINED; }
    virtual SRESULT LogOut() {
        m_nLoggedIn = SPL_UNDEFINED;
        return SR_SUCCEEDED;
    }
    virtual SRESULT Hash(SubtitlesInfo&) { return SR_UNDEFINED; }
    virtual std::string UserAgent() const {
        return SubtitlesProvidersUtils::StringFormat("MPC-HC v%u.%u.%u",
                                                     VersionInfo::GetMajorNumber(),
                                                     VersionInfo::GetMinorNumber(),
                                                     VersionInfo::GetPatchNumber());
    }
    virtual bool UseForAutoDownload() { return true; }

    bool LoginInternal();
    void OpenUrl() const;
    size_t Index() const;
    static bool CheckInternetConnection();
    static bool CheckLanguage(const std::string& sLanguageCode);
    std::list<std::string> GetLanguagesIntersection() const;
    std::list<std::string> GetLanguagesIntersection(std::list<std::string>&& userSelectedLangauges) const;
    bool SupportsUserSelectedLanguages() const;
    SRESULT DownloadInternal(std::string url, std::string referer, std::string& data);
    static void Set(SubtitlesInfo& pSubtitlesInfo);
    static bool IsAborting();

    BOOL Enabled(SubtitlesProviderFlags nFlag) { return m_bSearch; }
    void Enabled(SubtitlesProviderFlags nFlag, BOOL bEnabled) {
        m_bSearch = bEnabled;
    }
    std::string UserName() const { return m_sUserName; };
    void UserName(std::string sUserName) { m_sUserName = sUserName; };
    std::string Password(bool bDecrypt = true) {
        return bDecrypt
               ? SubtitlesProvidersUtils::StringDecrypt(Base64::decode(m_sPassword), SubtitlesProvidersUtils::StringGenerateUniqueKey())
               : m_sPassword;
    };
    void Password(LPCSTR sPassword, bool bEncrypt = true) {
        m_sPassword = bEncrypt
                      ? Base64::encode(SubtitlesProvidersUtils::StringEncrypt(sPassword, SubtitlesProvidersUtils::StringGenerateUniqueKey()))
                      : sPassword;
    };
    SubtitlesProviders& Providers() const { return *m_pOwner; }
    int GetIconIndex() const { return m_nIconIndex; }
    void SetIconIndex(int nIconIndex) { m_nIconIndex = nIconIndex; }

private:
    BOOL m_bSearch;
    std::string m_sUserName;
    std::string m_sPassword;
    SubtitlesProviders* m_pOwner;
    int m_nIconIndex;
protected:
    SubtitlesProviderLogin m_nLoggedIn;
public:
    DWORD m_dwLastStatusCode;
};

class SubtitlesProviders final
{
public:
    explicit SubtitlesProviders(CMainFrame* pMainFrame);
    ~SubtitlesProviders();
    SubtitlesProviders(SubtitlesProviders const&) = delete;
    SubtitlesProviders& operator=(SubtitlesProviders const&) = delete;

private:
    void RegisterProviders();
    template <class T>
    void Register(SubtitlesProviders* pOwner) {
        m_pProviders.push_back(T::Create(pOwner));
        auto& provider = m_pProviders.back();
        HICON hIcon = ::LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(provider->Icon()));
        provider->SetIconIndex(m_himl.Add(hIcon));
        DestroyIcon(hIcon);
    }

public:
    const std::vector<std::shared_ptr<SubtitlesProvider>>& Providers() const {
        return m_pProviders;
    };
    static BOOL SubtitlesProviders::CheckInternetConnection();

    void ReadSettings();
    std::string WriteSettings();

    void Search(bool bAutoDownload);
    void ManualSearch(bool bAutoDownload, CString manualSearch);
    void Download(SubtitlesInfo& pSubtitlesInfo, bool bActivate);
    void Abort(SubtitlesThreadType nType);

    void InsertTask(SubtitlesTask* pTask) {
        //CAutoLock tasklock(&m_csTasks);
        m_pTasks.push_back(pTask);
    }

    void RemoveTask(SubtitlesTask* pTask) {
        CAutoLock tasklock(&m_csTasks);
        if(!m_pTasks.empty()) {
            m_pTasks.remove(pTask);
        }
    }

    void MoveUp(size_t nIndex) {
        std::iter_swap(m_pProviders.begin() + nIndex, m_pProviders.begin() + nIndex - 1);
    }

    void MoveDown(size_t nIndex) {
        std::iter_swap(m_pProviders.begin() + nIndex, m_pProviders.begin() + nIndex + 1);
    }

    CImageList& GetImageList() { return m_himl; }

private:
    CMainFrame* m_pMainFrame;

    std::vector<std::shared_ptr<SubtitlesProvider>> m_pProviders;

    CCritSec m_csTasks;
    std::list<SubtitlesTask*> m_pTasks;
    CImageList m_himl;
};
