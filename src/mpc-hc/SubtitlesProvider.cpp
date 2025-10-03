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

#include "stdafx.h"
#include "SubtitlesProvider.h"
#include "SubtitlesProvidersUtils.h"
#include "mplayerc.h"
#include "ISOLang.h"
#include "Logger.h"
#include "base64/base64.h"
#include "tinyxml2/library/tinyxml2.h"
#include "rapidjson/include/rapidjson/pointer.h"
#include <wincrypt.h>

#pragma warning(disable: 4244)

#define LOG SUBTITLES_LOG
#define LOG_NONE    _T("")
#define LOG_INPUT   _T("\"%S\"")
#define LOG_OUTPUT  _T("%S")
#define LOG_BOTH    _T("\"%S\"=%S")
#define LOG_ERROR   _T("ERROR: %S")

#define GUESSED_NAME_POSTFIX " (*)"
#define CheckAbortAndReturn() { if (IsAborting()) return SR_ABORTED; }

using namespace SubtitlesProvidersUtils;

class LanguageDownloadException : public std::exception
{
    using exception::exception;
};

/******************************************************************************
** Register providers
******************************************************************************/
void SubtitlesProviders::RegisterProviders()
{
    //Register<OpenSubtitles>(this);
    Register<OpenSubtitles2>(this);
    Register<podnapisi>(this);
    //Register<Napisy24>(this);
}

/******************************************************************************
** OpenSubtitles
******************************************************************************/

#if 0
void OpenSubtitles::Initialize()
{
    xmlrpc = std::make_unique<XmlRpcClient>((Url() + "/xml-rpc").c_str());
    xmlrpc->setIgnoreCertificateAuthority();
}

SRESULT OpenSubtitles::Login(const std::string& sUserName, const std::string& sPassword)
{
    // OpenSubtitles currently only works with a user account
    if (sUserName.empty()) {
        return SR_FAILED;
    }

    if (xmlrpc) {
        XmlRpcValue args, result;
        args[0] = sUserName;
        args[1] = sPassword;
        args[2] = "en";
        const auto& strUA = UserAgent();
        args[3] = strUA.c_str(); // Test with "OSTestUserAgent"

        if (!xmlrpc->execute("LogIn", args, result)) {
            return SR_FAILED;
        }

        if (result["status"].getType() == XmlRpcValue::Type::TypeString) {
            if (result["status"] == std::string("200 OK")) {
                token = result["token"];
            } else if (result["status"] == std::string("401 Unauthorized") && !sUserName.empty()) {
                // Notify user that User/Pass provided are invalid.
                CString msg;
                msg.FormatMessage(IDS_SUB_CREDENTIALS_ERROR, static_cast<LPCWSTR>(UTF8To16(Name().c_str())), static_cast<LPCWSTR>(UTF8To16(sUserName.c_str())));
                AfxMessageBox(msg, MB_ICONERROR | MB_OK);
            }
        }
    }

    LOG(LOG_BOTH, sUserName.c_str(), token.valid() ? (LPCSTR)token : "failed");
    return token.valid() ? SR_SUCCEEDED : SR_FAILED;
}

SRESULT OpenSubtitles::LogOut()
{
    if (xmlrpc && token.valid()) {
        XmlRpcValue args, result;
        args[0] = token;
        VERIFY(xmlrpc->execute("LogOut", args, result));
        token.clear();
        LOG(LOG_NONE);
    }
    m_nLoggedIn = SPL_UNDEFINED;

    return SR_SUCCEEDED;
}

SRESULT OpenSubtitles::Hash(SubtitlesInfo& pFileInfo)
{
    pFileInfo.fileHash = StringFormat("%016I64x", GenerateOSHash(pFileInfo));
    LOG(LOG_OUTPUT, pFileInfo.fileHash.c_str());
    return SR_SUCCEEDED;
}

