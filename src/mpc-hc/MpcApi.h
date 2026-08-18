/*
 * (C) 2006-2015 see Authors.txt
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


/*
This file defines commands used for the MPC-HC API. To send commands
to MPC-HC and receive playback notifications, first launch MPC-HC with the /slave command-line
argument followed by an HWND that will receive notifications:

..\bin\mpc-hc /slave 125421

After startup, MPC-HC sends the host a WM_COPYDATA message with COPYDATASTRUCT filled with:
     - dwData :  CMD_CONNECT
     - lpData :  Unicode string containing MPC-HC's main window handle

To control MPC-HC, send WM_COPYDATA messages to the HWND provided on connection. All messages must be
formatted as null-terminated Unicode strings. For commands or notifications with multiple parameters,
values are separated by |.
If a string contains a |, it will be escaped with a \ so a \| is not a separator.

Ex: When a file is opened, MPC-HC sends to host the "now playing" notification:
     - dwData :  CMD_NOWPLAYING
     - lpData :  title|author|description|filename|duration

Ex: When a DVD is playing, use CMD_GETNOWPLAYING to get:
     - dwData :  CMD_NOWPLAYING
     - lpData :  dvddomain|titlenumber|numberofchapters|currentchapter|titleduration
                 dvddomains: DVD - Stopped, DVD - FirstPlay, DVD - RootMenu, DVD - TitleMenu, DVD - Title
*/

#pragma once

typedef enum MPC_LOADSTATE {
    MLS_CLOSED,
    MLS_LOADING,
    MLS_LOADED,
    MLS_CLOSING,
    MLS_FAILING,
} MPC_LOADSTATE;


typedef enum MPC_PLAYSTATE {
    PS_PLAY   = 0,
    PS_PAUSE  = 1,
    PS_STOP   = 2,
    PS_UNUSED = 3
} MPC_PLAYSTATE;


struct MPC_OSDDATA {
    int nMsgPos;       // screen position constant (see OSD_MESSAGEPOS constants)
    int nDurationMS;   // duration in milliseconds
    TCHAR strMsg[128]; // message to display in OSD
};

// MPC_OSDDATA. nMsgPos constants (for host side programming):
/*
typedef enum {
    OSD_NOMESSAGE,
    OSD_TOPLEFT,
    OSD_TOPRIGHT
} OSD_MESSAGEPOS;
*/


