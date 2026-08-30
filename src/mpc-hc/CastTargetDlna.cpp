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

#include "stdafx.h"
#include "CastTargetDlna.h"
#include <PathUtils.h>
#include <algorithm>

#define AVTRANSPORT_SERVICE       "urn:schemas-upnp-org:service:AVTransport:1"
#define RENDERINGCONTROL_SERVICE  "urn:schemas-upnp-org:service:RenderingControl:1"

// DLNA has no push notifications, so the transport is polled while a session
// is up. Two seconds is what the seekbar needs and what a renderer's little
// HTTP server tolerates comfortably.
#define POLL_INTERVAL_MS      2000ull
#define MAX_POLL_FAILURES     3    // consecutive, before the session is given up
#define STOP_MEDIA_TIMEOUT_MS 1000 // how long StopCasting() waits for the STOP to go out
#define SEEK_END_MARGIN_SEC   1.0  // kept clear of the end, where a seek is refused
// How far the position may be carried past the last reading the device gave
// us. A device that stops answering GetPositionInfo while still reporting
// PLAYING must not have its seekbar run away on an extrapolation nothing
// renews any more.
#define MAX_EXTRAPOLATION_MS  (3ull * POLL_INTERVAL_MS)

namespace
{
    CStringA UpnpClassFor(const CStringA& mime)
    {
        if (mime.Left(6).CompareNoCase("audio/") == 0) {
            return "object.item.audioItem.musicTrack";
        }
        if (mime.Left(6).CompareNoCase("image/") == 0) {
            return "object.item.imageItem.photo";
        }
        return "object.item.videoItem";
    }

    // Containers a renderer that does not answer GetProtocolInfo is assumed to
    // handle. Unlike the Chromecast whitelist this is permissive: DLNA
    // renderers are usually TVs or receivers with a broad demuxer set, and the
    // sink list is what narrows it down whenever a device provides one.
    bool IsCommonRendererFormat(const CStringA& mime)
    {
        static const char* const accepted[] = {
            "video/mp4", "video/webm", "video/x-matroska", "video/x-msvideo",
            "video/mp2t", "video/mpeg", "video/quicktime",
            "audio/mpeg", "audio/mp4", "audio/flac", "audio/wav", "audio/aac", "audio/ogg",
        };
        return std::any_of(std::cbegin(accepted), std::cend(accepted), [&](const char* type) {
            return mime.CompareNoCase(type) == 0;
        });
    }

    // The DLNA profile the content matches, empty when none can be claimed.
    // A renderer handed a profile name the stream then fails to live up to is
    // worse off than one handed no name at all, so every rule below wants
    // positive evidence from the local graph and everything else falls through
    // to no profile: containers DLNA never standardized (Matroska, WebM, AVI,
    // QuickTime), codecs outside the profile's definition, and resolutions
    // beyond the ones the profile covers.
    CStringA DlnaProfileName(const CString& path, const CStringA& mime, const CastMediaInfo& info)
    {
        const int width = info.width, height = info.height;

        if (mime.CompareNoCase("video/mp4") == 0) {
            // every standard AVC MP4 profile pairs the video with AAC
            if (info.video != CastMediaInfo::Video::H264 || info.audio != CastMediaInfo::Audio::AAC
                    || width <= 0 || height <= 0) {
                return CStringA();
            }
            if (width <= 720 && height <= 576) {
                return "AVC_MP4_BL_L3L_SD_AAC";
            }
            if (width <= 1280 && height <= 720) {
                return "AVC_MP4_MP_HD_720p_AAC";
            }
            if (width <= 1920 && height <= 1080) {
                // DLNA never defined a 1080p AVC MP4 profile, and the 1080i one
                // is what every media server names for 1080 content whatever
                // its scan type actually is
                return "AVC_MP4_MP_HD_1080i_AAC";
            }
            return CStringA(); // 4K and up: no standard profile exists
        }
        if (mime.CompareNoCase("video/mpeg") == 0) {
            // an MPEG program stream; the two profiles differ only in the
            // broadcast system, which the picture height gives away
            if (info.video != CastMediaInfo::Video::MPEG2 || width > 720) {
                return CStringA();
            }
            if (height == 480) {
                return "MPEG_PS_NTSC";
            }
            if (height == 576) {
                return "MPEG_PS_PAL";
            }
            return CStringA();
        }
        if (mime.CompareNoCase("video/mp2t") == 0) {
            // Only the plain .ts form is obvious: it holds 188-byte packets
            // with no timestamp, which is what the _ISO profiles mean, while
            // .m2ts and .mts carry 192-byte timestamped ones and belong to a
            // different family. H.264 in a transport stream has its own matrix
            // of profiles and is left unnamed.
            if (info.video != CastMediaInfo::Video::MPEG2 || width > 720
                    || PathUtils::FileExt(path).CompareNoCase(_T(".ts")) != 0) {
                return CStringA();
            }
            if (height == 480) {
                return "MPEG_TS_SD_NA_ISO";
            }
            if (height == 576) {
                return "MPEG_TS_SD_EU_ISO";
            }
            return CStringA();
        }
        if (mime.CompareNoCase("audio/mpeg") == 0) {
            // the MP3 profile is fixed at three sampling rates in one or two
            // channels; everything else is MP3X, which few renderers accept
            if (info.audio == CastMediaInfo::Audio::MP3 && info.channels >= 1 && info.channels <= 2
                    && (info.sampleRate == 32000 || info.sampleRate == 44100 || info.sampleRate == 48000)) {
                return "MP3";
            }
            return CStringA();
        }
        if (mime.CompareNoCase("audio/mp4") == 0) {
            return info.audio == CastMediaInfo::Audio::AAC ? CStringA("AAC_ISO") : CStringA();
        }
        if (mime.CompareNoCase("audio/aac") == 0) {
            return info.audio == CastMediaInfo::Audio::AAC ? CStringA("AAC_ADTS") : CStringA();
        }
        if (mime.CompareNoCase("audio/x-ms-wma") == 0) {
            // WMAFULL covers plain WMA; Pro and Lossless are a different codec
            // with no profile of ours, and the graph tells the three apart
            if (info.audio == CastMediaInfo::Audio::WMA && info.channels >= 1 && info.channels <= 2
                    && info.sampleRate > 0 && info.sampleRate <= 48000) {
                return "WMAFULL";
            }
            return CStringA();
        }
        // Two audio containers deliberately get no profile. DLNA's LPCM
        // profile means raw big-endian 16-bit samples served as audio/L16,
        // whereas a .wav is a RIFF file of little-endian ones and goes out
        // byte for byte, so naming LPCM for it would be a lie. FLAC has no
        // standard profile at all.
        return CStringA();
    }
}