SRESULT OpenSubtitles::Search(const SubtitlesInfo& pFileInfo)
{
    const auto languages = LanguagesISO6392();
    XmlRpcValue args, result;

    args[0] = token;
    auto& movieInfo = args[1][0];
    args[2]["limit"] = 500;
    movieInfo["sublanguageid"] = !languages.empty() ? JoinContainer(languages, ",") : "all";
    if (pFileInfo.manualSearchString.IsEmpty()) {
        movieInfo["moviehash"] = pFileInfo.fileHash;
        movieInfo["moviebytesize"] = std::to_string(pFileInfo.fileSize);
        //args[1][1]["sublanguageid"] = !languages.empty() ? languages : "all";
        //args[1][1]["tag"] = pFileInfo.fileName + "." + pFileInfo.fileExtension;

        LOG(LOG_INPUT,
            StringFormat("{ sublanguageid=\"%s\", moviehash=\"%s\", moviebytesize=\"%s\", limit=%d }",
                (LPCSTR)movieInfo["sublanguageid"],
                (LPCSTR)movieInfo["moviehash"],
                (LPCSTR)movieInfo["moviebytesize"],
                (int)args[2]["limit"]).c_str());
    } else {
        CT2CA pszConvertedAnsiString(pFileInfo.manualSearchString);
        movieInfo["query"] = std::string(pszConvertedAnsiString);
    }

    if (!xmlrpc->execute("SearchSubtitles", args, result)) {
        LOG(_T("search failed"));
        return SR_FAILED;
    }

    if (result["data"].getType() != XmlRpcValue::Type::TypeArray) {
        LOG(_T("search failed (invalid data)"));
        return SR_FAILED;
    }

    int nCount = result["data"].size();
    bool searchedByFileName = false;

    if (nCount == 0 && movieInfo.hasMember("moviehash")) {
        movieInfo.clear();
        //    movieInfo["tag"] = std::string(pFileInfo.fileName); //sadly, tag support has been disabled on opensubtitles.org :-/
        movieInfo["query"] = std::string(pFileInfo.fileName); //search by filename...as a query
        movieInfo["sublanguageid"] = !languages.empty() ? JoinContainer(languages, ",") : "all";
        if (!xmlrpc->execute("SearchSubtitles", args, result)) {
            LOG(_T("search failed"));
            return SR_FAILED;
        }
        if (result["data"].getType() != XmlRpcValue::Type::TypeArray) {
            LOG(_T("search failed (invalid data)"));
            return SR_FAILED;
        }
        nCount = result["data"].size();
        searchedByFileName = true;
    }

    std::string fnameLower = pFileInfo.fileName;
    std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), [](unsigned char c) { return std::tolower(c); });

    bool matchFound = false;
    int maxPasses = searchedByFileName ? 2 : 1;
    for (int passCount = 0; passCount < maxPasses && !matchFound; passCount++) {
        for (int i = 0; i < nCount; ++i) {
            CheckAbortAndReturn();
            XmlRpcValue& data(result["data"][i]);
            std::string subFileName = (const char*)data["SubFileName"];

            if (searchedByFileName && 0 == passCount) {
                std::string subFilePrefix = subFileName.substr(0, subFileName.find_last_of("."));
                std::transform(subFilePrefix.begin(), subFilePrefix.end(), subFilePrefix.begin(), [](unsigned char c) { return std::tolower(c); });
                if (fnameLower.compare(subFilePrefix) != 0) {
                    continue;
                }
            }
            matchFound = true;

            SubtitlesInfo pSubtitlesInfo;
            pSubtitlesInfo.id = (const char*)data["IDSubtitleFile"];
            pSubtitlesInfo.discNumber = data["SubActualCD"];
            pSubtitlesInfo.discCount = data["SubSumCD"];
            pSubtitlesInfo.fileExtension = (const char*)data["SubFormat"];
            pSubtitlesInfo.languageCode = (const char*)data["ISO639"]; //"SubLanguageID"
            pSubtitlesInfo.languageName = (const char*)data["LanguageName"];
            pSubtitlesInfo.downloadCount = data["SubDownloadsCnt"];

            pSubtitlesInfo.fileName = subFileName;
            regexResult results;
            stringMatch("\"([^\"]+)\" (.+)", (const char*)data["MovieName"], results);
            if (!results.empty()) {
                pSubtitlesInfo.title = results[0];
                pSubtitlesInfo.title2 = results[1];
            } else {
                pSubtitlesInfo.title = (const char*)data["MovieName"];
            }
            pSubtitlesInfo.year = (int)data["MovieYear"] == 0 ? -1 : (int)data["MovieYear"];
            pSubtitlesInfo.seasonNumber = (int)data["SeriesSeason"] == 0 ? -1 : (int)data["SeriesSeason"];
            pSubtitlesInfo.episodeNumber = (int)data["SeriesEpisode"] == 0 ? -1 : (int)data["SeriesEpisode"];
            pSubtitlesInfo.hearingImpaired = data["SubHearingImpaired"];
            pSubtitlesInfo.url = (const char*)data["SubtitlesLink"];
            pSubtitlesInfo.releaseNames.emplace_back((const char*)data["MovieReleaseName"]);
            pSubtitlesInfo.imdbid = (const char*)data["IDMovieImdb"];
            pSubtitlesInfo.corrected = (int)data["SubBad"] ? -1 : 0;
            Set(pSubtitlesInfo);
        }
    }

    LOG(std::to_wstring(nCount).c_str());
    return SR_SUCCEEDED;
}

SRESULT OpenSubtitles::Download(SubtitlesInfo& pSubtitlesInfo)
{
    XmlRpcValue args, result;
    args[0] = token;
    args[1][0] = pSubtitlesInfo.id;
    if (!xmlrpc->execute("DownloadSubtitles", args, result)) {
        return SR_FAILED;
    }

    LOG(LOG_INPUT, pSubtitlesInfo.id.c_str());

    if (result["data"].getType() != XmlRpcValue::Type::TypeArray) {
        LOG(_T("download failed (invalid type)"));
        return SR_FAILED;
    }

    pSubtitlesInfo.fileContents = Base64::decode(std::string(result["data"][0]["data"]));
    return SR_SUCCEEDED;
}

