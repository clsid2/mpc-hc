/*
* (C) 2025 see Authors.txt
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

// Declarations needed to use ISystemMediaTransportControls2 (Windows 10 1607+)
// when compiling against the Windows 8.1 SDK, whose windows.media.h lacks them.
// Copied from the Windows 10 SDK (10.0.26100.0) winrt/windows.media.h.
// When building with a Windows 10 SDK, windows.media.h already provides these
// declarations and this whole header compiles to nothing.
// Include after <windows.media.h>. Availability of the interface on the running
// system is checked at runtime via QueryInterface.

#ifndef ____x_ABI_CWindows_CMedia_CISystemMediaTransportControls2_INTERFACE_DEFINED__

namespace ABI {
    namespace Windows {
        namespace Media {
            class AutoRepeatModeChangeRequestedEventArgs;
            class PlaybackPositionChangeRequestedEventArgs;
            class PlaybackRateChangeRequestedEventArgs;
            class ShuffleEnabledChangeRequestedEventArgs;
        } /* Media */
    } /* Windows */
} /* ABI */

/*
 *
 * Struct Windows.Media.MediaPlaybackAutoRepeatMode
 *
 */
namespace ABI {
    namespace Windows {
        namespace Media {
            enum MediaPlaybackAutoRepeatMode : int
            {
                MediaPlaybackAutoRepeatMode_None = 0,
                MediaPlaybackAutoRepeatMode_Track = 1,
                MediaPlaybackAutoRepeatMode_List = 2,
            };
        } /* Media */
    } /* Windows */
} /* ABI */

/*
 *
 * Interface Windows.Media.IAutoRepeatModeChangeRequestedEventArgs
 *
 */
#if !defined(____x_ABI_CWindows_CMedia_CIAutoRepeatModeChangeRequestedEventArgs_INTERFACE_DEFINED__)
#define ____x_ABI_CWindows_CMedia_CIAutoRepeatModeChangeRequestedEventArgs_INTERFACE_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Media {
            MIDL_INTERFACE("ea137efa-d852-438e-882b-c990109a78f4")
            IAutoRepeatModeChangeRequestedEventArgs : public IInspectable
            {
            public:
                virtual HRESULT STDMETHODCALLTYPE get_RequestedAutoRepeatMode(
                    ABI::Windows::Media::MediaPlaybackAutoRepeatMode* value
                    ) = 0;
            };
        } /* Media */
    } /* Windows */
} /* ABI */
#endif /* !defined(____x_ABI_CWindows_CMedia_CIAutoRepeatModeChangeRequestedEventArgs_INTERFACE_DEFINED__) */

/*
 *
 * Interface Windows.Media.IPlaybackPositionChangeRequestedEventArgs
 *
 */
#if !defined(____x_ABI_CWindows_CMedia_CIPlaybackPositionChangeRequestedEventArgs_INTERFACE_DEFINED__)
#define ____x_ABI_CWindows_CMedia_CIPlaybackPositionChangeRequestedEventArgs_INTERFACE_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Media {
            MIDL_INTERFACE("b4493f88-eb28-4961-9c14-335e44f3e125")
            IPlaybackPositionChangeRequestedEventArgs : public IInspectable
            {
            public:
                virtual HRESULT STDMETHODCALLTYPE get_RequestedPlaybackPosition(
                    ABI::Windows::Foundation::TimeSpan* value
                    ) = 0;
            };
        } /* Media */
    } /* Windows */
} /* ABI */
#endif /* !defined(____x_ABI_CWindows_CMedia_CIPlaybackPositionChangeRequestedEventArgs_INTERFACE_DEFINED__) */

/*
 *
 * Interface Windows.Media.IPlaybackRateChangeRequestedEventArgs
 *
 */