CDlnaTarget::~CDlnaTarget()
{
    StopWorker();
    m_server.Stop();
    m_discovery.Stop();
}

bool CDlnaTarget::StartDiscovery()
{
    return m_discovery.Start();
}

void CDlnaTarget::StopDiscovery()
{
    m_discovery.Stop();
}

CastTargetDevice CDlnaTarget::ToTargetDevice(const DlnaDevice& dev)
{
    CastTargetDevice d;
    d.protocol = CastProtocol::Dlna;
    d.name = dev.friendlyName;
    d.id = dev.udn;
    d.address = dev.ipAddress;
    d.location = dev.location;
    d.formats = CString(dev.sinkProtocolInfo);
    d.supportsVideo = dev.supportsVideo;
    d.supportsAudio = dev.supportsAudio;
    return d;
}

std::vector<CastTargetDevice> CDlnaTarget::GetDevices()
{
    std::vector<CastTargetDevice> devices;
    for (const DlnaDevice& dev : m_discovery.GetDevices()) {
        devices.emplace_back(ToTargetDevice(dev));
    }
    return devices;
}

bool CDlnaTarget::ProbeAddress(CastProtocol protocol, const CString& address, UINT port,
                               DWORD timeoutMs, CastTargetDevice& device)
{
    DlnaDevice dev;
    if (protocol != CastProtocol::Dlna || address.IsEmpty()
            || !CDlnaDiscovery::ProbeAddress(address, port, timeoutMs, dev)) {
        return false;
    }
    device = ToTargetDevice(dev);
    device.port = port;
    return true;
}

bool CDlnaTarget::SearchById(const CString& udn, DWORD timeoutMs, DlnaDevice& device)
{
    const bool wasRunning = m_discovery.IsRunning();
    if (!wasRunning && !m_discovery.Start()) {
        return false;
    }

    bool found = false;
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        for (const DlnaDevice& dev : m_discovery.GetDevices()) {
            if (dev.udn == udn && !dev.avTransportURL.IsEmpty()) {
                device = dev;
                found = true;
                break;
            }
        }
        if (found || GetTickCount64() >= deadline) {
            break;
        }
        Sleep(150);
    }

    if (!wasRunning) {
        m_discovery.Stop(); // the copy above survives it
    }
    return found;
}

void CDlnaTarget::SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hNotifyWnd = hNotifyWnd;
    m_stateMsg = stateMsg;
}

bool CDlnaTarget::Connect(const CString& deviceId)
{
    if (m_casting) {
        return false;
    }

    for (const DlnaDevice& dev : m_discovery.GetDevices()) {
        if (dev.udn != deviceId || dev.avTransportURL.IsEmpty()) {
            continue;
        }
        return StartSession(dev, dev.friendlyName);
    }
    return false; // the device is gone from the discovery snapshot
}