const std::set<std::string>& OpenSubtitles::Languages() const
{
    static std::once_flag initialized;
    static std::set<std::string> result;
#if 1
    result = {"ab", "af", "am", "an", "ar", "as", "at", "az", "be", "bg", "bn", "br", "bs", "ca", "cy", "cs", "da", "de", "ea", "el", "en", "eo", "es", "et", "eu", "ex", "fa", "fi", "fr", "ga", "gd", "gl", "he", "hi", "hr", "hu", "hy", "ia", "id", "ig", "is", "it", "ja", "ka", "kk", "km", "kn", "ko", "ku", "lb", "lt", "lv", "ma", "me", "mk", "ml", "mn", "mr", "ms", "my", "ne", "nl", "no", "nv", "oc", "or", "pb", "pl", "pm", "pr", "ps", "pt", "ro", "ru", "sd", "se", "si", "sk", "sl", "so", "sp", "sq", "sr", "sv", "sw", "sx", "sy", "ta", "te", "th", "tl", "tr", "tp", "tt", "uk", "ur", "uz", "vi", "ze", "zh", "zt"};
#else

    try {
        std::call_once(initialized, [this]() {
            if (!CheckInternetConnection()) {
                throw LanguageDownloadException("No internet connection.");
            }
            XmlRpcValue args, res;
            args = "en";
            if (!xmlrpc->execute("GetSubLanguages", args, res)) {
                throw LanguageDownloadException("Failed to execute xmlrpc command.");
            }
            if (res["data"].getType() != XmlRpcValue::Type::TypeArray) {
                throw LanguageDownloadException("Response is not an array.");
            }

            auto& data = res["data"];
            int count = data.size();
            for (int i = 0; i < count; ++i) {
#ifdef _DEBUG
                // Validate if language code conversion is in sync with OpenSubtitles database.
                std::string subLanguageID = data[i]["SubLanguageID"];
                std::string ISO6391 = data[i]["ISO639"];
                ASSERT(!ISO6391.empty());
                ASSERT(!subLanguageID.empty());
                TRACE("opensubtitles lang %s %s\n", subLanguageID.c_str(), ISO6391.c_str());
                if (ISOLang::ISO6391To6392(ISO6391.c_str()) != subLanguageID.c_str()) {
                    TRACE("opensubtitles lang %s %s\n", subLanguageID.c_str(), ISO6391.c_str());
                }
                if (ISOLang::ISO6392To6391(subLanguageID.c_str()) != ISO6391.c_str()) {
                    TRACE("opensubtitles lang %s %s\n", subLanguageID.c_str(), ISO6391.c_str());
                }
                //std::string languageName = data[i]["LanguageName"];
                //ASSERT(ISO639XToLanguage(ISO6391.c_str()) == languageName.c_str());
                //ASSERT(ISO639XToLanguage(subLanguageID.c_str()) == languageName.c_str());
#endif
                result.emplace(data[i]["ISO639"]);
            }
            });
    } catch (const LanguageDownloadException& e) {
        UNREFERENCED_PARAMETER(e);
        LOG(LOG_ERROR, e.what());
    }
#endif
    return result;
}

bool OpenSubtitles::NeedLogin()
{
    // return true to call Login() or false to skip Login()
    if (!token.valid()) {
        return true;
    }

    XmlRpcValue args, result;
    args[0] = token;
    if (!xmlrpc->execute("NoOperation", args, result)) {
        return false;
    }

    if ((result["status"].getType() == XmlRpcValue::Type::TypeString) && (result["status"] == std::string("200 OK"))) {
        return false;
    }

    return true;
}
#endif

/******************************************************************************
** OpenSubtitles.com
******************************************************************************/

void OpenSubtitles2::Initialize()
{

}

bool OpenSubtitles2::NeedLogin()
{
    if (token.IsEmpty()) {
        return true;
    }

    return false;
}

SRESULT OpenSubtitles2::Login(const std::string& sUserName, const std::string& sPassword)
{
    SRESULT result = SR_FAILED;

    if (sUserName.empty() || sPassword.empty()) {
        return SR_UNDEFINED;
    }

    CString userAgent(UserAgent().c_str());
    CInternetSession session(userAgent);
    session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
    try {
        CHttpConnection* con = session.GetHttpConnection(_T("api.opensubtitles.com"), (DWORD)INTERNET_FLAG_SECURE);
        CString url(_T("/api/v1/login"));
        CHttpFile* httpFile = con->OpenRequest(CHttpConnection::HTTP_VERB_POST, url, NULL, 1, NULL, NULL, INTERNET_FLAG_SECURE);

        //Headers will be converted to UTF-8 but the body will be sent as-is
        //That's why everything uses CString except for the body
        CString headers(_T("Content-Type: application/json\r\n"));
        headers.AppendFormat(_T("Api-Key: %s\r\n"), APIKEY);
        headers.Append(_T("Accept: application/json\r\n"));

        std::string body(R"({ "username": ")");
        body = body + sUserName + R"(", "password": ")" + sPassword + R"(" })";


        Response response;
        if (CallAPI(httpFile, headers, body, response))
        {
            rapidjson::Document doc;
            doc.Parse(response.text.c_str());
            if (doc.IsObject() && doc.HasMember("token") && doc["token"].IsString()) {
                token = doc["token"].GetString();
                result = SR_SUCCEEDED;
            }
        } else if (response.code == 401) {
            CString msg;
            msg.FormatMessage(IDS_SUB_CREDENTIALS_ERROR, L"opensubtitles.com", static_cast<LPCWSTR>(UTF8To16(sUserName.c_str())));
            AfxMessageBox(msg, MB_ICONERROR | MB_OK);
        } else if (response.code >= 500 && response.code < 600) {
            CString msg;
            msg.Format(L"Failed to login to opensubtitles.com\n\nHTTP response code %d\n\nThis is a server error. Try again later.", response.code);
            AfxMessageBox(msg, MB_ICONERROR | MB_OK);
        } else {
            CString msg = L"Failed to login to opensubtitles.com";
            rapidjson::Document doc;
            doc.Parse(response.text.c_str());
            if (doc.IsObject() && doc.HasMember("message") && doc["message"].IsString()) {
                CString errmsg = doc["message"].GetString();
                msg.Append(L"\n\n");
                msg.Append(errmsg);
            } else {
                msg.AppendFormat(L"\n\nHTTP response code %d", response.code);
            }
            AfxMessageBox(msg, MB_ICONERROR | MB_OK);
        }

        httpFile->Close();
        delete httpFile;
        con->Close();
        delete con;
    } catch (CInternetException* pEx) {
        LOG(L"Exception %lu\n", pEx->m_dwError);
        pEx->Delete();
    }

    return result;
}