#if !defined(____x_ABI_CWindows_CMedia_CIPlaybackRateChangeRequestedEventArgs_INTERFACE_DEFINED__)
#define ____x_ABI_CWindows_CMedia_CIPlaybackRateChangeRequestedEventArgs_INTERFACE_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Media {
            MIDL_INTERFACE("2ce2c41f-3cd6-4f77-9ba7-eb27c26a2140")
            IPlaybackRateChangeRequestedEventArgs : public IInspectable
            {
            public:
                virtual HRESULT STDMETHODCALLTYPE get_RequestedPlaybackRate(
                    DOUBLE* value
                    ) = 0;
            };
        } /* Media */
    } /* Windows */
} /* ABI */
#endif /* !defined(____x_ABI_CWindows_CMedia_CIPlaybackRateChangeRequestedEventArgs_INTERFACE_DEFINED__) */

/*
 *
 * Interface Windows.Media.IShuffleEnabledChangeRequestedEventArgs
 *
 */
#if !defined(____x_ABI_CWindows_CMedia_CIShuffleEnabledChangeRequestedEventArgs_INTERFACE_DEFINED__)
#define ____x_ABI_CWindows_CMedia_CIShuffleEnabledChangeRequestedEventArgs_INTERFACE_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Media {
            MIDL_INTERFACE("49b593fe-4fd0-4666-a314-c0e01940d302")
            IShuffleEnabledChangeRequestedEventArgs : public IInspectable
            {
            public:
                virtual HRESULT STDMETHODCALLTYPE get_RequestedShuffleEnabled(
                    boolean* value
                    ) = 0;
            };
        } /* Media */
    } /* Windows */
} /* ABI */
#endif /* !defined(____x_ABI_CWindows_CMedia_CIShuffleEnabledChangeRequestedEventArgs_INTERFACE_DEFINED__) */

#ifndef DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs_USE
#define DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("a6214bde-02d5-55b3-ab0d-c6031be70da1"))
ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::AutoRepeatModeChangeRequestedEventArgs*> : ITypedEventHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::ISystemMediaTransportControls*>, ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::AutoRepeatModeChangeRequestedEventArgs*, ABI::Windows::Media::IAutoRepeatModeChangeRequestedEventArgs*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.TypedEventHandler`2<Windows.Media.SystemMediaTransportControls, Windows.Media.AutoRepeatModeChangeRequestedEventArgs>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::AutoRepeatModeChangeRequestedEventArgs*> __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs_t;
#define __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs ABI::Windows::Foundation::__FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs_USE */

#ifndef DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs_USE
#define DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("44e34f15-bdc0-50a7-ace4-39e91fb753f1"))
ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::PlaybackPositionChangeRequestedEventArgs*> : ITypedEventHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::ISystemMediaTransportControls*>, ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::PlaybackPositionChangeRequestedEventArgs*, ABI::Windows::Media::IPlaybackPositionChangeRequestedEventArgs*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.TypedEventHandler`2<Windows.Media.SystemMediaTransportControls, Windows.Media.PlaybackPositionChangeRequestedEventArgs>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::PlaybackPositionChangeRequestedEventArgs*> __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs_t;
#define __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs ABI::Windows::Foundation::__FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs_USE */

#ifndef DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs_USE
#define DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("15eb0182-6366-5b9f-bd8c-8ab4fa9d7cd9"))
ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::PlaybackRateChangeRequestedEventArgs*> : ITypedEventHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::ISystemMediaTransportControls*>, ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::PlaybackRateChangeRequestedEventArgs*, ABI::Windows::Media::IPlaybackRateChangeRequestedEventArgs*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.TypedEventHandler`2<Windows.Media.SystemMediaTransportControls, Windows.Media.PlaybackRateChangeRequestedEventArgs>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::PlaybackRateChangeRequestedEventArgs*> __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs_t;
#define __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs ABI::Windows::Foundation::__FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs_USE */

#ifndef DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs_USE
#define DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("17ecea80-27e4-5dae-abb4-c858ad1c5307"))
ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::ShuffleEnabledChangeRequestedEventArgs*> : ITypedEventHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::ISystemMediaTransportControls*>, ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Media::ShuffleEnabledChangeRequestedEventArgs*, ABI::Windows::Media::IShuffleEnabledChangeRequestedEventArgs*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.TypedEventHandler`2<Windows.Media.SystemMediaTransportControls, Windows.Media.ShuffleEnabledChangeRequestedEventArgs>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::ShuffleEnabledChangeRequestedEventArgs*> __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs_t;
#define __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs ABI::Windows::Foundation::__FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs_USE */

