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

#include <io.h>
#include "PathUtils.h"
#include "mplayerc.h"

enum class LogTargets {
    PLAYER    =  1,
    GRAPH     =  2,
    YDL       =  4,
    SUBTITLES =  8,
    BDA       = 16,
    CASTING   = 32,
};

namespace
{
    template<LogTargets TARGET>
    constexpr LPCTSTR GetFileName();

    template<>
    constexpr LPCTSTR GetFileName<LogTargets::PLAYER>()
    {
        return _T("player.log");
    }

    template<>
    constexpr LPCTSTR GetFileName<LogTargets::GRAPH>()
    {
        return _T("filtergraph.log");
    }

    template<>
    constexpr LPCTSTR GetFileName<LogTargets::BDA>()
    {
        return _T("bda.log");
    }

    template<>
    constexpr LPCTSTR GetFileName<LogTargets::SUBTITLES>()
    {
        return _T("subtitles.log");
    }

    template<>
    constexpr LPCTSTR GetFileName<LogTargets::YDL>()
    {
        return _T("youtubedl.log");
    }

    template<>
    constexpr LPCTSTR GetFileName<LogTargets::CASTING>()
    {
        return _T("casting.log");
    }

    void WriteToFile(FILE* f, LPCSTR function, _In_z_ _Printf_format_string_ LPCTSTR fmt, va_list& args)
    {
        SYSTEMTIME local_time;
        GetLocalTime(&local_time);

        _ftprintf_s(f, _T("%.2hu:%.2hu:%.2hu.%.3hu - %S: "), local_time.wHour, local_time.wMinute,
                    local_time.wSecond, local_time.wMilliseconds, function);
        _vftprintf_s(f, fmt, args);
        _ftprintf_s(f, _T("\n"));
    }
    void WriteToFile2(FILE* f, _In_z_ _Printf_format_string_ LPCTSTR fmt, va_list& args)
    {
        SYSTEMTIME local_time;
        GetLocalTime(&local_time);

        _ftprintf_s(f, _T("%.2hu:%.2hu:%.2hu.%.3hu: "), local_time.wHour, local_time.wMinute,
            local_time.wSecond, local_time.wMilliseconds);
        _vftprintf_s(f, fmt, args);
        _ftprintf_s(f, _T("\n"));
    }
    // A whole line at once, for a log several threads write to at the same
    // time: three calls can interleave halfway through a line, one cannot.
    // Flushed as it goes, because the interesting logs are the ones read
    // after a hang.
    void WriteLineToFile(FILE* f, _In_z_ _Printf_format_string_ LPCTSTR fmt, va_list& args)
    {
        SYSTEMTIME local_time;
        GetLocalTime(&local_time);

        CString line;
        line.Format(_T("%.2hu:%.2hu:%.2hu.%.3hu: "), local_time.wHour, local_time.wMinute,
                    local_time.wSecond, local_time.wMilliseconds);
        CString message;
        message.FormatV(fmt, args);
        // A device name or a URL arrives from the network and is logged as it
        // came, and this log gets pasted in public. So anything in it that
        // could end the line early or start a forged one is shown rather than
        // obeyed: a break becomes the two characters \r or \n, and every other
        // control character its \uXXXX. A tab is the one kept.
        for (int i = 0; i < message.GetLength(); i++) {
            const TCHAR c = message[i];
            if (c == _T('\r')) {
                line += _T("\\r");
            } else if (c == _T('\n')) {
                line += _T("\\n");
            } else if ((c < _T(' ') && c != _T('\t')) || (c >= 0x7F && c <= 0x9F)
                       || c == 0x2028 || c == 0x2029) {
                line.AppendFormat(_T("\\u%04X"), (unsigned)c);
            } else {
                line += c;
            }
        }
        line += _T('\n');
        // Written as UTF-8 bytes rather than through the wide stream functions:
        // those convert through the C locale, which cannot represent anything
        // outside ASCII and abandons the line at the first character it cannot
        // encode, and a file name is very often not ASCII. A BOM goes in front
        // of a log that is being started, so that an editor opening it knows.
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, line.GetLength(), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) {
            return;
        }
        CStringA utf8;
        WideCharToMultiByte(CP_UTF8, 0, line, line.GetLength(),
                            utf8.GetBufferSetLength(bytes), bytes, nullptr, nullptr);
        utf8.ReleaseBuffer(bytes);
        if (_filelengthi64(_fileno(f)) == 0) {
            fputs("\xEF\xBB\xBF", f);
        }
        fputs(utf8, f);
        fflush(f);
    }
}