SRESULT OpenSubtitles2::Search(const SubtitlesInfo& pFileInfo)
{
    SRESULT result = SR_FAILED;

    CString userAgent(UserAgent().c_str());
    CInternetSession session(userAgent);
    session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
    try {
        CHttpConnection* con = session.GetHttpConnection(_T("api.opensubtitles.com"), (DWORD)INTERNET_FLAG_SECURE);

        CString url(_T("/api/v1/subtitles?"));

        std::list<std::string> languages = LanguagesISO6391();
        if (!languages.empty()) {
            languages.sort();
            languages.unique();
            // use alternative language codes used by the provider in case they differ from our ISO codes
            for (auto it = languages.begin(); it != languages.end(); ++it) {
                if ((*it).compare("pb") == 0) { // Portuguese Brazil
                    *it = "pt-br";
                } else if ((*it).compare("pt") == 0) { // Portuguese
                    *it = "pt-pt";
                } else if ((*it).compare("zh") == 0) { // Chinese
                    *it = "zh-cn,zh-tw";
                }
            }
            url.AppendFormat(_T("languages=%s&"), JoinContainer(languages, _T(",")).c_str());
        }
        if (!pFileInfo.fileHash.empty()) {
            url.AppendFormat(_T("moviehash=%s&"), (LPCTSTR) CString(pFileInfo.fileHash.c_str()));
        }

        CString query;
        if (pFileInfo.manualSearchString.IsEmpty())
            query = pFileInfo.fileName.c_str();
        else
            query = pFileInfo.manualSearchString;

        url.AppendFormat(_T("query=%s"), (LPCTSTR)query);

        CHttpFile* httpFile = con->OpenRequest(CHttpConnection::HTTP_VERB_GET, url, NULL, 1, NULL, NULL, INTERNET_FLAG_SECURE);

        int count = 0;
        CString headers(_T("Api-Key: "));
        headers.Append(APIKEY);
        headers.Append(_T("\r\n"));
        Response response;
        if (CallAPI(httpFile, headers, response)) {
            rapidjson::Document doc;
            doc.Parse(response.text.c_str());
            if (doc.IsObject() && doc.HasMember("data") && doc["data"].IsArray()) {
                result = SR_SUCCEEDED;
                const auto& data = doc["data"];
                for (const auto& item : data.GetArray()) {

                    SubtitlesInfo pSubtitlesInfo;

                    if (!GetOptionalValue(item, "/attributes/files/0/file_id", pSubtitlesInfo.id)) {
                        continue;
                    }
                    GetOptionalValue(item, "/attributes/files/0/file_name", pSubtitlesInfo.fileName);
                    GetOptionalValue(item, "/attributes/files/0/cd_number", pSubtitlesInfo.discNumber);
                    pSubtitlesInfo.fileExtension = "srt";
                    GetOptionalValue(item, "/attributes/language", pSubtitlesInfo.languageCode);
                    GetOptionalValue(item, "/attributes/download_count", pSubtitlesInfo.downloadCount);
                    GetOptionalValue(item, "/attributes/feature_details/movie_name", pSubtitlesInfo.title);
                    GetOptionalValue(item, "/attributes/feature_details/year", pSubtitlesInfo.year);
                    GetOptionalValue(item, "/attributes/feature_details/season_number", pSubtitlesInfo.seasonNumber);
                    GetOptionalValue(item, "/attributes/feature_details/episode_number", pSubtitlesInfo.episodeNumber);
                    GetOptionalValue(item, "/attributes/hearing_impaired", pSubtitlesInfo.hearingImpaired);
                    GetOptionalValue(item, "/attributes/feature_details/imdb_id", pSubtitlesInfo.imdbid);
                    GetOptionalValue(item, "/attributes/fps", pSubtitlesInfo.frameRate);
                    Set(pSubtitlesInfo);
                    count++;
                }
            }
            LOG(L"%d subs found\n", count);
        }
        httpFile->Close();
        delete httpFile;
        con->Close();
        delete con;
    } catch (CInternetException* pEx) {
        LOG(L"Exception %lu\n", pEx->m_dwError);
        pEx->Delete();
    }
    return result;
}