bool CDlnaTarget::ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs)
{
    if (m_casting) {
        return false;
    }

    // The description is asked for where the device was last seen; only when
    // nothing answers there is a full search worth the wait. Either way what
    // the device says about itself is written back, so the saved entry follows
    // the device instead of rotting.
    DlnaDevice dev;
    bool found = false;
    if (!saved.location.IsEmpty()) {
        found = CDlnaDiscovery::ProbeLocation(saved.location, saved.address, dev);
    } else if (!saved.address.IsEmpty()) {
        found = CDlnaDiscovery::ProbeAddress(saved.address, saved.port, directMs, dev);
    }
    if (found && !saved.id.IsEmpty() && dev.udn != saved.id) {
        found = false; // somebody else lives at that address now
    }
    if (!found && !saved.id.IsEmpty()) {
        found = SearchById(saved.id, searchMs, dev);
    }
    if (!found || dev.avTransportURL.IsEmpty()) {
        return false;
    }

    saved.address = dev.ipAddress;
    saved.location = dev.location;
    if (!dev.friendlyName.IsEmpty()) {
        saved.name = dev.friendlyName;
    }
    if (!dev.sinkProtocolInfo.IsEmpty()) {
        saved.formats = CString(dev.sinkProtocolInfo);
        saved.supportsVideo = dev.supportsVideo;
        saved.supportsAudio = dev.supportsAudio;
    }
    return StartSession(dev, saved.DisplayName());
}

bool CDlnaTarget::StartSession(const DlnaDevice& dev, const CString& deviceName)
{
    m_deviceAddress = dev.ipAddress;
    m_localAddress = Dlna::LocalAddressFor(dev.ipAddress);
    if (m_localAddress.IsEmpty()) {
        return false; // no route to the device, nothing could be served
    }
    if (!m_server.IsRunning() && !m_server.Start()) {
        return false;
    }
    m_server.SetAllowedPeer(CStringA(dev.ipAddress));

    m_controlURL = dev.avTransportURL;
    m_scpdURL = dev.avTransportSCPDURL;
    m_volumeURL = dev.renderingControlURL;
    m_mediaURL.Empty();
    m_localDuration = 0.0;
    // Standard AVTransport cannot tell a sleeping device to wake up; when
    // the manufacturer offers a way, the worker uses it before loading.
    m_vendorHook = DlnaVendor::CreateHook(dev.vendor);
    m_hasPlayed = false;
    m_stopIssued = false;
    m_pendingSeek = -1.0;
    m_pollFailures = 0;
    m_seekModes.clear();
    m_seekModesKnown = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = CastTargetState::Connecting;
        m_failReason.Empty();
        m_canSeek = true;
        m_position = m_duration = 0.0;
        m_positionTick = 0;
        m_positionAdvancing = false;
        m_commands.clear();
    }

    m_hStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    m_hCommandEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_hStopSentEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (m_hStopEvent && m_hCommandEvent && m_hStopSentEvent) {
        m_hThread = ::CreateThread(nullptr, 0, StaticThreadProc, (LPVOID)this, 0, nullptr);
    }
    if (!m_hThread) {
        StopWorker();
        m_server.ClearAllowedPeer();
        m_vendorHook.reset();
        return false;
    }

    m_deviceId = dev.udn;
    m_deviceName = deviceName;
    m_casting = true;
    // notifications of the previous session no longer apply
    m_generation = CastNextSessionGeneration();
    return true;
}

bool CDlnaTarget::CanCastFileSaved(const CastSavedDevice& saved, const CString& path)
{
    const CStringA mime = CCastMediaServer::MimeForFile(path);
    if (mime.IsEmpty() || mime == "application/octet-stream") {
        return false;
    }
    // What the device said it accepts beats any guess of ours; the container
    // whitelist only stands in for a device that never answered.
    return saved.formats.IsEmpty() ? IsCommonRendererFormat(mime)
           : SinkAccepts(CStringA(saved.formats), mime);
}

bool CDlnaTarget::CanCastFile(const CString& deviceId, const CString& path)
{
    const CStringA mime = CCastMediaServer::MimeForFile(path);
    if (mime.IsEmpty() || mime == "application/octet-stream") {
        return false;
    }

    for (const DlnaDevice& dev : m_discovery.GetDevices()) {
        if (dev.udn == deviceId) {
            // What the device says it accepts beats any guess of ours; the
            // container whitelist only stands in for a device that stayed
            // silent when asked for its protocol info.
            return dev.sinkProtocolInfo.IsEmpty() ? IsCommonRendererFormat(mime)
                   : SinkAccepts(dev.sinkProtocolInfo, mime);
        }
    }
    return false;
}

bool CDlnaTarget::SinkAccepts(const CStringA& sink, const CStringA& mime)
{
    CStringA haystack(sink), needle(mime);
    haystack.MakeLower();
    needle.MakeLower();

    // each entry is "<protocol>:<network>:<content format>:<additional info>"
    for (int pos = 0; pos < haystack.GetLength();) {
        const int comma = haystack.Find(',', pos);
        CStringA entry = comma < 0 ? haystack.Mid(pos) : haystack.Mid(pos, comma - pos);
        pos = comma < 0 ? haystack.GetLength() : comma + 1;
        entry.Trim();

        const int firstColon = entry.Find(':');
        const int secondColon = firstColon >= 0 ? entry.Find(':', firstColon + 1) : -1;
        if (secondColon < 0) {
            continue;
        }
        const int thirdColon = entry.Find(':', secondColon + 1);
        CStringA format = thirdColon < 0 ? entry.Mid(secondColon + 1)
                          : entry.Mid(secondColon + 1, thirdColon - secondColon - 1);
        const int semi = format.Find(';'); // audio/L16;rate=44100 and friends
        if (semi >= 0) {
            format = format.Left(semi);
        }
        format.Trim();
        if (format == needle || format == "*") {
            return true;
        }
    }
    return false;
}