typedef enum MPCAPI_COMMAND :
    unsigned int {
    // ==== Commands from MPC-HC to host
    // (Each entry notes its equivalent on the integer message channel, or "INT API: no".)

    // Sent after connecting to the host
    // Parameter 1: MPC-HC window handle (commands must be sent to this HWND)
    // INT API: no (connection handshake is WM_COPYDATA only)
    CMD_CONNECT             = 0x50000000,

    // Send when opening or closing file
    // Parameter 1: current state (see MPC_LOADSTATE enum)
    // INT API: MPCINT_STATE
    CMD_STATE               = 0x50000001,

    // Send when playing, pausing or closing a file
    // Parameter 1: current play mode (see MPC_PLAYSTATE enum)
    // INT API: MPCINT_PLAYMODE
    CMD_PLAYMODE            = 0x50000002,

    // Send after opening a new file
    // Parameter 1: title
    // Parameter 2: author
    // Parameter 3: description
    // Parameter 4: complete filename (path included)
    // Parameter 5: duration in seconds
    // INT API: no (string data)
    CMD_NOWPLAYING          = 0x50000003,

    // List of subtitle tracks
    // Parameter 1: Subtitle track name 0
    // Parameter 2: Subtitle track name 1
    // ...
    // Parameter n: Active subtitle track, -1 if subtitles are disabled
    //
    // if no subtitle track present, returns -1
    // if no file loaded, returns -2
    // INT API: no (string list)
    CMD_LISTSUBTITLETRACKS  = 0x50000004,

    // List of audio tracks
    // Parameter 1: Audio track name 0
    // Parameter 2: Audio track name 1
    // ...
    // Parameter n: Active audio track
    //
    // if no audio track is present, returns -1
    // if no file is loaded, returns -2
    // INT API: no (string list)
    CMD_LISTAUDIOTRACKS     = 0x50000005,

    // Send index of currently selected audio track
    // INT API: MPCINT_CURRENTAUDIOTRACK
    CMD_CURRENTAUDIOTRACK   = 0x5000000C,

    // Send index of currently selected subtitle track
    // INT API: MPCINT_CURRENTSUBTITLETRACK
    CMD_CURRENTSUBTITLETRACK   = 0x5000000D,

    // Send the current volume after it changes or in response to CMD_GETVOLUME
    // Parameter 1: volume (0-100)
    // INT API: MPCINT_CURRENTVOLUME
    CMD_CURRENTVOLUME       = 0x5000000E,

    // Send the current mute state after it changes or in response to CMD_GETMUTE
    // Parameter 1: mute state (0 or 1)
    // INT API: MPCINT_CURRENTMUTE
    CMD_CURRENTMUTE         = 0x5000000F,

    // Send current playback position in response
    // of CMD_GETCURRENTPOSITION.
    // Parameter 1: current position in seconds
    // INT API: no (fractional seconds; the integer channel can't carry sub-second precision)
    CMD_CURRENTPOSITION     = 0x50000007,

    // Send the current playback position after a jump.
    // (Automatically sent after a seek event).
    // Parameter 1: new playback position (in seconds).
    // INT API: no (fractional seconds)
    CMD_NOTIFYSEEK          = 0x50000008,

    // Notify the end of current playback
    // (Automatically sent).
    // Parameter 1: none.
    // INT API: no (kept WM_COPYDATA only)
    CMD_NOTIFYENDOFSTREAM   = 0x50000009,

    // Send version string
    // Parameter 1: MPC-HC's version
    // INT API: no (string data)
    CMD_VERSION             = 0x5000000A,

    // List of files in the playlist
    // Parameter 1: file path 0
    // Parameter 2: file path 1
    // ...
    // Parameter n: active file, -1 if no active file
    // INT API: no (string list)
    CMD_PLAYLIST            = 0x50000006,

    // Send information about MPC-HC closing
    // INT API: no (kept WM_COPYDATA only)
    CMD_DISCONNECT          = 0x5000000B,

    // ==== Commands from host to MPC-HC

    // Open new file
    // Parameter 1: file path
    // INT API: no (string data)
    CMD_OPENFILE            = 0xA0000000,

    // Stop playback, but keep file / playlist
    // INT API: MPCINT_STOP
    CMD_STOP                = 0xA0000001,

    // Stop playback and close file / playlist
    // INT API: MPCINT_CLOSEFILE
    CMD_CLOSEFILE           = 0xA0000002,

    // Pause or restart playback
    // INT API: MPCINT_PLAYPAUSE
    CMD_PLAYPAUSE           = 0xA0000003,

    // Unpause playback
    // INT API: MPCINT_PLAY
    CMD_PLAY                = 0xA0000004,

    // Pause playback
    // INT API: MPCINT_PAUSE
    CMD_PAUSE               = 0xA0000005,

    // Add a new file to playlist (did not start playing)
    // Parameter 1: file path
    // INT API: no (string data)
    CMD_ADDTOPLAYLIST       = 0xA0001000,

    // Remove all files from playlist
    // INT API: MPCINT_CLEARPLAYLIST
    CMD_CLEARPLAYLIST       = 0xA0001001,

    // Start playing playlist
    // INT API: MPCINT_STARTPLAYLIST
    CMD_STARTPLAYLIST       = 0xA0001002,

    CMD_REMOVEFROMPLAYLIST  = 0xA0001003,   // TODO

    // Cue current file to specific position
    // Parameter 1: new position in seconds
    // INT API: no (fractional seconds)
    CMD_SETPOSITION         = 0xA0002000,

    // Set the audio delay
    // Parameter 1: new audio delay in ms
    // INT API: MPCINT_SETAUDIODELAY
    CMD_SETAUDIODELAY       = 0xA0002001,

    // Set the subtitle delay
    // Parameter 1: new subtitle delay in ms
    // INT API: MPCINT_SETSUBTITLEDELAY
    CMD_SETSUBTITLEDELAY    = 0xA0002002,

    // Set the active file in the playlist
    // Parameter 1: index of the active file, -1 for no file selected
    // DOESN'T WORK
    // INT API: no
    CMD_SETINDEXPLAYLIST    = 0xA0002003,

    // Set the audio track
    // Parameter 1: index of the audio track
    // INT API: MPCINT_SETAUDIOTRACK
    CMD_SETAUDIOTRACK       = 0xA0002004,

    // Set the subtitle track
    // Parameter 1: index of the subtitle track, -1 for disabling subtitles
    // INT API: MPCINT_SETSUBTITLETRACK
    CMD_SETSUBTITLETRACK    = 0xA0002005,

    // Ask for a list of the subtitles tracks of the file
    // return a CMD_LISTSUBTITLETRACKS
    // INT API: no (string-list response)
    CMD_GETSUBTITLETRACKS   = 0xA0003000,

    // Ask for the current playback position
    // Returns CMD_CURRENTPOSITION
    // INT API: no (fractional-seconds response)
    CMD_GETCURRENTPOSITION  = 0xA0003004,

    // Jump forward/backward of N seconds,
    // Parameter 1: seconds (negative values for backward)
    // INT API: MPCINT_JUMPOFNSECONDS
    CMD_JUMPOFNSECONDS      = 0xA0003005,

    // Ask MPC-HC for its version
    // Returns CMD_VERSION
    // INT API: no (string response)
    CMD_GETVERSION          = 0xA0003006,

    // Ask for a list of the audio tracks of the file
    // return a CMD_LISTAUDIOTRACKS
    // INT API: no (string-list response)
    CMD_GETAUDIOTRACKS      = 0xA0003001,

    // Ask for the properties of the current loaded file
    // return a CMD_NOWPLAYING
    // INT API: no (string response)
    CMD_GETNOWPLAYING       = 0xA0003002,

    // Ask for the current playlist
    // return a CMD_PLAYLIST
    // INT API: no (string-list response)
    CMD_GETPLAYLIST         = 0xA0003003,

    // Ask for the index of the currently selected audio track
    // return a CMD_CURRENTAUDIOTRACK
    // INT API: MPCINT_GETCURRENTAUDIOTRACK
    CMD_GETCURRENTAUDIOTRACK      = 0xA0003007,

    // Ask for the index of the currently selected subtitle track
    // return a CMD_CURRENTSUBTITLETRACK
    // INT API: MPCINT_GETCURRENTSUBTITLETRACK
    CMD_GETCURRENTSUBTITLETRACK   = 0xA0003008,

    // Ask for the current volume
    // Returns CMD_CURRENTVOLUME
    // INT API: MPCINT_GETVOLUME
    CMD_GETVOLUME           = 0xA0003009,

    // Ask for the current mute state
    // Returns CMD_CURRENTMUTE
    // INT API: MPCINT_GETMUTE
    CMD_GETMUTE             = 0xA000300A,

    // Set the volume without changing the mute state
    // Parameter 1: volume (0-100)
    // Honored only when the WM_COPYDATA wParam is the connected host's registered
    // window handle; sends from other windows are ignored
    // INT API: MPCINT_SETVOLUME
    CMD_SETVOLUME           = 0xA0003010,

    // Set the mute state without changing the volume
    // Parameter 1: mute state (0 or 1)
    // Honored only when the WM_COPYDATA wParam is the connected host's registered
    // window handle; sends from other windows are ignored
    // INT API: MPCINT_SETMUTE
    CMD_SETMUTE             = 0xA0003011,

    // Toggle FullScreen
    // INT API: MPCINT_TOGGLEFULLSCREEN
    CMD_TOGGLEFULLSCREEN    = 0xA0004000,

    // Jump forward(medium)
    // INT API: MPCINT_JUMPFORWARDMED
    CMD_JUMPFORWARDMED      = 0xA0004001,

    // Jump backward(medium)
    // INT API: MPCINT_JUMPBACKWARDMED
    CMD_JUMPBACKWARDMED     = 0xA0004002,

    // Increase Volume
    // INT API: MPCINT_INCREASEVOLUME
    CMD_INCREASEVOLUME      = 0xA0004003,

    // Decrease volume
    // INT API: MPCINT_DECREASEVOLUME
    CMD_DECREASEVOLUME      = 0xA0004004,

    // Toggle shader
    // INT API: MPCINT_SHADER_TOGGLE
    CMD_SHADER_TOGGLE       = 0xA0004005,

    // Close App
    // INT API: MPCINT_CLOSEAPP
    CMD_CLOSEAPP            = 0xA0004006,

    // Set playing rate
    // INT API: no (fractional value)
    CMD_SETSPEED            = 0xA0004008,

    // Set Pan & Scan
    // INT API: no (string preset name)
    CMD_SETPANSCAN          = 0xA0004009,

    // Show host defined OSD message string
    // INT API: no (struct data)
    CMD_OSDSHOWMESSAGE      = 0xA0005000,

    // Show a host-defined message in the status bar for three seconds
    // Parameter 1: non-empty, single-line, well-formed UTF-16 text; maximum
    //              512 UTF-16 code units, excluding the terminating null.
    //              Control characters (including TAB), DEL, C1 controls and the
    //              U+2028/U+2029 line separators are rejected; a rejected message
    //              is silently ignored.
    // Unlike other commands, this one is honored only when the WM_COPYDATA wParam
    // is the connected host's registered window handle; sends from other windows
    // are ignored.
    // INT API: no (string data)
    CMD_STATUSSHOWMESSAGE   = 0xA0005001

} MPCAPI_COMMAND;