SRESULT OpenSubtitles2::Download(SubtitlesInfo& pSubtitlesInfo)
{
    SRESULT result = SR_FAILED;

    CString userAgent(UserAgent().c_str());
    CInternetSession session(userAgent);
    session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 10000);
    try {
        CHttpConnection* con = session.GetHttpConnection(_T("api.opensubtitles.com"), (DWORD)INTERNET_FLAG_SECURE);
        CString url(_T("/api/v1/download"));
        CHttpFile* httpFile = con->OpenRequest(CHttpConnection::HTTP_VERB_POST, url, NULL, 1, NULL, NULL, INTERNET_FLAG_SECURE);

        CString headers(_T("Accept: application/json\r\n")); 
        headers.AppendFormat(_T("Api-Key: %s\r\n"), APIKEY);
	    headers.Append(_T("Content-Type: application/json\r\n"));
        if (!token.IsEmpty()) {
            headers.AppendFormat(_T("Authorization: Bearer %s\r\n"), (LPCTSTR)token);
        }

        std::string body(R"({ "file_id": )");
        body += pSubtitlesInfo.id;
        body += " }";

        Response response;
        if (CallAPI(httpFile, headers, body, response)) {
            rapidjson::Document doc;
            doc.Parse(response.text.c_str());
            if (!doc.HasParseError()) {
                if (doc.HasMember("file_name") && doc["file_name"].IsString())
                {
                    std::string downloadLink = doc["link"].GetString();
                    LOG(LOG_INPUT, downloadLink.c_str());
                    result = DownloadInternal(downloadLink, "", pSubtitlesInfo.fileContents);
                }
            }
        } else {
            m_dwLastStatusCode = response.code;
        }
        httpFile->Close();
        delete httpFile;
        con->Close();
        delete con;
    } catch (CInternetException* pEx) {
        LOG(L"Exception %lu\n", pEx->m_dwError);
        pEx->Delete();
    }
    return result;
}

SRESULT OpenSubtitles2::LogOut()
{
    SRESULT result = SR_FAILED;

    if (!token.IsEmpty()) {
        CString userAgent(UserAgent().c_str());
        CInternetSession session(userAgent);
        session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
        try {
            CHttpConnection* con = session.GetHttpConnection(_T("api.opensubtitles.com"), (DWORD)INTERNET_FLAG_SECURE);
            CString url(_T("/api/v1/logout"));
            CHttpFile* httpFile = con->OpenRequest(CHttpConnection::HTTP_VERB_DELETE, url, NULL, 1, NULL, NULL, INTERNET_FLAG_SECURE);

            CString headers(_T("Accept: application/json\r\n"));
            headers.AppendFormat(_T("Api-Key: %s\r\n"), (LPCTSTR)APIKEY);
            headers.AppendFormat(_T("Authorization: Bearer %s\r\n"), (LPCTSTR)token);
            Response response;
            if (CallAPI(httpFile, headers, response)) {
                result = SR_SUCCEEDED;
            }
            httpFile->Close();
            delete httpFile;
            con->Close();
            delete con;
        } catch (CInternetException* pEx) {
            LOG(L"Exception %lu\n", pEx->m_dwError);
            pEx->Delete();
        }
    }

    token.Empty();
    
    return result;
}

SRESULT OpenSubtitles2::Hash(SubtitlesInfo& pFileInfo)
{
    pFileInfo.fileHash = StringFormat("%016I64x", GenerateOSHash(pFileInfo));
    LOG(LOG_OUTPUT, pFileInfo.fileHash.c_str());
    return SR_SUCCEEDED;
}

bool OpenSubtitles2::GetOptionalValue(const rapidjson::Value& node, const char* path, std::string& result)
{
    bool success = false;
    const rapidjson::Value* foundNode = rapidjson::Pointer(path).Get(node);
    if (foundNode) {
        if (foundNode->IsString()) {
            result = foundNode->GetString();
            success = true;
        } else if (foundNode->IsInt64()) {
            result = std::to_string(foundNode->GetInt64());
            success = true;
        }
    }
    return success;
}

bool OpenSubtitles2::GetOptionalValue(const rapidjson::Value& node, const char* path, int& result)
{
    bool success = false;
    const rapidjson::Value* foundNode = rapidjson::Pointer(path).Get(node);
    if (foundNode) {
        if (foundNode->IsInt()) {
            result = foundNode->GetInt();
            success = true;
        } else if (foundNode->IsBool()) {
            result = foundNode->GetBool() ? TRUE : FALSE;
            success = true;
        }
    }
    return success;
}

bool OpenSubtitles2::GetOptionalValue(const rapidjson::Value& node, const char* path, double& result)
{
    bool success = false;
    const rapidjson::Value* foundNode = rapidjson::Pointer(path).Get(node);
    if (foundNode) {
        if (foundNode->IsDouble()) {
            result = foundNode->GetDouble();
            success = true;
        }
    }
    return success;
}
bool OpenSubtitles2::CallAPI(CHttpFile* httpFile, CString& headers, Response& response)
{
    httpFile->SendRequest(headers);
    return CallAPIResponse(httpFile, response);
}


bool OpenSubtitles2::CallAPI(CHttpFile* httpFile, CString& headers, std::string& body, Response& response)
{
    httpFile->SendRequest(headers, body.data(), static_cast<DWORD>(body.size()));
    return CallAPIResponse(httpFile, response);
}

bool OpenSubtitles2::CallAPIResponse(CHttpFile* httpFile, Response& response)
{
    httpFile->QueryInfoStatusCode(response.code);

    auto size = httpFile->GetLength();
    while (size > 0)
    {
        std::string temp;
        temp.resize(size);
        httpFile->Read(temp.data(), size);
        response.text += temp;
        size = httpFile->GetLength();
    }
    if (response.code != 200)
    {
        LOG(LOG_ERROR, (std::string("Server returned: ") + std::to_string(response.code) + " with message: " + response.text).c_str());
        return false;
    }
    return true;
}