/*
 *
 * Interface Windows.Media.ISystemMediaTransportControlsTimelineProperties
 *
 */
#if !defined(____x_ABI_CWindows_CMedia_CISystemMediaTransportControlsTimelineProperties_INTERFACE_DEFINED__)
#define ____x_ABI_CWindows_CMedia_CISystemMediaTransportControlsTimelineProperties_INTERFACE_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Media {
            MIDL_INTERFACE("5125316a-c3a2-475b-8507-93534dc88f15")
            ISystemMediaTransportControlsTimelineProperties : public IInspectable
            {
            public:
                virtual HRESULT STDMETHODCALLTYPE get_StartTime(
                    ABI::Windows::Foundation::TimeSpan* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_StartTime(
                    ABI::Windows::Foundation::TimeSpan value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE get_EndTime(
                    ABI::Windows::Foundation::TimeSpan* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_EndTime(
                    ABI::Windows::Foundation::TimeSpan value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE get_MinSeekTime(
                    ABI::Windows::Foundation::TimeSpan* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_MinSeekTime(
                    ABI::Windows::Foundation::TimeSpan value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE get_MaxSeekTime(
                    ABI::Windows::Foundation::TimeSpan* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_MaxSeekTime(
                    ABI::Windows::Foundation::TimeSpan value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE get_Position(
                    ABI::Windows::Foundation::TimeSpan* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_Position(
                    ABI::Windows::Foundation::TimeSpan value
                    ) = 0;
            };
        } /* Media */
    } /* Windows */
} /* ABI */
#endif /* !defined(____x_ABI_CWindows_CMedia_CISystemMediaTransportControlsTimelineProperties_INTERFACE_DEFINED__) */

/*
 *
 * Interface Windows.Media.ISystemMediaTransportControls2
 *
 */
#define ____x_ABI_CWindows_CMedia_CISystemMediaTransportControls2_INTERFACE_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Media {
            MIDL_INTERFACE("ea98d2f6-7f3c-4af2-a586-72889808efb1")
            ISystemMediaTransportControls2 : public IInspectable
            {
            public:
                virtual HRESULT STDMETHODCALLTYPE get_AutoRepeatMode(
                    ABI::Windows::Media::MediaPlaybackAutoRepeatMode* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_AutoRepeatMode(
                    ABI::Windows::Media::MediaPlaybackAutoRepeatMode value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE get_ShuffleEnabled(
                    boolean* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_ShuffleEnabled(
                    boolean value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE get_PlaybackRate(
                    DOUBLE* value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE put_PlaybackRate(
                    DOUBLE value
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE UpdateTimelineProperties(
                    ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties* timelineProperties
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE add_PlaybackPositionChangeRequested(
                    __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackPositionChangeRequestedEventArgs* handler,
                    EventRegistrationToken* token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE remove_PlaybackPositionChangeRequested(
                    EventRegistrationToken token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE add_PlaybackRateChangeRequested(
                    __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CPlaybackRateChangeRequestedEventArgs* handler,
                    EventRegistrationToken* token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE remove_PlaybackRateChangeRequested(
                    EventRegistrationToken token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE add_ShuffleEnabledChangeRequested(
                    __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CShuffleEnabledChangeRequestedEventArgs* handler,
                    EventRegistrationToken* token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE remove_ShuffleEnabledChangeRequested(
                    EventRegistrationToken token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE add_AutoRepeatModeChangeRequested(
                    __FITypedEventHandler_2_Windows__CMedia__CSystemMediaTransportControls_Windows__CMedia__CAutoRepeatModeChangeRequestedEventArgs* handler,
                    EventRegistrationToken* token
                    ) = 0;
                virtual HRESULT STDMETHODCALLTYPE remove_AutoRepeatModeChangeRequested(
                    EventRegistrationToken token
                    ) = 0;
            };
        } /* Media */
    } /* Windows */
} /* ABI */

#endif /* !defined(____x_ABI_CWindows_CMedia_CISystemMediaTransportControls2_INTERFACE_DEFINED__) */
