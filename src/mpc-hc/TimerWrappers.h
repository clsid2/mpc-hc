/*
 * (C) 2013-2014 see Authors.txt
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
#include <utility>

template <class T>
class OnDemandTimer
{
public:
    typedef std::function<void()> TimerCallback;

    OnDemandTimer() = delete;
    OnDemandTimer(const OnDemandTimer&) = delete;
    OnDemandTimer& operator=(const OnDemandTimer&) = delete;

    OnDemandTimer(CWnd* pWnd, UINT_PTR nIDEvent, UINT nElapse, TIMERPROC lpTimerFunc = nullptr)
        : m_pWnd(pWnd)
        , m_nIDEvent(nIDEvent)
        , m_uElapse(nElapse)
        , m_lpTimerFunc(lpTimerFunc) {
        ASSERT(nIDEvent != 0);
    }

    ~OnDemandTimer() {
        if (!m_subscribers.empty() && m_pWnd->m_hWnd) {
            VERIFY(m_pWnd->KillTimer(m_nIDEvent));
        }
    }

    void Subscribe(T id, const TimerCallback& callback) {
        if (m_pWnd->m_hWnd) {
            const bool bWasEmpty = m_subscribers.empty();
            m_subscribers[id] = callback;
            if (bWasEmpty) {
                VERIFY(m_nIDEvent == m_pWnd->SetTimer(m_nIDEvent, m_uElapse, m_lpTimerFunc));
            }
        } else {
            ASSERT(FALSE);
        }
    }

    void Unsubscribe(T id) {
        const bool bWasEmpty = m_subscribers.empty();
        m_subscribers.erase(id);
        if (!bWasEmpty && m_subscribers.empty() && m_pWnd->m_hWnd) {
            VERIFY(m_pWnd->KillTimer(m_nIDEvent));
        }
    }

    void NotifySubscribers() {
        if (m_pWnd->m_hWnd) {
            auto subscribers = m_subscribers;
            for (const auto& kv : subscribers) {
                kv.second();
            }
        } else {
            ASSERT(FALSE);
        }
    }

protected:
    CWnd* const m_pWnd;
    const UINT_PTR m_nIDEvent;
    const UINT m_uElapse;
    const TIMERPROC m_lpTimerFunc;
    std::map<T, TimerCallback> m_subscribers;
};

template <class T>
class OneTimeTimerPool
{
public:
    typedef std::function<void()> TimerCallback;

    OneTimeTimerPool() = delete;
    OneTimeTimerPool(const OneTimeTimerPool&) = delete;
    OneTimeTimerPool& operator=(const OneTimeTimerPool&) = delete;

    OneTimeTimerPool(CWnd* pWnd, UINT_PTR nIDEventStart, UINT nIDEventPoolSize, TIMERPROC lpTimerFunc = nullptr)
        : m_pWnd(pWnd)
        , m_nIDEventStart(nIDEventStart)
        , m_nIDEventPoolSize(nIDEventPoolSize)
        , m_lpTimerFunc(lpTimerFunc) {
        ASSERT(nIDEventStart != 0);
        ASSERT(nIDEventPoolSize != 0);
        for (unsigned i = 0; i < nIDEventPoolSize; i++) {
            m_used[nIDEventStart + i] = false;
        }
    }

    ~OneTimeTimerPool() {
        if (m_pWnd) {
            for (const auto& kv : m_subscribers) {
                const UINT_PTR nIDEvent = kv.second.first;
                ASSERT(m_used[nIDEvent]);
                VERIFY(m_pWnd->KillTimer(nIDEvent));
            }
        } else {
            ASSERT(FALSE);
        }
    }

    void Subscribe(T id, const TimerCallback& callback, UINT nElapse) {
        if (m_pWnd) {
            Unsubscribe(id);
            const UINT_PTR nIDEventTarget = FindFree();
            ENSURE(nIDEventTarget != 0);
            const UINT_PTR nIDEvent = m_pWnd->SetTimer(nIDEventTarget, nElapse, m_lpTimerFunc);
            if (nIDEvent) {
                if (nIDEvent == nIDEventTarget) {
                    m_subscribers[id] = std::make_pair(nIDEvent, callback);
                    ASSERT(!m_used[nIDEvent]);
                    m_used[nIDEvent] = true;
                } else {
                    // shouldn't really happen
                    ASSERT(FALSE);
                    VERIFY(m_pWnd->KillTimer(nIDEvent));
                }
            } else {
                ASSERT(FALSE);
            }
        } else {
            ASSERT(FALSE);
        }
    }

    void Unsubscribe(T id) {
        if (m_pWnd) {
            auto size = m_subscribers.size();
            if (size > 0 && size < 256) {
                auto it = m_subscribers.find(id);
                if (it != m_subscribers.end()) {
                    const UINT_PTR nIDEvent = it->second.first;
                    ASSERT(m_used[nIDEvent]);
                    VERIFY(m_pWnd->KillTimer(nIDEvent));
                    m_subscribers.erase(it);
                    m_used[nIDEvent] = false;
                }
            } else {
                ASSERT(size == 0);
            }
        } else {
            ASSERT(FALSE);
        }
    }

    void NotifySubscribers(UINT_PTR nIDEvent) {
        if (m_pWnd) {
            auto size = m_subscribers.size();
            if (size > 0 && size < 256) {
                for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
                    if (it->second.first == nIDEvent) {
                        ASSERT(m_used[nIDEvent]);
                        VERIFY(m_pWnd->KillTimer(nIDEvent));
                        const TimerCallback cb = it->second.second;
                        m_subscribers.erase(it);
                        m_used[nIDEvent] = false;
                        cb();
                        break;
                    }
                }
            } else {
                ASSERT(size == 0);
            }
        } else {
            ASSERT(FALSE);
        }
    }

protected:
    inline UINT_PTR FindFree() const {
        UINT_PTR ret = 0;
        for (const auto& kv : m_used) {
            if (!kv.second) {
                ret = kv.first;
                break;
            }
        }
        return ret;
    }

    CWnd* const m_pWnd;
    const UINT_PTR m_nIDEventStart;
    const UINT_PTR m_nIDEventPoolSize;
    const TIMERPROC m_lpTimerFunc;
    std::map<T, std::pair<UINT_PTR, TimerCallback>> m_subscribers;
    std::map<UINT_PTR, bool> m_used;
};