const std::set<std::string>& OpenSubtitles2::Languages() const
{
    static std::set<std::string> result;
    result = { "ab", "af", "am", "an", "ar", "as", "at", "az", "be", "bg", "bn", "br", "bs", "ca", "cy", "cs", "da", "de", "ea", "el", "en", "eo", "es", "et", "eu", "ex", "fa", "fi", "fr", "ga", "gd", "gl", "he", "hi", "hr", "hu", "hy", "ia", "id", "ig", "is", "it", "ja", "ka", "kk", "km", "kn", "ko", "ku", "lb", "lt", "lv", "ma", "me", "mk", "ml", "mn", "mr", "ms", "my", "ne", "nl", "no", "nv", "oc", "or", "pb", "pl", "pm", "pr", "ps", "pt", "ro", "ru", "sd", "se", "si", "sk", "sl", "so", "sp", "sq", "sr", "sv", "sw", "sx", "sy", "ta", "te", "th", "tl", "tr", "tp", "tt", "uk", "ur", "uz", "vi", "ze", "zh", "zt" };
    return result;
}

/******************************************************************************
** podnapisi
******************************************************************************/

SRESULT podnapisi::Login(const std::string& sUserName, const std::string& sPassword)
{
    //TODO: implement
    return SR_UNDEFINED;
}

/*
UPDATED
https://www.podnapisi.net/forum/viewtopic.php?f=62&t=26164#p212652
RESULTS ------------------------------------------------
"/sXML/1/"  //Reply in XML format
"/page//"   //Return nth page of results
SEARCH -------------------------------------------------
"/sT/1/"    //Type: -1=all, 0=movies, 1=series, don't specify for auto detection
"/sAKA/1/"  //Include movie title aliases
"/sM//"     //Movie id from www.omdb.si
"/sK//"     //Title url encoded text
"/sY//"     //Year number
"/sTS//"    //Season number
"/sTE//"    //Episode number
"/sR//"     //Release name url encoded text
"/sJ/0/"    //Languages (old integer IDs), comma delimited, 0=all
"/sL/en/"   //Languages in ISO ISO codes (exception are sr-latn and pt-br), comma delimited
"/sEH//"    //Exact hash match (OSH)
"/sMH//"    //Movie hash (OSH)
SEARCH ADDITIONAL --------------------------------------
"/sFT/0/"   //Subtitles Format: 0=all, 1=MicroDVD, 2=SAMI, 3=SSA, 4=SubRip, 5=SubViewer 2.0, 6=SubViewer, 7=MPSub, 8=Advanced SSA, 9=DVDSubtitle, 10=TMPlayer, 11=MPlayer2
"/sA/0/"    //Search subtitles by user id, 0=all
"/sI//"     //Search subtitles by subtitle id
SORTING ------------------------------------------------
"/sS//"     //Sorting field: movie, year, fps, language, downloads, cds, username, time, rating
"/sO//"     //Soring order: asc, desc
FILTERS ------------------------------------------------
"/sOE/1/"   //Subtitles for extended edition only
"/sOD/1/"   //Subtitles suitable for DVD only
"/sOH/1/"   //Subtitles for high-definition video only
"/sOI/1/"   //Subtitles for hearing impaired only
"/sOT/1/"   //Technically correct only
"/sOL/1/"   //Grammatically correct only
"/sOA/1/"   //Author subtitles only
"/sOCS/1/"  //Only subtitles for a complete season
UNKNOWN ------------------------------------------------
"/sH//"     //Search subtitles by video file hash ??? (not working for me)
*/

