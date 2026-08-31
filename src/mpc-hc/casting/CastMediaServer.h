/*
 * (C) 2026 see Authors.txt
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

#include <winsock2.h>
#include <list>
#include <mutex>
#include <vector>

// Phase 3 of Chromecast support: a minimal local HTTP/1.1 server that serves
// a single registered media file to the cast device for direct playback.
// Range requests are fully supported (the device probes moov-at-end MP4 files
// with large-offset ranges and implements seeking through them), the URL path
// is randomized per registered file (devices cache by URL), and connections
// can be restricted to the cast device's address. Later phases add the UI
// wiring.
class CCastMediaServer
{
public:
    CCastMediaServer();
    ~CCastMediaServer();

    CCastMediaServer(const CCastMediaServer&) = delete;
    CCastMediaServer& operator=(const CCastMediaServer&) = delete;

    // The port the server tries first, falling back to an ephemeral one when
    // it is taken. Both cast targets serve from their own instance, so this is
    // shared and set once from the settings.
    static UINT preferredPort;

    bool Start();
    void Stop();
    bool IsRunning() const { return m_hThread != nullptr; }
    UINT GetPort() const; // actual listening port; may differ from preferredPort if it was taken

    // Registers the single file to serve under a freshly randomized URL path;
    // any previously registered path stops working. Thread-safe.
    // contentFeatures is what a DLNA renderer is answered when it asks what
    // the resource is; the caller passes exactly what it also put in the DIDL
    // metadata, and leaves it empty when it has nothing to say (Chromecast).
    void SetFile(const CString& filePath, const CStringA& mime, const CStringA& contentFeatures = CStringA());
    void ClearFile(); // unregister; all requests get a 404

    // Full http://ip:port/path URL for the registered file, empty when no
    // file is registered. Pass CCastSession::GetLocalAddress() for localIp,
    // the address the device can reach us at on multi-NIC systems.
    CStringA GetURLForHost(const CStringA& localIp) const;

    // When an allowed peer is set, connections from any other address are
    // closed immediately. The caller sets this to the cast device's IP so the
    // server cannot be used as a general LAN file server.
    void SetAllowedPeer(const CStringA& ip);
    void ClearAllowedPeer();

    // Whether the file can be direct-played by a Chromecast without remuxing.
    // MVP heuristic: container extension whitelist, to grow into codec
    // inspection later. DLNA renderers accept far more than this, so they
    // bring their own rule (CDlnaTarget::CanCastFile).
    static bool IsCastableFile(const CString& path);
    static CStringA MimeForFile(const CString& path);

    // The table MimeForFile() answers from. Exposed so that what a device says
    // it accepts can be logged against the file types casting knows instead of
    // against a list written down a second time; nothing is decided by it.
    struct FileType {
        const char* ext;
        const char* mime;
    };
    static const std::vector<FileType>& KnownFileTypes();

    // A media URL with its random path token replaced by a placeholder, for a
    // log that gets pasted in public: the token is the only thing keeping the
    // served file out of reach of anything that was not handed the URL.
    static CStringA MaskURLToken(const CStringA& url);

    // The "contentFeatures.dlna.org" value answered to a DLNA renderer that
    // asks for one and no profile could be named: byte-range seeking, no
    // transcoding, streaming transfer. The DIDL metadata a renderer is handed
    // has to carry the same string.
    static const char* const dlnaContentFeatures;

private:
    struct Client; // per-connection state, defined in the .cpp

    static DWORD WINAPI StaticThreadProc(LPVOID lpParam);
    DWORD ThreadProc();

    SOCKET OpenListenSocket();
    void AcceptClients(std::list<Client>& clients);
    void OnClientEvent(Client& client);
    void ProcessRequests(Client& client);
    void HandleRequest(Client& client, const CStringA& header);
    void SendSimpleResponse(Client& client, int status, const CStringA& statusText,
                            const CStringA& extraHeaders, bool closeConnection);
    void QueueResponse(Client& client, const CStringA& headerBytes);
    bool EnsureFileOpen(Client& client, const CString& filePath);
    bool RefillOutBuf(Client& client);
    void PumpSend(Client& client);
    static void CloseClient(Client& client);

    // shared state, guarded by m_mutex
    mutable std::mutex m_mutex;
    CString m_filePath;    // registered file, empty = nothing to serve
    CStringA m_mime;
    CStringA m_contentFeatures; // empty = answer dlnaContentFeatures
    CStringA m_urlPath;    // randomized "/cast/<hex>/media.<ext>" path
    CStringA m_allowedPeer; // empty = allow any peer
    UINT m_port = 0;

    SOCKET m_listenSocket = INVALID_SOCKET;
    bool m_bWsaInitialized = false;
    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
};