// ---------------------------------------------------------------------------
// Non-blocking integer message API (companion to the WM_COPYDATA API above)
//
// The WM_COPYDATA API passes every value as string data, which must be sent
// synchronously (the payload buffer has to outlive delivery). For plain integer
// values that is unnecessary overhead. This companion channel carries a command
// and a small integer entirely inside a window message, so it can be delivered
// with a non-blocking PostMessage.
//
// Both MPC-HC and the host obtain the message id at runtime with
//     UINT msg = RegisterWindowMessage(MPCAPI_INT_MESSAGE_NAME);
// (a system-wide unique id, so it cannot collide with either side's WM_APP range)
// and exchange values as:
//     wParam :  the sender's HWND (as with WM_COPYDATA)
//     lParam :  low 24 bits = signed value, high 8 bits = command id
//               (use the MPCAPI_INT_* pack/unpack macros below)
//
// Why 24 bits of value and 8 of command, rather than a 32-bit value: the sender's HWND has
// to occupy wParam (it authenticates the sender, exactly as with WM_COPYDATA), so the command
// id and the value must share the single lParam -- and lParam is only 32 bits wide on 32-bit
// MPC-HC (host and player can be different bitness, so the extra width of a 64-bit lParam
// cannot be relied on). An 8-bit command id (up to 256 commands) leaves 24 bits for a signed
// value of +/-8388607, which covers every integer parameter in this API; a full 32-bit value
// would leave no room for the command id.
//
// Capability negotiation: after connecting, a host sends MPCINT_HELLO carrying its
// MPCAPI_INT_VERSION. MPC-HC records it and replies with its own MPCINT_HELLO. Once
// MPC-HC knows the host speaks this channel it sends change notifications through it
// instead of WM_COPYDATA; a host that never says HELLO keeps getting the WM_COPYDATA
// notifications, so old and new hosts both work.
#define MPCAPI_INT_MESSAGE_NAME  L"MPC-HC API Integer Message"
#define MPCAPI_INT_VERSION       1