SRESULT podnapisi::Search(const SubtitlesInfo& pFileInfo)
{
    SRESULT searchResult = SR_UNDEFINED;
    int page = 1, pages = 1, results = 0;
    do {
        CheckAbortAndReturn();

        std::string url(Url() + "/ppodnapisi/search");
        url += "?sXML=1";
        url += "&sAKA=1";

        if (pFileInfo.manualSearchString.IsEmpty()) {
            std::string search(pFileInfo.title);
            if (!pFileInfo.country.empty()) {
                search += " " + pFileInfo.country;
            }
            search = std::regex_replace(search, std::regex(" and | *[!?&':] *", RegexFlags), " ");

            if (!search.empty()) {
                url += "&sK=" + UrlEncode(search.c_str());
            }
            url += (pFileInfo.year != -1 ? "&sY=" + std::to_string(pFileInfo.year) : "");
            url += (pFileInfo.seasonNumber != -1 ? "&sTS=" + std::to_string(pFileInfo.seasonNumber) : "");
            url += (pFileInfo.episodeNumber != -1 ? "&sTE=" + std::to_string(pFileInfo.episodeNumber) : "");
            url += "&sMH=" + pFileInfo.fileHash;
            //url += "&sR=" + UrlEncode(pFileInfo.fileName.c_str());
        } else {
            CT2CA pszConvertedAnsiString(pFileInfo.manualSearchString);
            std::string search(pszConvertedAnsiString);
            search = std::regex_replace(search, std::regex(" and | *[!?&':] *", RegexFlags), " ");

            if (!search.empty()) {
                url += "&sK=" + UrlEncode(search.c_str());
            }
        }
        std::list<std::string> languages = LanguagesISO6391();
        if (!languages.empty()) {
            languages.sort();
            languages.unique();
            // use alternative language codes used by the provider in case they differ from our ISO codes
            for (auto it = languages.begin(); it != languages.end(); ++it) {
                if ((*it).compare("pb") == 0) { // Portuguese Brazil
                    *it = "pt-br";
                }
            }
            url += "&sL=" + JoinContainer(languages, ",");
        }
        url += "&page=" + std::to_string(page);
        LOG(LOG_INPUT, url.c_str());

        std::string data;
        searchResult = DownloadInternal(url, "", data);

        using namespace tinyxml2;

        tinyxml2::XMLDocument dxml;
        if (dxml.Parse(data.c_str()) == XML_SUCCESS) {

            auto GetChildElementText = [&](XMLElement * pElement, const char* value) -> std::string {
                std::string str;
                XMLElement* pChildElement = pElement->FirstChildElement(value);
                if (pChildElement != nullptr)
                {
                    auto pText = pChildElement->GetText();
                    if (pText != nullptr) {
                        str = pText;
                    }
                }
                return str;
                };

            XMLElement* pRootElmt = dxml.FirstChildElement("results");
            if (pRootElmt) {
                XMLElement* pPaginationElmt = pRootElmt->FirstChildElement("pagination");
                if (pPaginationElmt) {
                    page = atoi(GetChildElementText(pPaginationElmt, "current").c_str());
                    pages = atoi(GetChildElementText(pPaginationElmt, "count").c_str());
                    results = atoi(GetChildElementText(pPaginationElmt, "results").c_str());
                }
                // 30 results per page
                if (page > 1) {
                    return SR_TOOMANY;
                }

                if (results > 0) {
                    XMLElement* pSubtitleElmt = pRootElmt->FirstChildElement("subtitle");

                    while (pSubtitleElmt) {
                        CheckAbortAndReturn();

                        SubtitlesInfo pSubtitlesInfo;

                        pSubtitlesInfo.id = GetChildElementText(pSubtitleElmt, "pid");
                        pSubtitlesInfo.title = HtmlSpecialCharsDecode(GetChildElementText(pSubtitleElmt, "title").c_str());

                        std::string year = GetChildElementText(pSubtitleElmt, "year");
                        pSubtitlesInfo.year = year.empty() ? -1 : atoi(year.c_str());

                        pSubtitlesInfo.url = GetChildElementText(pSubtitleElmt, "url");
                        std::string format = GetChildElementText(pSubtitleElmt, "format");
                        pSubtitlesInfo.fileExtension = (format == "SubRip" || format == "N/A") ? "srt" : format;

                        pSubtitlesInfo.languageCode = podnapisi_languages[atoi(GetChildElementText(pSubtitleElmt, "languageId").c_str())].code;
                        pSubtitlesInfo.languageName = GetChildElementText(pSubtitleElmt, "languageName");
                        pSubtitlesInfo.seasonNumber = atoi(GetChildElementText(pSubtitleElmt, "tvSeason").c_str());
                        pSubtitlesInfo.episodeNumber = atoi(GetChildElementText(pSubtitleElmt, "tvEpisode").c_str());
                        pSubtitlesInfo.discCount = atoi(GetChildElementText(pSubtitleElmt, "cds").c_str());
                        pSubtitlesInfo.discNumber = pSubtitlesInfo.discCount;

                        std::string flags = GetChildElementText(pSubtitleElmt, "flags");
                        pSubtitlesInfo.hearingImpaired = (flags.find("n") != std::string::npos) ? TRUE : FALSE;
                        pSubtitlesInfo.corrected = (flags.find("r") != std::string::npos) ? -1 : 0;
                        pSubtitlesInfo.downloadCount = atoi(GetChildElementText(pSubtitleElmt, "downloads").c_str());
                        pSubtitlesInfo.imdbid = GetChildElementText(pSubtitleElmt, "movieId");
                        pSubtitlesInfo.frameRate = atof(GetChildElementText(pSubtitleElmt, "fps").c_str());

                        XMLElement* pReleasesElem = pSubtitleElmt->FirstChildElement("releases");
                        if (pReleasesElem) {
                            XMLElement* pReleaseElem = pReleasesElem->FirstChildElement("release");

                            while (pReleaseElem) {
                                auto pText = pReleaseElem->GetText();

                                if (!pText) {
                                    continue;
                                }

                                pSubtitlesInfo.releaseNames.emplace_back(pText);

                                if (pSubtitlesInfo.fileName.empty() || pFileInfo.fileName.find(pText) != std::string::npos) {
                                    pSubtitlesInfo.fileName = pText;
                                    pSubtitlesInfo.fileName += "." + pSubtitlesInfo.fileExtension;
                                }
                                pReleaseElem = pReleaseElem->NextSiblingElement();
                            }
                        }

                        if (pSubtitlesInfo.fileName.empty()) {
                            std::string str = pSubtitlesInfo.title;
                            if (!year.empty()) {
                                str += " " + year;
                            }
                            if (pSubtitlesInfo.seasonNumber > 0) {
                                str += StringFormat(" S%02d", pSubtitlesInfo.seasonNumber);
                            }
                            if (pSubtitlesInfo.episodeNumber > 0) {
                                str += StringFormat("%sE%02d", (pSubtitlesInfo.seasonNumber > 0) ? "" : " ", pSubtitlesInfo.episodeNumber);
                            }
                            str += GUESSED_NAME_POSTFIX;
                            pSubtitlesInfo.fileName = str;
                        }

                        Set(pSubtitlesInfo);
                        pSubtitleElmt = pSubtitleElmt->NextSiblingElement();
                    }
                }
            }
        }
    } while (page++ < pages);

    return searchResult;
}

