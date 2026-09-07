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
#include <map>
#include <mutex>
#include <vector>

#include "CastHlsStream.h"

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

    // --- HLS streaming resources ---
    // A second way to serve media, beside the single registered file: while a
    // transcode runs, its segmenter publishes a playlist, an init segment and
    // media segments under their own randomized /cast/<token>/hls/ prefix as
    // named resources built from parts (memory blobs and ranges of the raw
    // fragmented MP4). A resource's total length is known before it is served,
    // so every response carries an exact Content-Length. Names are matched
    // exactly against what was published; nothing is ever resolved against the
    // filesystem from request input.
    void BeginHls(const CString& rawFilePath); // new random token; the table starts empty
    // http://ip:port/cast/<token>/hls/media.m3u8, empty when HLS is not active
    CStringA GetHlsURLForHost(const CStringA& localIp) const;
    // Publishes (or replaces) name. Thread-safe; requests parked waiting for
    // name are released with the new content.
    void PublishResource(const CStringA& name, const CStringA& mime,
                         std::vector<CCastHlsStream::Part> parts, ULONGLONG totalLen);
    // Current and future requests for name get a 404: how a segment the
    // transcode will never produce is kept from parking a request forever.
    void FailResource(const CStringA& name);
    // Clears the table and the prefix; parked requests are answered 404.
    void EndHls();

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
    // Everything under the HLS prefix: lookup, parking an unpublished name,
    // and serving the published parts (memory blobs and file ranges).
    void HandleHlsRequest(Client& client, const CStringA& header, const CStringA& method,
                          const CStringA& target, const CStringA& targetPath, bool keepAlive);
    void SendSimpleResponse(Client& client, int status, const CStringA& statusText,
                            const CStringA& extraHeaders, bool closeConnection);
    void QueueResponse(Client& client, const CStringA& headerBytes);
    bool EnsureFileOpen(Client& client, const CString& filePath);
    bool RefillOutBuf(Client& client);
    bool RefillFromParts(Client& client); // HLS body: the current part of client.parts
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

    // HLS resource table, same guard as the file registration. Entries appear
    // when the segmenter publishes them; a name that is absent while the
    // prefix is set is "not yet published", which parks the request rather
    // than answering 404.
    struct HlsResource {
        CStringA mime;
        std::vector<CCastHlsStream::Part> parts;
        ULONGLONG totalLen = 0;
        bool failed = false;
    };
    std::map<CStringA, HlsResource> m_hlsResources; // keyed by name under the prefix
    CStringA m_hlsPrefix;  // "/cast/<hex>/hls/", empty when HLS is not active
    CString m_hlsFilePath; // the raw fragmented MP4 the file-range parts read from

    SOCKET m_listenSocket = INVALID_SOCKET;
    bool m_bWsaInitialized = false;
    HANDLE m_hThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
    // auto-reset; PublishResource/FailResource/EndHls wake the worker so it can
    // re-run the requests parked on what just changed
    HANDLE m_hWakeEvent = nullptr;
};