void CDlnaTarget::LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                            const CastMediaInfo& info)
{
    if (!m_casting) {
        return;
    }

    // One string describes the resource, and the renderer is told it twice:
    // here in the DIDL it is handed, and again in the HTTP response when it
    // asks. A strict renderer compares the two, so they come from one place.
    const CStringA mime = CCastMediaServer::MimeForFile(filePath);
    const CStringA features = ContentFeatures(filePath, mime, info);
    m_server.SetFile(filePath, mime, features);
    const CStringA url = m_server.GetURLForHost(CStringA(m_localAddress));
    if (url.IsEmpty()) {
        // nothing to serve: the device would sit there playing nothing, so the
        // failure is reported and the UI tears the session down
        Fail(_T("no media URL"));
        return;
    }

    // Some renderers size their buffers from it and a few refuse an item
    // without it; the file is open in our own graph, so this costs nothing.
    WIN32_FILE_ATTRIBUTE_DATA attr;
    const ULONGLONG size = GetFileAttributesEx(filePath, GetFileExInfoStandard, &attr)
                           ? ((ULONGLONG)attr.nFileSizeHigh << 32) | attr.nFileSizeLow : 0;

    Command cmd;
    cmd.type = Command::Type::Load;
    cmd.url = CString(url);
    cmd.mime = CString(mime);
    cmd.title = title;
    cmd.features = features;
    cmd.size = size;
    cmd.duration = durationSec;
    cmd.param = startSec >= 1.0 ? startSec : 0.0;
    QueueCommand(std::move(cmd));
}

void CDlnaTarget::Play()
{
    Command cmd;
    cmd.type = Command::Type::Play;
    QueueCommand(std::move(cmd));
}

void CDlnaTarget::Pause()
{
    Command cmd;
    cmd.type = Command::Type::Pause;
    QueueCommand(std::move(cmd));
}

void CDlnaTarget::Seek(double seconds)
{
    Command cmd;
    cmd.type = Command::Type::Seek;
    cmd.param = std::max(seconds, 0.0);
    QueueCommand(std::move(cmd));
}

bool CDlnaTarget::CanSeek() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_canSeek;
}

void CDlnaTarget::SetVolume(double level, bool muted)
{
    Command cmd;
    cmd.type = Command::Type::SetVolume;
    cmd.param = std::min(std::max(level, 0.0), 1.0);
    cmd.muted = muted;
    QueueCommand(std::move(cmd));
}

void CDlnaTarget::StopCasting()
{
    if (m_casting && m_hThread) {
        // The device keeps playing our URL until it is told otherwise, so the
        // STOP is queued first and given a short window to go out; joining
        // right away would leave the queued command unexecuted.
        Command cmd;
        cmd.type = Command::Type::Stop;
        QueueCommand(std::move(cmd));
        WaitForSingleObject(m_hStopSentEvent, STOP_MEDIA_TIMEOUT_MS);
    }

    StopWorker();
    m_server.ClearFile();
    m_server.ClearAllowedPeer();
    m_server.Stop();

    // The device is left switched on and on the input it was put on: casting
    // is something the user asked for, and undoing it afterwards would be as
    // surprising as a remote control switching the amplifier off by itself.
    m_vendorHook.reset();

    m_casting = false;
    m_deviceId.Empty();
    m_deviceName.Empty();
    m_deviceAddress.Empty();
    m_localAddress.Empty();
    m_controlURL.Empty();
    m_scpdURL.Empty();
    m_volumeURL.Empty();
    m_mediaURL.Empty();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = CastTargetState::Idle;
    m_failReason.Empty();
    m_canSeek = true;
    m_position = m_duration = 0.0;
    m_positionTick = 0;
    m_positionAdvancing = false;
    m_commands.clear();
}