template<LogTargets TARGET>
struct Logger final {
    static void Log(LPCSTR function, LPCTSTR fmt, ...) {
        static Logger logger;

        if (!logger.m_file) {
            return;
        }

        va_list args;
        va_start(args, fmt);
        WriteToFile(logger.m_file, function, fmt, args);
        va_end(args);
    }
    static void Log2(LPCTSTR fmt, ...) {
        static Logger logger;

        if (!logger.m_file) {
            return;
        }

        va_list args;
        va_start(args, fmt);
        WriteToFile2(logger.m_file, fmt, args);
        va_end(args);
    }
    static void LogLine(LPCTSTR fmt, ...) {
        static Logger logger;

        if (!logger.m_file) {
            return;
        }

        va_list args;
        va_start(args, fmt);
        WriteLineToFile(logger.m_file, fmt, args);
        va_end(args);
    }

    // Whether anything would be written at all, so that a caller with a
    // summary to build can skip building it.
    static bool IsEnabled() {
        const auto& s = AfxGetAppSettings();
        return s.IsInitialized() && (s.DebugLogMask & (int)TARGET) != 0;
    }

private:
    Logger() {
        const auto& s = AfxGetAppSettings();
        m_file = nullptr;
        if (s.IsInitialized()) {
            if (s.DebugLogMask & (int)TARGET) {
                CString savePath;
                if (AfxGetMyApp()->GetAppSavePath(savePath)) {
                    if (!PathUtils::Exists(savePath)) {
                        ::CreateDirectory(savePath, nullptr);
                    }
                    m_file = _tfsopen(PathUtils::CombinePaths(savePath, GetFileName<TARGET>()), _T("at"), SH_DENYWR);
                }
                ASSERT(m_file);
            }
        } else {
            ASSERT(false);
        }
    }

    ~Logger() {
        if (m_file) {
            fclose(m_file);
        }
    }

    FILE* m_file;
};


#define MPCHC_LOG(TARGET, fmt, ...)  Logger<LogTargets::TARGET>::Log(__FUNCTION__, fmt, __VA_ARGS__)
#define MPCHC_LOG2(TARGET, fmt, ...) Logger<LogTargets::TARGET>::Log2(fmt, __VA_ARGS__)

#define PLAYER_LOG(...) MPCHC_LOG2(PLAYER, __VA_ARGS__)
#define GRAPH_LOG(...) MPCHC_LOG2(GRAPH, __VA_ARGS__)
#define BDA_LOG(...) MPCHC_LOG(BDA, __VA_ARGS__)
#define SUBTITLES_LOG(...) MPCHC_LOG(SUBTITLES, __VA_ARGS__)
#define YDL_LOG(fmt, ...) MPCHC_LOG2(YDL, fmt, __VA_ARGS__)
#define CASTING_LOG(...) Logger<LogTargets::CASTING>::LogLine(__VA_ARGS__)
#define CASTING_LOGGING() Logger<LogTargets::CASTING>::IsEnabled()

#define USE_LOGGER(s) (s.DebugLogMask & (int)LogTargets::PLAYER)
#define USE_GRAPH_LOGGER(s) (s.DebugLogMask & (int)LogTargets::GRAPH)

#define FLUSH_LOGGER() _flushall()