SRESULT podnapisi::Hash(SubtitlesInfo& pFileInfo)
{
    pFileInfo.fileHash = StringFormat("%016I64x", GenerateOSHash(pFileInfo));
    LOG(LOG_OUTPUT, pFileInfo.fileHash.c_str());
    return SR_SUCCEEDED;
}

SRESULT podnapisi::Download(SubtitlesInfo& pSubtitlesInfo)
{
    std::string url = StringFormat("%s/subtitles/%s/download", Url().c_str(), pSubtitlesInfo.id.c_str());
    LOG(LOG_INPUT, url.c_str());
    return DownloadInternal(url, "", pSubtitlesInfo.fileContents);
}

const std::set<std::string>& podnapisi::Languages() const
{
    static std::once_flag initialized;
    static std::set<std::string> result;

    std::call_once(initialized, [this]() {
        for (const auto& iter : podnapisi_languages) {
            if (strlen(iter.code)) {
                result.emplace(iter.code);
            }
        }
        });
    return result;
}

/******************************************************************************
** Napisy24
******************************************************************************/

#if 0
SRESULT Napisy24::Search(const SubtitlesInfo& pFileInfo)
{
    if (!pFileInfo.manualSearchString.IsEmpty()) {
        return SR_FAILED; //napisys24 does not support manual search
    }
    stringMap headers({
        { "User-Agent", UserAgent() },
        { "Content-Type", "application/x-www-form-urlencoded" }
        });
    std::string data;
    std::string url = Url() + "/run/CheckSubAgent.php";
    std::string content = "postAction=CheckSub";
    content += "&ua=mpc-hc";
    content += "&ap=mpc-hc";
    content += "&fh=" + pFileInfo.fileHash;
    content += "&fs=" + std::to_string(pFileInfo.fileSize);
    content += "&fn=" + pFileInfo.fileName;

    LOG(LOG_INPUT, std::string(url + "?" + content).c_str());
    StringUpload(url, headers, content, data);

    if (data.length() < 4) {
        return SR_FAILED;
    }

    // Get status
    std::string status = data.substr(0, 4);
    if (status != "OK-2" && status != "OK-3") {
        return SR_FAILED;
    }
    data.erase(0, 5);

    size_t infoEnd = data.find("||");
    if (infoEnd == std::string::npos) {
        return SR_FAILED;
    }

    // Search already returns whole file
    SubtitlesInfo subtitleInfo;
    subtitleInfo.fileContents = data.substr(infoEnd + 2);
    subtitleInfo.languageCode = "pl"; // API doesn't support other languages yet.

    // Remove subtitle data
    data.erase(infoEnd);

    std::unordered_map<std::string, std::string> subtitleInfoMap;
    std::istringstream stringStream(data);
    std::string entry;
    while (std::getline(stringStream, entry, '|')) {
        auto delimPos = entry.find(':');
        if (delimPos == std::string::npos) {
            continue;
        }
        std::string key = entry.substr(0, delimPos);
        if (entry.length() <= delimPos + 1) {
            continue;
        }
        std::string value = entry.substr(delimPos + 1);
        subtitleInfoMap[key] = value;
    }

    subtitleInfo.url = Url() + "/komentarze?napisId=" + subtitleInfoMap["napisId"];
    subtitleInfo.title = subtitleInfoMap["ftitle"];
    subtitleInfo.imdbid = subtitleInfoMap["fimdb"];

    auto it = subtitleInfoMap.find("fyear");
    if (it != subtitleInfoMap.end()) {
        subtitleInfo.year = std::stoi(it->second);
    }

    it = subtitleInfoMap.find("fps");
    if (it != subtitleInfoMap.end()) {
        subtitleInfo.frameRate = std::stod(it->second);
    }

    int hour, minute, second;
    if (sscanf_s(subtitleInfoMap["time"].c_str(), "%02d:%02d:%02d", &hour, &minute, &second) == 3) {
        subtitleInfo.lengthMs = ((hour * 60 + minute) * 60 + second) * 1000;
    }

    subtitleInfo.fileName = pFileInfo.fileName + "." + pFileInfo.fileExtension;
    subtitleInfo.discNumber = 1;
    subtitleInfo.discCount = 1;

    Set(subtitleInfo);

    return SR_SUCCEEDED;
}

SRESULT Napisy24::Hash(SubtitlesInfo& pFileInfo)
{
    pFileInfo.fileHash = StringFormat("%016I64x", GenerateOSHash(pFileInfo));
    LOG(LOG_OUTPUT, pFileInfo.fileHash.c_str());
    return SR_SUCCEEDED;
}

SRESULT Napisy24::Download(SubtitlesInfo& subtitlesInfo)
{
    LOG(LOG_INPUT, subtitlesInfo.url.c_str());
    return subtitlesInfo.fileContents.empty() ? SR_FAILED : SR_SUCCEEDED;
}

const std::set<std::string>& Napisy24::Languages() const
{
    static std::set<std::string> result = {"pl"};
    return result;
}
#endif