void CDlnaTarget::StopWorker()
{
    if (m_hThread) {
        SetEvent(m_hStopEvent);
        // The worker dereferences this object, so the join has to be
        // unconditional or the object is freed underneath a live thread. Every
        // SOAP exchange it performs is bounded and gives up as soon as the stop
        // event is signalled, so the first wait is only a debug tripwire.
        if (WaitForSingleObject(m_hThread, 15000) != WAIT_OBJECT_0) {
            ASSERT(FALSE);
            WaitForSingleObject(m_hThread, INFINITE);
        }
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    for (HANDLE* handle : { &m_hStopEvent, &m_hCommandEvent, &m_hStopSentEvent }) {
        if (*handle) {
            CloseHandle(*handle);
            *handle = nullptr;
        }
    }
}

CastTargetState CDlnaTarget::GetState() const
{
    if (!m_casting) {
        return CastTargetState::Idle;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

CString CDlnaTarget::GetFailureReason() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_failReason;
}

double CDlnaTarget::GetPosition() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != CastTargetState::Playing || !m_positionTick || !m_positionAdvancing) {
        return m_position;
    }
    // the device is only asked every couple of seconds, so the seekbar is fed
    // an extrapolated position in between, but never for longer than a few
    // polls: past that the reading it is based on is no longer worth anything
    const ULONGLONG age = std::min(GetTickCount64() - m_positionTick, MAX_EXTRAPOLATION_MS);
    const double elapsed = (double)age / 1000.0;
    const double position = m_position + elapsed;
    return m_duration > 0.0 ? std::min(position, m_duration) : position;
}

double CDlnaTarget::GetDuration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_duration;
}

void CDlnaTarget::QueueCommand(Command&& cmd)
{
    if (!m_hThread) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands.emplace_back(std::move(cmd));
    }
    SetEvent(m_hCommandEvent);
}

void CDlnaTarget::SetState(CastTargetState state)
{
    HWND hNotifyWnd;
    UINT stateMsg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state == state) {
            return;
        }
        m_state = state;
        hNotifyWnd = m_hNotifyWnd;
        stateMsg = m_stateMsg;
    }
    if (hNotifyWnd && stateMsg) {
        PostMessage(hNotifyWnd, stateMsg, static_cast<WPARAM>(state), (LPARAM)m_generation);
    }
}

void CDlnaTarget::Fail(const CString& reason)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failReason = reason;
    }
    TRACE(_T("DlnaTarget: failed: %s\n"), reason.GetString());
    SetState(CastTargetState::Failed);
}

void CDlnaTarget::UpdatePosition(double position, double duration, bool fromPoll)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (position >= 0.0) {
        // Only a device clock that is seen to move is carried forward between
        // polls. One that is stalled, still buffering or asleep keeps
        // answering the same RelTime, and extrapolating that saws the seekbar
        // back and forth by the poll interval.
        m_positionAdvancing = fromPoll && position > m_position;
        m_position = position;
        m_positionTick = GetTickCount64();
    }
    if (duration > 0.0) {
        m_duration = duration;
    }
}

// --- worker thread ---

DWORD WINAPI CDlnaTarget::StaticThreadProc(LPVOID lpParam)
{
    SetThreadName(DWORD(-1), "DlnaTarget Thread");
    return ((CDlnaTarget*)lpParam)->ThreadProc();
}

DWORD CDlnaTarget::ThreadProc()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return DWORD_ERROR;
    }

    // A receiver in network standby answers AVTransport, takes the URI and
    // reports PLAYING without ever fetching it, so any waking up the device
    // needs has to happen before the first command goes out. It is best effort
    // and bounded, and a device that is already awake costs one request.
    DlnaVendor::WakeOnLan(m_deviceAddress);
    if (m_vendorHook) {
        m_vendorHook->PrepareForCasting(m_hStopEvent);
    }
    DetermineSeekModes();

    ULONGLONG lastPoll = 0;
    HANDLE handles[] = { m_hStopEvent, m_hCommandEvent };
    for (;;) {
        const ULONGLONG tick = GetTickCount64();
        const ULONGLONG deadline = lastPoll + POLL_INTERVAL_MS;
        const DWORD ret = WaitForMultipleObjects(_countof(handles), handles, FALSE,
                                                 (DWORD)(deadline > tick ? deadline - tick : 0));
        if (ret == WAIT_OBJECT_0) {
            break; // stop requested
        }

        ProcessCommands();
        if (WaitForSingleObject(m_hStopEvent, 0) == WAIT_OBJECT_0) {
            break;
        }
        if (GetTickCount64() - lastPoll >= POLL_INTERVAL_MS) {
            Poll();
            lastPoll = GetTickCount64();
        }
    }

    WSACleanup();
    return 0;
}

void CDlnaTarget::ProcessCommands()
{
    for (;;) {
        Command cmd;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_commands.empty()) {
                return;
            }
            cmd = m_commands.front();
            m_commands.erase(m_commands.begin());
        }
        RunCommand(cmd);
        if (WaitForSingleObject(m_hStopEvent, 0) == WAIT_OBJECT_0) {
            return;
        }
    }
}