// Pack/unpack the integer-channel lParam (both sides must use these).
#define MPCAPI_INT_MAKELPARAM(value, command) \
    ((LPARAM)((((DWORD)(command) & 0xFF) << 24) | ((DWORD)(value) & 0x00FFFFFF)))
#define MPCAPI_INT_COMMAND_OF(lparam)  ((WORD)(((DWORD)(DWORD_PTR)(lparam) >> 24) & 0xFF))
#define MPCAPI_INT_VALUE_OF(lparam) \
    ((int)((((DWORD)(DWORD_PTR)(lparam) & 0x00FFFFFF) ^ 0x00800000) - 0x00800000))

// Commands available on this channel carry a signed value that fits the 24-bit field
// (volume, mute, track indices, small state enums, jump/delay amounts). Commands that carry
// strings or structs, or a fractional value (playback position/seek, playback speed), stay
// WM_COPYDATA-only; see the per-command "INT API:" notes in MPCAPI_COMMAND above.
typedef enum MPCAPI_INT_COMMAND {
    MPCINT_HELLO                  = 1,  // host<->MPC: value = sender's MPCAPI_INT_VERSION

    // ==== Notifications MPC-HC -> host (mirror the CMD_* notifications) ====
    MPCINT_CURRENTVOLUME          = 2,  // value = current volume (0-100)          (CMD_CURRENTVOLUME)
    MPCINT_CURRENTMUTE            = 3,  // value = mute state (0 or 1)             (CMD_CURRENTMUTE)
    MPCINT_STATE                  = 8,  // value = MPC_LOADSTATE                   (CMD_STATE)
    MPCINT_PLAYMODE               = 9,  // value = MPC_PLAYSTATE                   (CMD_PLAYMODE)
    MPCINT_CURRENTAUDIOTRACK      = 10, // value = current audio track index      (CMD_CURRENTAUDIOTRACK)
    MPCINT_CURRENTSUBTITLETRACK   = 11, // value = current subtitle track index   (CMD_CURRENTSUBTITLETRACK)

    // ==== Commands host -> MPC-HC (mirror the CMD_* commands) ====
    MPCINT_SETVOLUME              = 4,  // value = volume (0-100)                  (CMD_SETVOLUME)
    MPCINT_SETMUTE                = 5,  // value = mute (0 or 1)                   (CMD_SETMUTE)
    MPCINT_GETVOLUME              = 6,  // request -> MPCINT_CURRENTVOLUME         (CMD_GETVOLUME)
    MPCINT_GETMUTE                = 7,  // request -> MPCINT_CURRENTMUTE           (CMD_GETMUTE)
    MPCINT_STOP                   = 20, // (CMD_STOP)
    MPCINT_CLOSEFILE              = 21, // (CMD_CLOSEFILE)
    MPCINT_PLAYPAUSE              = 22, // (CMD_PLAYPAUSE)
    MPCINT_PLAY                   = 23, // (CMD_PLAY)
    MPCINT_PAUSE                  = 24, // (CMD_PAUSE)
    MPCINT_CLEARPLAYLIST          = 25, // (CMD_CLEARPLAYLIST)
    MPCINT_STARTPLAYLIST          = 26, // (CMD_STARTPLAYLIST)
    MPCINT_TOGGLEFULLSCREEN       = 27, // (CMD_TOGGLEFULLSCREEN)
    MPCINT_JUMPFORWARDMED         = 28, // (CMD_JUMPFORWARDMED)
    MPCINT_JUMPBACKWARDMED        = 29, // (CMD_JUMPBACKWARDMED)
    MPCINT_INCREASEVOLUME         = 30, // (CMD_INCREASEVOLUME)
    MPCINT_DECREASEVOLUME         = 31, // (CMD_DECREASEVOLUME)
    MPCINT_SHADER_TOGGLE          = 32, // (CMD_SHADER_TOGGLE)
    MPCINT_CLOSEAPP               = 33, // (CMD_CLOSEAPP)
    MPCINT_SETAUDIOTRACK          = 40, // value = audio track index               (CMD_SETAUDIOTRACK)
    MPCINT_SETSUBTITLETRACK       = 41, // value = subtitle track index (-1 = off) (CMD_SETSUBTITLETRACK)
    MPCINT_JUMPOFNSECONDS         = 42, // value = seconds to jump (+/-)           (CMD_JUMPOFNSECONDS)
    MPCINT_SETAUDIODELAY          = 43, // value = audio delay in ms               (CMD_SETAUDIODELAY)
    MPCINT_SETSUBTITLEDELAY       = 44, // value = subtitle delay in ms            (CMD_SETSUBTITLEDELAY)
    MPCINT_GETCURRENTAUDIOTRACK   = 50, // request -> MPCINT_CURRENTAUDIOTRACK     (CMD_GETCURRENTAUDIOTRACK)
    MPCINT_GETCURRENTSUBTITLETRACK= 51, // request -> MPCINT_CURRENTSUBTITLETRACK  (CMD_GETCURRENTSUBTITLETRACK)
} MPCAPI_INT_COMMAND;
