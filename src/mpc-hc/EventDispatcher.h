/*
 * (C) 2013-2016 see Authors.txt
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

#include <functional>
#include <map>
#include <memory>
#include <set>

enum class MpcEvent {
    SWITCHING_TO_FULLSCREEN,
    SWITCHED_TO_FULLSCREEN,
    SWITCHING_FROM_FULLSCREEN,
    SWITCHED_FROM_FULLSCREEN,
    SWITCHING_TO_FULLSCREEN_D3D,
    SWITCHED_TO_FULLSCREEN_D3D,
    MEDIA_LOADED,
    SHADER_PRERESIZE_SELECTION_CHANGED,
    SHADER_POSTRESIZE_SELECTION_CHANGED,
    SHADER_SELECTION_CHANGED,
    SHADER_LIST_CHANGED,
    DISPLAY_MODE_AUTOCHANGING,
    DISPLAY_MODE_AUTOCHANGED,
    CONTEXT_MENU_POPUP_INITIALIZED,
    CONTEXT_MENU_POPUP_UNINITIALIZED,
    SYSTEM_MENU_POPUP_INITIALIZED,
    SYSTEM_MENU_POPUP_UNINITIALIZED,
    CHANGING_UI_LANGUAGE,
    STREAM_POS_UPDATE_REQUEST,
    DPI_CHANGED,
    DEFAULT_TOOLBAR_SIZE_CHANGED,
    TOOLBAR_THEME_CHANGED,
};

class EventClient;

class EventRouter
{
    friend class EventClient;

public:
    typedef std::set<MpcEvent> EventSelection;
    typedef std::function<void(MpcEvent)> EventCallback;

private:
    struct EventRouterCore {
        DWORD m_tid;
        bool m_bDestroyed;
        struct EventClientInfo {
            EventSelection receives, fires;
            EventCallback callback;
        };
        std::map<EventClient*, EventClientInfo> m_conns;

        EventRouterCore();
        void FireEvent(MpcEvent ev);
    };
    std::shared_ptr<EventRouterCore> m_core;

public:
    EventRouter();
    ~EventRouter();

    void Connect(EventClient& node, const EventSelection& receives, const EventCallback& callback);
    void Connect(EventClient& node, const EventSelection& fires);
    void Connect(EventClient& node, const EventSelection& receives, const EventCallback& callback, const EventSelection& fires);
    void Disconnect(EventClient& node);
};

class EventClient
{
    friend class EventRouter;

private:
    std::set<std::shared_ptr<EventRouter::EventRouterCore>> m_conns;

public:
    ~EventClient();

    void FireEvent(MpcEvent ev);
};