void CDlnaTarget::RunCommand(const Command& cmd)
{
    CStringA args, response;

    switch (cmd.type) {
        case Command::Type::Load: {
            m_mediaURL = cmd.url;
            m_localDuration = cmd.duration > 0.0 ? cmd.duration : 0.0;
            m_hasPlayed = false;
            m_stopIssued = false;
            m_pendingSeek = cmd.param > 0.0 ? cmd.param : -1.0;
            UpdatePosition(0.0, cmd.duration);
            SetState(CastTargetState::Loading);

            // A renderer still playing something else refuses the new URI, so
            // it is stopped first. That stop failing is normal on an idle
            // device and must not be reported, hence not through AvTransport().
            CString preStopFault;
            Dlna::SoapCall(m_controlURL, AVTRANSPORT_SERVICE, "Stop", "<InstanceID>0</InstanceID>",
                           response, preStopFault, m_hStopEvent);

            const CStringA metadata = BuildMetadata(cmd);
            TRACE(_T("DlnaTarget: handing the device %hs\n"), metadata.GetString());
            args.Format("<InstanceID>0</InstanceID>"
                        "<CurrentURI>%s</CurrentURI>"
                        "<CurrentURIMetaData>%s</CurrentURIMetaData>",
                        Dlna::XmlEscape(CStringA(cmd.url)).GetString(),
                        Dlna::XmlEscape(metadata).GetString());
            if (!AvTransport("SetAVTransportURI", args, response)) {
                return; // AvTransport() has already reported the fault
            }
            if (!AvTransport("Play", "<InstanceID>0</InstanceID><Speed>1</Speed>", response)) {
                return;
            }
            break;
        }
        case Command::Type::Play:
            if (AvTransport("Play", "<InstanceID>0</InstanceID><Speed>1</Speed>", response)) {
                SetState(CastTargetState::Playing);
            }
            break;
        case Command::Type::Pause:
            if (AvTransport("Pause", "<InstanceID>0</InstanceID>", response)) {
                SetState(CastTargetState::Paused);
            }
            break;
        case Command::Type::Seek:
            if (!m_hasPlayed) {
                // seeking a transport that has not started yet is refused by
                // most renderers, so it waits for the first PLAYING poll
                m_pendingSeek = cmd.param;
                break;
            }
            {
                double target = cmd.param;
                if (SeekDevice(target)) {
                    UpdatePosition(target, 0.0);
                }
            }
            break;
        case Command::Type::SetVolume: {
            if (m_volumeURL.IsEmpty()) {
                break;
            }
            CStringA volumeArgs, response2;
            CString fault;
            volumeArgs.Format("<InstanceID>0</InstanceID><Channel>Master</Channel>"
                              "<DesiredVolume>%d</DesiredVolume>", (int)(cmd.param * 100.0 + 0.5));
            Dlna::SoapCall(m_volumeURL, RENDERINGCONTROL_SERVICE, "SetVolume", volumeArgs,
                           response2, fault, m_hStopEvent);
            volumeArgs.Format("<InstanceID>0</InstanceID><Channel>Master</Channel>"
                              "<DesiredMute>%d</DesiredMute>", cmd.muted ? 1 : 0);
            Dlna::SoapCall(m_volumeURL, RENDERINGCONTROL_SERVICE, "SetMute", volumeArgs,
                           response2, fault, m_hStopEvent);
            break;
        }
        case Command::Type::Stop:
            m_stopIssued = true;
            AvTransport("Stop", "<InstanceID>0</InstanceID>", response);
            SetEvent(m_hStopSentEvent);
            break;
    }
}

bool CDlnaTarget::AvTransport(const CStringA& action, const CStringA& args, CStringA& response)
{
    CString fault;
    if (Dlna::SoapCall(m_controlURL, AVTRANSPORT_SERVICE, action, args, response, fault, m_hStopEvent)) {
        return true;
    }
    // A stop on the way out is allowed to fail quietly; anything else the user
    // asked for has to surface, with whatever the device said about it.
    if (!m_stopIssued && WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0) {
        Fail(fault.IsEmpty() ? CString(action) : fault);
    }
    return false;
}

void CDlnaTarget::SetCanSeek(bool canSeek)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_canSeek = canSeek;
}

void CDlnaTarget::DetermineSeekModes()
{
    m_seekModesKnown = true;

    // In the order they are tried. For a stream that begins at zero, which
    // everything we serve does, the two mean the same thing; REL_TIME is what
    // a renderer that seeks at all almost always offers.
    static const char* const preferred[] = { "REL_TIME", "ABS_TIME" };

    // The service description states which units Seek takes. A device that
    // does not publish one, or is not reachable for it, is not written off:
    // both units are simply tried in turn on the first seek.
    std::vector<CStringA> allowed;
    CStringA scpd;
    if (!m_scpdURL.IsEmpty() && Dlna::HttpGet(m_scpdURL, scpd, m_hStopEvent)) {
        for (int pos = 0;;) {
            int innerStart = 0, innerEnd = 0, next = 0;
            if (!Dlna::FindElement(scpd, "stateVariable", pos, innerStart, innerEnd, next)) {
                break;
            }
            const CStringA variable = scpd.Mid(innerStart, innerEnd - innerStart);
            pos = next;
            if (Dlna::GetElementText(variable, "name") != "A_ARG_TYPE_SeekMode") {
                continue;
            }
            for (int valuePos = 0;;) {
                int valueStart = 0, valueEnd = 0, valueNext = 0;
                if (!Dlna::FindElement(variable, "allowedValue", valuePos, valueStart, valueEnd, valueNext)) {
                    break;
                }
                CStringA value = Dlna::XmlUnescape(variable.Mid(valueStart, valueEnd - valueStart));
                value.Trim();
                allowed.emplace_back(value);
                valuePos = valueNext;
            }
            break;
        }
    }

    for (const char* mode : preferred) {
        if (allowed.empty() || std::find(allowed.cbegin(), allowed.cend(), CStringA(mode)) != allowed.cend()) {
            m_seekModes.emplace_back(mode);
        }
    }
    if (m_seekModes.empty()) {
        // the device published its units and neither of ours is among them
        TRACE(_T("DlnaTarget: the device offers no seek unit we can use\n"));
    }
    SetCanSeek(!m_seekModes.empty());
}

double CDlnaTarget::ClampSeekTarget(double seconds) const
{
    // A drag to the very end of the seekbar asks for a target at or past the
    // length, which a renderer answers with a fault instead of a seek. The
    // length our own graph measured is preferred over the device's, the same
    // way the DIDL prefers it, and a margin is kept clear of the end.
    double duration = m_localDuration;
    if (duration <= 0.0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        duration = m_duration;
    }
    if (duration > 0.0) {
        seconds = std::min(seconds, std::max(0.0, duration - SEEK_END_MARGIN_SEC));
    }
    return std::max(0.0, seconds);
}

// seconds is updated to the target the device was actually asked for, so that
// the position the caller records afterwards is the one the device is at.
bool CDlnaTarget::SeekDevice(double& seconds)
{
    if (!m_seekModesKnown) {
        DetermineSeekModes();
    }

    seconds = ClampSeekTarget(seconds);

    CStringA args, response;
    while (!m_seekModes.empty()) {
        CString fault;
        Dlna::SoapStatus status;
        args.Format("<InstanceID>0</InstanceID><Unit>%s</Unit><Target>%s</Target>",
                    m_seekModes.front().GetString(), FormatDuration(seconds, false).GetString());
        if (Dlna::SoapCall(m_controlURL, AVTRANSPORT_SERVICE, "Seek", args, response, fault,
                           m_hStopEvent, &status)) {
            return true;
        }
        if (WaitForSingleObject(m_hStopEvent, 0) == WAIT_OBJECT_0 || status.httpStatus == 0) {
            return false; // nothing came back, so the unit is not what failed
        }
        // Only an error that indicts the unit itself takes it out of the
        // session: 710 is "seek mode not supported" and 402 an argument the
        // device would not parse. Everything else -- 718 for a target the
        // device thinks is past the end, 701 for a transport not ready to
        // seek -- is about this one seek, and the unit is offered again next
        // time. A fault that carries no errorCode says nothing either way, so
        // it only drops the unit while there is another one to fall back on.
        const int errorCode = status.errorCode;
        if (!(errorCode == 710 || errorCode == 402 || (errorCode == 0 && m_seekModes.size() > 1))) {
            TRACE(_T("DlnaTarget: the device refused this Seek in %hs: %s\n"),
                  m_seekModes.front().GetString(), fault.GetString());
            return false;
        }
        // The device rejected the unit rather than the target: it will reject
        // the same unit again, so this session stops offering it.
        TRACE(_T("DlnaTarget: the device refused Seek in %hs: %s\n"),
              m_seekModes.front().GetString(), fault.GetString());
        m_seekModes.erase(m_seekModes.begin());
    }

    // Out of units. The stream itself is fine, so the session carries on;
    // saying so only lets the UI stop pretending the seekbar works rather
    // than silently dropping every drag on it.
    SetCanSeek(false);
    return false;
}

void CDlnaTarget::Poll()
{
    CStringA response;
    CString fault;
    if (!Dlna::SoapCall(m_controlURL, AVTRANSPORT_SERVICE, "GetTransportInfo",
                        "<InstanceID>0</InstanceID>", response, fault, m_hStopEvent)) {
        if (WaitForSingleObject(m_hStopEvent, 0) == WAIT_OBJECT_0) {
            return;
        }
        if (++m_pollFailures >= MAX_POLL_FAILURES) {
            Fail(fault.IsEmpty() ? CString(_T("device stopped responding")) : fault);
        }
        return;
    }
    m_pollFailures = 0;

    const CStringA transportState = Dlna::GetElementText(response, "CurrentTransportState");
    const bool playing = transportState == "PLAYING";
    if (playing) {
        m_hasPlayed = true;
    }

    // GetPositionInfo also tells us whether the device is still on our URI:
    // another controller pointing it elsewhere ends our session.
    if (Dlna::SoapCall(m_controlURL, AVTRANSPORT_SERVICE, "GetPositionInfo",
                       "<InstanceID>0</InstanceID>", response, fault, m_hStopEvent)) {
        const CString trackURI = UTF8To16(Dlna::GetElementText(response, "TrackURI"));
        if (!m_mediaURL.IsEmpty() && !trackURI.IsEmpty() && trackURI != m_mediaURL) {
            TRACE(_T("DlnaTarget: the device moved on to %s\n"), trackURI.GetString());
            SetState(CastTargetState::TakenOver);
            return;
        }
        // What our own graph measured beats what the device makes of the
        // stream: one receiver answers 5:35:06 for a twenty second file. The
        // device is only believed when we have no length of our own.
        const double trackDuration = m_localDuration > 0.0 ? m_localDuration
                                     : ParseDuration(Dlna::GetElementText(response, "TrackDuration"));
        UpdatePosition(ParseDuration(Dlna::GetElementText(response, "RelTime")), trackDuration, true);
    } else {
        // The transport may well still say PLAYING, but its clock is out of
        // reach, so the last reading must not be carried forward any further:
        // a device that never answers this again would otherwise have the
        // seekbar racing to the end on an extrapolation nothing renews.
        std::lock_guard<std::mutex> lock(m_mutex);
        m_positionAdvancing = false;
    }

    if (playing) {
        SetState(CastTargetState::Playing);
        if (m_pendingSeek >= 0.0) {
            double target = m_pendingSeek;
            m_pendingSeek = -1.0;
            if (SeekDevice(target)) {
                UpdatePosition(target, 0.0);
            }
        }
    } else if (transportState == "PAUSED_PLAYBACK" || transportState == "PAUSED_RECORDING") {
        SetState(CastTargetState::Paused);
    } else if (transportState == "TRANSITIONING") {
        SetState(CastTargetState::Buffering);
    } else if (transportState == "STOPPED" || transportState == "NO_MEDIA_PRESENT") {
        // A renderer is STOPPED both before it starts and after it finishes;
        // only a stop that follows playback we did not ask to end is the end
        // of the media.
        if (m_hasPlayed && !m_stopIssued) {
            SetState(CastTargetState::Ended);
        }
    }
}

// --- formatting ---

CStringA CDlnaTarget::BuildMetadata(const Command& cmd)
{
    const CStringA mime(cmd.mime);

    // Both are optional in DIDL-Lite, but a renderer that sizes its buffering
    // from the byte count or draws its seekbar from the duration wants them,
    // and a few refuse an item that carries neither.
    CStringA resAttrs;
    if (cmd.size) {
        resAttrs.Format(" size=\"%I64u\"", cmd.size);
    }
    if (cmd.duration > 0.0) {
        resAttrs.AppendFormat(" duration=\"%s\"", FormatDuration(cmd.duration, true).GetString());
    }

    CStringA metadata;
    metadata.Format("<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\""
                    " xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
                    " xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\""
                    " xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">"
                    "<item id=\"0\" parentID=\"-1\" restricted=\"1\">"
                    "<dc:title>%s</dc:title>"
                    "<upnp:class>%s</upnp:class>"
                    "<res protocolInfo=\"http-get:*:%s:%s\"%s>%s</res>"
                    "</item></DIDL-Lite>",
                    Dlna::XmlEscape(UTF16To8(cmd.title)).GetString(),
                    UpnpClassFor(mime).GetString(),
                    mime.GetString(),
                    cmd.features.IsEmpty() ? CCastMediaServer::dlnaContentFeatures : cmd.features.GetString(),
                    resAttrs.GetString(),
                    Dlna::XmlEscape(CStringA(cmd.url)).GetString());
    return metadata;
}

CStringA CDlnaTarget::ContentFeatures(const CString& path, const CStringA& mime, const CastMediaInfo& info)
{
    CStringA features;
    const CStringA profile = DlnaProfileName(path, mime, info);
    if (!profile.IsEmpty()) {
        features.Format("DLNA.ORG_PN=%s;", profile.GetString());
    }
    features += CCastMediaServer::dlnaContentFeatures;
    return features;
}

CStringA CDlnaTarget::FormatDuration(double seconds, bool withMilliseconds)
{
    if (seconds < 0.0 || seconds > 24.0 * 3600.0 * 99.0) {
        seconds = 0.0;
    }
    const ULONGLONG total = (ULONGLONG)seconds;
    CStringA text;
    text.Format("%02u:%02u:%02u", (UINT)(total / 3600), (UINT)((total / 60) % 60), (UINT)(total % 60));
    if (withMilliseconds) {
        text.AppendFormat(".%03u", (UINT)((seconds - (double)total) * 1000.0));
    }
    return text;
}

double CDlnaTarget::ParseDuration(const CStringA& text)
{
    // H+:MM:SS[.F+], or NOT_IMPLEMENTED from a device that does not keep time
    UINT hours = 0, minutes = 0, seconds = 0;
    if (text.IsEmpty() || sscanf_s(text, "%u:%u:%u", &hours, &minutes, &seconds) != 3
            || minutes > 59 || seconds > 59 || hours > 99 * 24) {
        return -1.0;
    }
    return (double)hours * 3600.0 + (double)minutes * 60.0 + (double)seconds;
}
