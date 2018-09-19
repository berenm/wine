/*
 * Unit tests for Direct Show functions
 *
 * Copyright (C) 2004 Christian Costa
 * Copyright (C) 2008 Alexander Dorofeyev
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#define CONST_VTABLE

#include "dshow.h"
#include "wine/heap.h"
#include "wine/test.h"

typedef struct TestFilterImpl
{
    IBaseFilter IBaseFilter_iface;

    LONG refCount;
    CRITICAL_SECTION csFilter;
    FILTER_STATE state;
    FILTER_INFO filterInfo;
    CLSID clsid;
    IPin **ppPins;
    UINT nPins;
} TestFilterImpl;

static const WCHAR avifile[] = {'t','e','s','t','.','a','v','i',0};
static const WCHAR mpegfile[] = {'t','e','s','t','.','m','p','g',0};

static WCHAR *load_resource(const WCHAR *name)
{
    static WCHAR pathW[MAX_PATH];
    DWORD written;
    HANDLE file;
    HRSRC res;
    void *ptr;

    GetTempPathW(ARRAY_SIZE(pathW), pathW);
    lstrcatW(pathW, name);

    file = CreateFileW(pathW, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "file creation failed, at %s, error %d\n", wine_dbgstr_w(pathW),
        GetLastError());

    res = FindResourceW(NULL, name, (LPCWSTR)RT_RCDATA);
    ok( res != 0, "couldn't find resource\n" );
    ptr = LockResource( LoadResource( GetModuleHandleA(NULL), res ));
    WriteFile( file, ptr, SizeofResource( GetModuleHandleA(NULL), res ), &written, NULL );
    ok( written == SizeofResource( GetModuleHandleA(NULL), res ), "couldn't write resource\n" );
    CloseHandle( file );

    return pathW;
}

static IFilterGraph2 *create_graph(void)
{
    IFilterGraph2 *ret;
    HRESULT hr;
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterGraph2, (void **)&ret);
    ok(hr == S_OK, "Failed to create FilterGraph: %#x\n", hr);
    return ret;
}

static void test_basic_video(IFilterGraph2 *graph)
{
    IBasicVideo* pbv;
    LONG video_width, video_height, window_width;
    LONG left, top, width, height;
    HRESULT hr;

    hr = IFilterGraph2_QueryInterface(graph, &IID_IBasicVideo, (void **)&pbv);
    ok(hr==S_OK, "Cannot get IBasicVideo interface returned: %x\n", hr);

    /* test get video size */
    hr = IBasicVideo_GetVideoSize(pbv, NULL, NULL);
    ok(hr==E_POINTER, "IBasicVideo_GetVideoSize returned: %x\n", hr);
    hr = IBasicVideo_GetVideoSize(pbv, &video_width, NULL);
    ok(hr==E_POINTER, "IBasicVideo_GetVideoSize returned: %x\n", hr);
    hr = IBasicVideo_GetVideoSize(pbv, NULL, &video_height);
    ok(hr==E_POINTER, "IBasicVideo_GetVideoSize returned: %x\n", hr);
    hr = IBasicVideo_GetVideoSize(pbv, &video_width, &video_height);
    ok(hr==S_OK, "Cannot get video size returned: %x\n", hr);

    /* test source position */
    hr = IBasicVideo_GetSourcePosition(pbv, NULL, NULL, NULL, NULL);
    ok(hr == E_POINTER, "IBasicVideo_GetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, &left, &top, NULL, NULL);
    ok(hr == E_POINTER, "IBasicVideo_GetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, NULL, NULL, &width, &height);
    ok(hr == E_POINTER, "IBasicVideo_GetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(left == 0, "expected 0, got %d\n", left);
    ok(top == 0, "expected 0, got %d\n", top);
    ok(width == video_width, "expected %d, got %d\n", video_width, width);
    ok(height == video_height, "expected %d, got %d\n", video_height, height);

    hr = IBasicVideo_SetSourcePosition(pbv, 0, 0, 0, 0);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, 0, 0, video_width*2, video_height*2);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_put_SourceTop(pbv, -1);
    ok(hr==E_INVALIDARG, "IBasicVideo_put_SourceTop returned: %x\n", hr);
    hr = IBasicVideo_put_SourceTop(pbv, 0);
    ok(hr==S_OK, "Cannot put source top returned: %x\n", hr);
    hr = IBasicVideo_put_SourceTop(pbv, 1);
    ok(hr==E_INVALIDARG, "IBasicVideo_put_SourceTop returned: %x\n", hr);

    hr = IBasicVideo_SetSourcePosition(pbv, video_width, 0, video_width, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, 0, video_height, video_width, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, -1, 0, video_width, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, 0, -1, video_width, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, video_width/2, video_height/2, video_width, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, video_width/2, video_height/2, video_width, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);

    hr = IBasicVideo_SetSourcePosition(pbv, 0, 0, video_width, video_height+1);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);
    hr = IBasicVideo_SetSourcePosition(pbv, 0, 0, video_width+1, video_height);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetSourcePosition returned: %x\n", hr);

    hr = IBasicVideo_SetSourcePosition(pbv, video_width/2, video_height/2, video_width/3+1, video_height/3+1);
    ok(hr==S_OK, "Cannot set source position returned: %x\n", hr);

    hr = IBasicVideo_get_SourceLeft(pbv, &left);
    ok(hr==S_OK, "Cannot get source left returned: %x\n", hr);
    ok(left==video_width/2, "expected %d, got %d\n", video_width/2, left);
    hr = IBasicVideo_get_SourceTop(pbv, &top);
    ok(hr==S_OK, "Cannot get source top returned: %x\n", hr);
    ok(top==video_height/2, "expected %d, got %d\n", video_height/2, top);
    hr = IBasicVideo_get_SourceWidth(pbv, &width);
    ok(hr==S_OK, "Cannot get source width returned: %x\n", hr);
    ok(width==video_width/3+1, "expected %d, got %d\n", video_width/3+1, width);
    hr = IBasicVideo_get_SourceHeight(pbv, &height);
    ok(hr==S_OK, "Cannot get source height returned: %x\n", hr);
    ok(height==video_height/3+1, "expected %d, got %d\n", video_height/3+1, height);

    hr = IBasicVideo_put_SourceLeft(pbv, video_width/3);
    ok(hr==S_OK, "Cannot put source left returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(left == video_width/3, "expected %d, got %d\n", video_width/3, left);
    ok(width == video_width/3+1, "expected %d, got %d\n", video_width/3+1, width);

    hr = IBasicVideo_put_SourceTop(pbv, video_height/3);
    ok(hr==S_OK, "Cannot put source top returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(top == video_height/3, "expected %d, got %d\n", video_height/3, top);
    ok(height == video_height/3+1, "expected %d, got %d\n", video_height/3+1, height);

    hr = IBasicVideo_put_SourceWidth(pbv, video_width/4+1);
    ok(hr==S_OK, "Cannot put source width returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(left == video_width/3, "expected %d, got %d\n", video_width/3, left);
    ok(width == video_width/4+1, "expected %d, got %d\n", video_width/4+1, width);

    hr = IBasicVideo_put_SourceHeight(pbv, video_height/4+1);
    ok(hr==S_OK, "Cannot put source height returned: %x\n", hr);
    hr = IBasicVideo_GetSourcePosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(top == video_height/3, "expected %d, got %d\n", video_height/3, top);
    ok(height == video_height/4+1, "expected %d, got %d\n", video_height/4+1, height);

    /* test destination rectangle */
    window_width = max(video_width, GetSystemMetrics(SM_CXMIN) - 2 * GetSystemMetrics(SM_CXFRAME));

    hr = IBasicVideo_GetDestinationPosition(pbv, NULL, NULL, NULL, NULL);
    ok(hr == E_POINTER, "IBasicVideo_GetDestinationPosition returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, &left, &top, NULL, NULL);
    ok(hr == E_POINTER, "IBasicVideo_GetDestinationPosition returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, NULL, NULL, &width, &height);
    ok(hr == E_POINTER, "IBasicVideo_GetDestinationPosition returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get destination position returned: %x\n", hr);
    ok(left == 0, "expected 0, got %d\n", left);
    ok(top == 0, "expected 0, got %d\n", top);
    todo_wine ok(width == window_width, "expected %d, got %d\n", window_width, width);
    todo_wine ok(height == video_height, "expected %d, got %d\n", video_height, height);

    hr = IBasicVideo_SetDestinationPosition(pbv, 0, 0, 0, 0);
    ok(hr==E_INVALIDARG, "IBasicVideo_SetDestinationPosition returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, 0, 0, video_width*2, video_height*2);
    ok(hr==S_OK, "Cannot put destination position returned: %x\n", hr);

    hr = IBasicVideo_put_DestinationLeft(pbv, -1);
    ok(hr==S_OK, "Cannot put destination left returned: %x\n", hr);
    hr = IBasicVideo_put_DestinationLeft(pbv, 0);
    ok(hr==S_OK, "Cannot put destination left returned: %x\n", hr);
    hr = IBasicVideo_put_DestinationLeft(pbv, 1);
    ok(hr==S_OK, "Cannot put destination left returned: %x\n", hr);

    hr = IBasicVideo_SetDestinationPosition(pbv, video_width, 0, video_width, video_height);
    ok(hr==S_OK, "Cannot set destinaiton position returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, 0, video_height, video_width, video_height);
    ok(hr==S_OK, "Cannot set destinaiton position returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, -1, 0, video_width, video_height);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, 0, -1, video_width, video_height);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, video_width/2, video_height/2, video_width, video_height);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, video_width/2, video_height/2, video_width, video_height);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);

    hr = IBasicVideo_SetDestinationPosition(pbv, 0, 0, video_width, video_height+1);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);
    hr = IBasicVideo_SetDestinationPosition(pbv, 0, 0, video_width+1, video_height);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);

    hr = IBasicVideo_SetDestinationPosition(pbv, video_width/2, video_height/2, video_width/3+1, video_height/3+1);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);

    hr = IBasicVideo_get_DestinationLeft(pbv, &left);
    ok(hr==S_OK, "Cannot get destination left returned: %x\n", hr);
    ok(left==video_width/2, "expected %d, got %d\n", video_width/2, left);
    hr = IBasicVideo_get_DestinationTop(pbv, &top);
    ok(hr==S_OK, "Cannot get destination top returned: %x\n", hr);
    ok(top==video_height/2, "expected %d, got %d\n", video_height/2, top);
    hr = IBasicVideo_get_DestinationWidth(pbv, &width);
    ok(hr==S_OK, "Cannot get destination width returned: %x\n", hr);
    ok(width==video_width/3+1, "expected %d, got %d\n", video_width/3+1, width);
    hr = IBasicVideo_get_DestinationHeight(pbv, &height);
    ok(hr==S_OK, "Cannot get destination height returned: %x\n", hr);
    ok(height==video_height/3+1, "expected %d, got %d\n", video_height/3+1, height);

    hr = IBasicVideo_put_DestinationLeft(pbv, video_width/3);
    ok(hr==S_OK, "Cannot put destination left returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(left == video_width/3, "expected %d, got %d\n", video_width/3, left);
    ok(width == video_width/3+1, "expected %d, got %d\n", video_width/3+1, width);

    hr = IBasicVideo_put_DestinationTop(pbv, video_height/3);
    ok(hr==S_OK, "Cannot put destination top returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(top == video_height/3, "expected %d, got %d\n", video_height/3, top);
    ok(height == video_height/3+1, "expected %d, got %d\n", video_height/3+1, height);

    hr = IBasicVideo_put_DestinationWidth(pbv, video_width/4+1);
    ok(hr==S_OK, "Cannot put destination width returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(left == video_width/3, "expected %d, got %d\n", video_width/3, left);
    ok(width == video_width/4+1, "expected %d, got %d\n", video_width/4+1, width);

    hr = IBasicVideo_put_DestinationHeight(pbv, video_height/4+1);
    ok(hr==S_OK, "Cannot put destination height returned: %x\n", hr);
    hr = IBasicVideo_GetDestinationPosition(pbv, &left, &top, &width, &height);
    ok(hr == S_OK, "Cannot get source position returned: %x\n", hr);
    ok(top == video_height/3, "expected %d, got %d\n", video_height/3, top);
    ok(height == video_height/4+1, "expected %d, got %d\n", video_height/4+1, height);

    /* reset source rectangle */
    hr = IBasicVideo_SetDefaultSourcePosition(pbv);
    ok(hr==S_OK, "IBasicVideo_SetDefaultSourcePosition returned: %x\n", hr);

    /* reset destination position */
    hr = IBasicVideo_SetDestinationPosition(pbv, 0, 0, video_width, video_height);
    ok(hr==S_OK, "Cannot set destination position returned: %x\n", hr);

    IBasicVideo_Release(pbv);
}

static void test_media_seeking(IFilterGraph2 *graph)
{
    IMediaSeeking *seeking;
    IMediaFilter *filter;
    LONGLONG pos, stop, duration;
    GUID format;
    HRESULT hr;

    IFilterGraph2_SetDefaultSyncSource(graph);
    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaSeeking, (void **)&seeking);
    ok(hr == S_OK, "QueryInterface(IMediaControl) failed: %08x\n", hr);

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaFilter, (void **)&filter);
    ok(hr == S_OK, "QueryInterface(IMediaFilter) failed: %08x\n", hr);

    format = GUID_NULL;
    hr = IMediaSeeking_GetTimeFormat(seeking, &format);
    ok(hr == S_OK, "GetTimeFormat failed: %#x\n", hr);
    ok(IsEqualGUID(&format, &TIME_FORMAT_MEDIA_TIME), "got %s\n", wine_dbgstr_guid(&format));

    pos = 0xdeadbeef;
    hr = IMediaSeeking_ConvertTimeFormat(seeking, &pos, NULL, 0x123456789a, NULL);
    ok(hr == S_OK, "ConvertTimeFormat failed: %#x\n", hr);
    ok(pos == 0x123456789a, "got %s\n", wine_dbgstr_longlong(pos));

    pos = 0xdeadbeef;
    hr = IMediaSeeking_ConvertTimeFormat(seeking, &pos, &TIME_FORMAT_MEDIA_TIME, 0x123456789a, NULL);
    ok(hr == S_OK, "ConvertTimeFormat failed: %#x\n", hr);
    ok(pos == 0x123456789a, "got %s\n", wine_dbgstr_longlong(pos));

    pos = 0xdeadbeef;
    hr = IMediaSeeking_ConvertTimeFormat(seeking, &pos, NULL, 0x123456789a, &TIME_FORMAT_MEDIA_TIME);
    ok(hr == S_OK, "ConvertTimeFormat failed: %#x\n", hr);
    ok(pos == 0x123456789a, "got %s\n", wine_dbgstr_longlong(pos));

    hr = IMediaSeeking_GetCurrentPosition(seeking, &pos);
    ok(hr == S_OK, "GetCurrentPosition failed: %#x\n", hr);
    ok(pos == 0, "got %s\n", wine_dbgstr_longlong(pos));

    hr = IMediaSeeking_GetDuration(seeking, &duration);
    ok(hr == S_OK, "GetDuration failed: %#x\n", hr);
    ok(duration > 0, "got %s\n", wine_dbgstr_longlong(duration));

    hr = IMediaSeeking_GetStopPosition(seeking, &stop);
    ok(hr == S_OK, "GetCurrentPosition failed: %08x\n", hr);
    ok(stop == duration || stop == duration + 1, "expected %s, got %s\n",
        wine_dbgstr_longlong(duration), wine_dbgstr_longlong(stop));

    hr = IMediaSeeking_SetPositions(seeking, NULL, AM_SEEKING_ReturnTime, NULL, AM_SEEKING_NoPositioning);
    ok(hr == S_OK, "SetPositions failed: %#x\n", hr);
    hr = IMediaSeeking_SetPositions(seeking, NULL, AM_SEEKING_NoPositioning, NULL, AM_SEEKING_ReturnTime);
    ok(hr == S_OK, "SetPositions failed: %#x\n", hr);

    pos = 0;
    hr = IMediaSeeking_SetPositions(seeking, &pos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
    ok(hr == S_OK, "SetPositions failed: %08x\n", hr);

    IMediaFilter_SetSyncSource(filter, NULL);
    pos = 0xdeadbeef;
    hr = IMediaSeeking_GetCurrentPosition(seeking, &pos);
    ok(hr == S_OK, "GetCurrentPosition failed: %08x\n", hr);
    ok(pos == 0, "Position != 0 (%s)\n", wine_dbgstr_longlong(pos));
    IFilterGraph2_SetDefaultSyncSource(graph);

    IMediaSeeking_Release(seeking);
    IMediaFilter_Release(filter);
}

static void test_state_change(IFilterGraph2 *graph)
{
    IMediaControl *control;
    OAFilterState state;
    HRESULT hr;

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    ok(hr == S_OK, "QueryInterface(IMediaControl) failed: %x\n", hr);

    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() failed: %x\n", hr);
    ok(state == State_Stopped, "wrong state %d\n", state);

    hr = IMediaControl_Run(control);
    ok(SUCCEEDED(hr), "Run() failed: %x\n", hr);
    hr = IMediaControl_GetState(control, INFINITE, &state);
    ok(SUCCEEDED(hr), "GetState() failed: %x\n", hr);
    ok(state == State_Running, "wrong state %d\n", state);

    hr = IMediaControl_Stop(control);
    ok(SUCCEEDED(hr), "Stop() failed: %x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() failed: %x\n", hr);
    ok(state == State_Stopped, "wrong state %d\n", state);

    hr = IMediaControl_Pause(control);
    ok(SUCCEEDED(hr), "Pause() failed: %x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() failed: %x\n", hr);
    ok(state == State_Paused, "wrong state %d\n", state);

    hr = IMediaControl_Run(control);
    ok(SUCCEEDED(hr), "Run() failed: %x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() failed: %x\n", hr);
    ok(state == State_Running, "wrong state %d\n", state);

    hr = IMediaControl_Pause(control);
    ok(SUCCEEDED(hr), "Pause() failed: %x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() failed: %x\n", hr);
    ok(state == State_Paused, "wrong state %d\n", state);

    hr = IMediaControl_Stop(control);
    ok(SUCCEEDED(hr), "Stop() failed: %x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() failed: %x\n", hr);
    ok(state == State_Stopped, "wrong state %d\n", state);

    IMediaControl_Release(control);
}

static void test_media_event(IFilterGraph2 *graph)
{
    IMediaEvent *media_event;
    IMediaSeeking *seeking;
    IMediaControl *control;
    IMediaFilter *filter;
    LONG_PTR lparam1, lparam2;
    LONGLONG current, stop;
    OAFilterState state;
    int got_eos = 0;
    HANDLE event;
    HRESULT hr;
    LONG code;

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaFilter, (void **)&filter);
    ok(hr == S_OK, "QueryInterface(IMediaFilter) failed: %#x\n", hr);

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    ok(hr == S_OK, "QueryInterface(IMediaControl) failed: %#x\n", hr);

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaEvent, (void **)&media_event);
    ok(hr == S_OK, "QueryInterface(IMediaEvent) failed: %#x\n", hr);

    hr = IFilterGraph2_QueryInterface(graph, &IID_IMediaSeeking, (void **)&seeking);
    ok(hr == S_OK, "QueryInterface(IMediaEvent) failed: %#x\n", hr);

    hr = IMediaControl_Stop(control);
    ok(SUCCEEDED(hr), "Stop() failed: %#x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() timed out\n");

    hr = IMediaSeeking_GetDuration(seeking, &stop);
    ok(hr == S_OK, "GetDuration() failed: %#x\n", hr);
    current = 0;
    hr = IMediaSeeking_SetPositions(seeking, &current, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
    ok(hr == S_OK, "SetPositions() failed: %#x\n", hr);

    hr = IMediaFilter_SetSyncSource(filter, NULL);
    ok(hr == S_OK, "SetSyncSource() failed: %#x\n", hr);

    hr = IMediaEvent_GetEventHandle(media_event, (OAEVENT *)&event);
    ok(hr == S_OK, "GetEventHandle() failed: %#x\n", hr);

    /* flush existing events */
    while ((hr = IMediaEvent_GetEvent(media_event, &code, &lparam1, &lparam2, 0)) == S_OK);

    ok(WaitForSingleObject(event, 0) == WAIT_TIMEOUT, "event should not be signaled\n");

    hr = IMediaControl_Run(control);
    ok(SUCCEEDED(hr), "Run() failed: %#x\n", hr);

    while (!got_eos)
    {
        if (WaitForSingleObject(event, 1000) == WAIT_TIMEOUT)
            break;

        while ((hr = IMediaEvent_GetEvent(media_event, &code, &lparam1, &lparam2, 0)) == S_OK)
        {
            if (code == EC_COMPLETE)
            {
                got_eos = 1;
                break;
            }
        }
    }
    ok(got_eos, "didn't get EOS\n");

    hr = IMediaSeeking_GetCurrentPosition(seeking, &current);
    ok(hr == S_OK, "GetCurrentPosition() failed: %#x\n", hr);
todo_wine
    ok(current == stop, "expected %s, got %s\n", wine_dbgstr_longlong(stop), wine_dbgstr_longlong(current));

    hr = IMediaControl_Stop(control);
    ok(SUCCEEDED(hr), "Run() failed: %#x\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "GetState() timed out\n");

    hr = IFilterGraph2_SetDefaultSyncSource(graph);
    ok(hr == S_OK, "SetDefaultSinkSource() failed: %#x\n", hr);

    IMediaSeeking_Release(seeking);
    IMediaEvent_Release(media_event);
    IMediaControl_Release(control);
    IMediaFilter_Release(filter);
}

static void rungraph(IFilterGraph2 *graph)
{
    test_basic_video(graph);
    test_media_seeking(graph);
    test_state_change(graph);
    test_media_event(graph);
}

static HRESULT test_graph_builder_connect(WCHAR *filename)
{
    static const WCHAR outputW[] = {'O','u','t','p','u','t',0};
    static const WCHAR inW[] = {'I','n',0};
    IBaseFilter *source_filter, *video_filter;
    IPin *pin_in, *pin_out;
    IFilterGraph2 *graph;
    IVideoWindow *window;
    HRESULT hr;

    graph = create_graph();

    hr = CoCreateInstance(&CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IVideoWindow, (void **)&window);
    ok(hr == S_OK, "Failed to create VideoRenderer: %#x\n", hr);

    hr = IFilterGraph2_AddSourceFilter(graph, filename, NULL, &source_filter);
    ok(hr == S_OK, "AddSourceFilter failed: %#x\n", hr);

    hr = IVideoWindow_QueryInterface(window, &IID_IBaseFilter, (void **)&video_filter);
    ok(hr == S_OK, "QueryInterface(IBaseFilter) failed: %#x\n", hr);
    hr = IFilterGraph2_AddFilter(graph, video_filter, NULL);
    ok(hr == S_OK, "AddFilter failed: %#x\n", hr);

    hr = IBaseFilter_FindPin(source_filter, outputW, &pin_out);
    ok(hr == S_OK, "FindPin failed: %#x\n", hr);
    hr = IBaseFilter_FindPin(video_filter, inW, &pin_in);
    ok(hr == S_OK, "FindPin failed: %#x\n", hr);
    hr = IFilterGraph2_Connect(graph, pin_out, pin_in);

    if (SUCCEEDED(hr))
        rungraph(graph);

    IPin_Release(pin_in);
    IPin_Release(pin_out);
    IBaseFilter_Release(source_filter);
    IBaseFilter_Release(video_filter);
    IVideoWindow_Release(window);
    IFilterGraph2_Release(graph);

    return hr;
}

static void test_render_run(const WCHAR *file)
{
    IFilterGraph2 *graph;
    HANDLE h;
    HRESULT hr;
    LONG refs;
    WCHAR *filename = load_resource(file);

    h = CreateFileW(filename, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        skip("Could not read test file %s, skipping test\n", wine_dbgstr_w(file));
        DeleteFileW(filename);
        return;
    }
    CloseHandle(h);

    trace("running %s\n", wine_dbgstr_w(file));

    graph = create_graph();

    hr = IFilterGraph2_RenderFile(graph, filename, NULL);
    if (FAILED(hr))
    {
        skip("%s: codec not supported; skipping test\n", wine_dbgstr_w(file));

        refs = IFilterGraph2_Release(graph);
        ok(!refs, "Graph has %u references\n", refs);

        hr = test_graph_builder_connect(filename);
todo_wine
        ok(hr == VFW_E_CANNOT_CONNECT, "got %#x\n", hr);
    }
    else
    {
        ok(hr == S_OK || hr == VFW_S_AUDIO_NOT_RENDERED, "RenderFile failed: %x\n", hr);
        rungraph(graph);

        refs = IFilterGraph2_Release(graph);
        ok(!refs, "Graph has %u references\n", refs);

        hr = test_graph_builder_connect(filename);
        ok(hr == S_OK || hr == VFW_S_PARTIAL_RENDER, "got %#x\n", hr);
    }

    /* check reference leaks */
    h = CreateFileW(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    ok(h != INVALID_HANDLE_VALUE, "CreateFile failed: err=%d\n", GetLastError());
    CloseHandle(h);

    DeleteFileW(filename);
}

static void test_enum_filters(void)
{
    IBaseFilter *filter1, *filter2, *filters[2];
    IFilterGraph2 *graph = create_graph();
    IEnumFilters *enum1, *enum2;
    ULONG count, ref;
    HRESULT hr;

    CoCreateInstance(&CLSID_AsyncReader, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter1);
    CoCreateInstance(&CLSID_AsyncReader, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter2);

    hr = IFilterGraph2_EnumFilters(graph, &enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Next(enum1, 1, filters, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    IFilterGraph2_AddFilter(graph, filter1, NULL);
    IFilterGraph2_AddFilter(graph, filter2, NULL);

    hr = IEnumFilters_Next(enum1, 1, filters, NULL);
    ok(hr == VFW_E_ENUM_OUT_OF_SYNC, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Next(enum1, 1, filters, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(filters[0] == filter2, "Got filter %p.\n", filters[0]);
    IBaseFilter_Release(filters[0]);

    hr = IEnumFilters_Next(enum1, 1, filters, &count);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(count == 1, "Got count %u.\n", count);
    ok(filters[0] == filter1, "Got filter %p.\n", filters[0]);
    IBaseFilter_Release(filters[0]);

    hr = IEnumFilters_Next(enum1, 1, filters, &count);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(count == 0, "Got count %u.\n", count);

    hr = IEnumFilters_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Next(enum1, 2, filters, &count);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(count == 2, "Got count %u.\n", count);
    ok(filters[0] == filter2, "Got filter %p.\n", filters[0]);
    ok(filters[1] == filter1, "Got filter %p.\n", filters[1]);
    IBaseFilter_Release(filters[0]);
    IBaseFilter_Release(filters[1]);

    IFilterGraph2_RemoveFilter(graph, filter1);
    IFilterGraph2_AddFilter(graph, filter1, NULL);

    hr = IEnumFilters_Next(enum1, 2, filters, &count);
    ok(hr == VFW_E_ENUM_OUT_OF_SYNC, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Next(enum1, 2, filters, &count);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(count == 2, "Got count %u.\n", count);
    ok(filters[0] == filter1, "Got filter %p.\n", filters[0]);
    ok(filters[1] == filter2, "Got filter %p.\n", filters[1]);
    IBaseFilter_Release(filters[0]);
    IBaseFilter_Release(filters[1]);

    hr = IEnumFilters_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Clone(enum1, &enum2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Skip(enum2, 1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumFilters_Next(enum2, 2, filters, &count);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(count == 1, "Got count %u.\n", count);
    ok(filters[0] == filter2, "Got filter %p.\n", filters[0]);
    IBaseFilter_Release(filters[0]);

    hr = IEnumFilters_Skip(enum1, 3);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    IEnumFilters_Release(enum2);
    IEnumFilters_Release(enum1);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static DWORD WINAPI call_RenderFile_multithread(LPVOID lParam)
{
    WCHAR *filename = load_resource(avifile);
    IFilterGraph2 *graph = lParam;
    HRESULT hr;

    hr = IFilterGraph2_RenderFile(graph, filename, NULL);
todo_wine
    ok(SUCCEEDED(hr), "RenderFile failed: %x\n", hr);

    if (SUCCEEDED(hr))
        rungraph(graph);

    return 0;
}

static void test_render_with_multithread(void)
{
    IFilterGraph2 *graph;
    HANDLE thread;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    graph = create_graph();

    thread = CreateThread(NULL, 0, call_RenderFile_multithread, graph, 0, NULL);

    ok(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0, "wait failed\n");
    IFilterGraph2_Release(graph);
    CloseHandle(thread);
    CoUninitialize();
}

static void test_graph_builder(void)
{
    HRESULT hr;
    IGraphBuilder *pgraph;
    IBaseFilter *pF = NULL;
    IBaseFilter *pF2 = NULL;
    IPin *pIn = NULL;
    IEnumPins *pEnum = NULL;
    PIN_DIRECTION dir;
    static const WCHAR testFilterW[] = {'t','e','s','t','F','i','l','t','e','r',0};
    static const WCHAR fooBarW[] = {'f','o','o','B','a','r',0};

    pgraph = (IGraphBuilder *)create_graph();

    /* create video filter */
    hr = CoCreateInstance(&CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (LPVOID*)&pF);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);
    ok(pF != NULL, "pF is NULL\n");

    hr = IGraphBuilder_AddFilter(pgraph, NULL, testFilterW);
    ok(hr == E_POINTER, "IGraphBuilder_AddFilter returned %x\n", hr);

    /* add the two filters to the graph */
    hr = IGraphBuilder_AddFilter(pgraph, pF, testFilterW);
    ok(hr == S_OK, "failed to add pF to the graph: %x\n", hr);

    /* find the pins */
    hr = IBaseFilter_EnumPins(pF, &pEnum);
    ok(hr == S_OK, "IBaseFilter_EnumPins failed for pF: %x\n", hr);
    ok(pEnum != NULL, "pEnum is NULL\n");
    hr = IEnumPins_Next(pEnum, 1, &pIn, NULL);
    ok(hr == S_OK, "IEnumPins_Next failed for pF: %x\n", hr);
    ok(pIn != NULL, "pIn is NULL\n");
    hr = IPin_QueryDirection(pIn, &dir);
    ok(hr == S_OK, "IPin_QueryDirection failed: %x\n", hr);
    ok(dir == PINDIR_INPUT, "pin has wrong direction\n");

    hr = IGraphBuilder_FindFilterByName(pgraph, fooBarW, &pF2);
    ok(hr == VFW_E_NOT_FOUND, "IGraphBuilder_FindFilterByName returned %x\n", hr);
    ok(pF2 == NULL, "IGraphBuilder_FindFilterByName returned %p\n", pF2);
    hr = IGraphBuilder_FindFilterByName(pgraph, testFilterW, &pF2);
    ok(hr == S_OK, "IGraphBuilder_FindFilterByName returned %x\n", hr);
    ok(pF2 != NULL, "IGraphBuilder_FindFilterByName returned NULL\n");
    hr = IGraphBuilder_FindFilterByName(pgraph, testFilterW, NULL);
    ok(hr == E_POINTER, "IGraphBuilder_FindFilterByName returned %x\n", hr);

    hr = IGraphBuilder_Connect(pgraph, NULL, pIn);
    ok(hr == E_POINTER, "IGraphBuilder_Connect returned %x\n", hr);

    hr = IGraphBuilder_Connect(pgraph, pIn, NULL);
    ok(hr == E_POINTER, "IGraphBuilder_Connect returned %x\n", hr);

    hr = IGraphBuilder_Connect(pgraph, pIn, pIn);
    ok(hr == VFW_E_CANNOT_CONNECT, "IGraphBuilder_Connect returned %x\n", hr);

    if (pIn) IPin_Release(pIn);
    if (pEnum) IEnumPins_Release(pEnum);
    if (pF) IBaseFilter_Release(pF);
    if (pF2) IBaseFilter_Release(pF2);
    IGraphBuilder_Release(pgraph);
}

struct testpin
{
    IPin IPin_iface;
    LONG ref;
    PIN_DIRECTION dir;
    IBaseFilter *filter;
    IPin *peer;

    IEnumMediaTypes IEnumMediaTypes_iface;
    const AM_MEDIA_TYPE *types;
    unsigned int type_count, enum_idx;
};

static inline struct testpin *impl_from_IEnumMediaTypes(IEnumMediaTypes *iface)
{
    return CONTAINING_RECORD(iface, struct testpin, IEnumMediaTypes_iface);
}

static HRESULT WINAPI testenummt_QueryInterface(IEnumMediaTypes *iface, REFIID iid, void **out)
{
    struct testpin *pin = impl_from_IEnumMediaTypes(iface);
    if (winetest_debug > 1) trace("%p->QueryInterface(%s)\n", pin, wine_dbgstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI testenummt_AddRef(IEnumMediaTypes *iface)
{
    struct testpin *pin = impl_from_IEnumMediaTypes(iface);
    return InterlockedIncrement(&pin->ref);
}

static ULONG WINAPI testenummt_Release(IEnumMediaTypes *iface)
{
    struct testpin *pin = impl_from_IEnumMediaTypes(iface);
    return InterlockedDecrement(&pin->ref);
}

static HRESULT WINAPI testenummt_Next(IEnumMediaTypes *iface, ULONG count, AM_MEDIA_TYPE **out, ULONG *fetched)
{
    struct testpin *pin = impl_from_IEnumMediaTypes(iface);
    unsigned int i;

    for (i = 0; i < count; ++i)
    {
        if (pin->enum_idx + i >= pin->type_count)
            break;

        out[i] = CoTaskMemAlloc(sizeof(*out[i]));
        *out[i] = pin->types[pin->enum_idx + i];
    }

    if (fetched)
        *fetched = i;
    pin->enum_idx += i;

    return (i == count) ? S_OK : S_FALSE;
}

static HRESULT WINAPI testenummt_Skip(IEnumMediaTypes *iface, ULONG count)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testenummt_Reset(IEnumMediaTypes *iface)
{
    struct testpin *pin = impl_from_IEnumMediaTypes(iface);
    pin->enum_idx = 0;
    return S_OK;
}

static HRESULT WINAPI testenummt_Clone(IEnumMediaTypes *iface, IEnumMediaTypes **out)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static const IEnumMediaTypesVtbl testenummt_vtbl =
{
    testenummt_QueryInterface,
    testenummt_AddRef,
    testenummt_Release,
    testenummt_Next,
    testenummt_Skip,
    testenummt_Reset,
    testenummt_Clone,
};

static inline struct testpin *impl_from_IPin(IPin *iface)
{
    return CONTAINING_RECORD(iface, struct testpin, IPin_iface);
}

static HRESULT WINAPI testpin_QueryInterface(IPin *iface, REFIID iid, void **out)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->QueryInterface(%s)\n", pin, wine_dbgstr_guid(iid));

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IPin))
    {
        *out = &pin->IPin_iface;
        IPin_AddRef(*out);
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

ULONG WINAPI testpin_AddRef(IPin *iface)
{
    struct testpin *pin = impl_from_IPin(iface);
    return InterlockedIncrement(&pin->ref);
}

ULONG WINAPI testpin_Release(IPin *iface)
{
    struct testpin *pin = impl_from_IPin(iface);
    return InterlockedDecrement(&pin->ref);
}

static HRESULT WINAPI testpin_Disconnect(IPin *iface)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->Disconnect()\n", pin);

    if (!pin->peer)
        return S_FALSE;

    IPin_Release(pin->peer);
    pin->peer = NULL;
    return S_OK;
}

static HRESULT WINAPI testpin_ConnectedTo(IPin *iface, IPin **peer)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->ConnectedTo()\n", pin);

    *peer = pin->peer;
    if (*peer)
    {
        IPin_AddRef(*peer);
        return S_OK;
    }
    return VFW_E_NOT_CONNECTED;
}

static HRESULT WINAPI testpin_ConnectionMediaType(IPin *iface, AM_MEDIA_TYPE *mt)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_QueryPinInfo(IPin *iface, PIN_INFO *info)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->QueryPinInfo()\n", pin);

    info->pFilter = pin->filter;
    IBaseFilter_AddRef(pin->filter);
    info->dir = pin->dir;
    info->achName[0] = 0;
    return S_OK;
}


static HRESULT WINAPI testpin_QueryDirection(IPin *iface, PIN_DIRECTION *dir)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->QueryDirection()\n", pin);

    *dir = pin->dir;
    return S_OK;
}

static HRESULT WINAPI testpin_QueryId(IPin *iface, WCHAR **id)
{
    if (winetest_debug > 1) trace("%p->QueryId()\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_QueryAccept(IPin *iface, const AM_MEDIA_TYPE *mt)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_EnumMediaTypes(IPin *iface, IEnumMediaTypes **out)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->EnumMediaTypes()\n", pin);

    *out = &pin->IEnumMediaTypes_iface;
    IEnumMediaTypes_AddRef(*out);
    pin->enum_idx = 0;
    return S_OK;
}

static HRESULT WINAPI testpin_QueryInternalConnections(IPin *iface, IPin **out, ULONG *count)
{
    if (winetest_debug > 1) trace("%p->QueryInternalConnections()\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_BeginFlush(IPin *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_EndFlush(IPin * iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_NewSegment(IPin *iface, REFERENCE_TIME start, REFERENCE_TIME stop, double rate)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testpin_EndOfStream(IPin *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testsink_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testsink_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->ReceiveConnection(%p)\n", pin, peer);

    pin->peer = peer;
    IPin_AddRef(peer);
    return S_OK;
}

static HRESULT WINAPI testsink_EnumMediaTypes(IPin *iface, IEnumMediaTypes **out)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static const IPinVtbl testsink_vtbl =
{
    testpin_QueryInterface,
    testpin_AddRef,
    testpin_Release,
    testsink_Connect,
    testsink_ReceiveConnection,
    testpin_Disconnect,
    testpin_ConnectedTo,
    testpin_ConnectionMediaType,
    testpin_QueryPinInfo,
    testpin_QueryDirection,
    testpin_QueryId,
    testpin_QueryAccept,
    testsink_EnumMediaTypes,
    testpin_QueryInternalConnections,
    testpin_EndOfStream,
    testpin_BeginFlush,
    testpin_EndFlush,
    testpin_NewSegment
};

static void testsink_init(struct testpin *pin)
{
    memset(pin, 0, sizeof(*pin));
    pin->IPin_iface.lpVtbl = &testsink_vtbl;
    pin->ref = 1;
    pin->dir = PINDIR_INPUT;

    pin->IEnumMediaTypes_iface.lpVtbl = &testenummt_vtbl;
}

static HRESULT WINAPI testsource_Connect(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct testpin *pin = impl_from_IPin(iface);
    if (winetest_debug > 1) trace("%p->Connect(%p)\n", pin, peer);

    ok(!mt, "Got media type %p.\n", mt);

    pin->peer = peer;
    IPin_AddRef(peer);
    return IPin_ReceiveConnection(peer, &pin->IPin_iface, mt);
}

static HRESULT WINAPI testsource_ReceiveConnection(IPin *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static const IPinVtbl testsource_vtbl =
{
    testpin_QueryInterface,
    testpin_AddRef,
    testpin_Release,
    testsource_Connect,
    testsource_ReceiveConnection,
    testpin_Disconnect,
    testpin_ConnectedTo,
    testpin_ConnectionMediaType,
    testpin_QueryPinInfo,
    testpin_QueryDirection,
    testpin_QueryId,
    testpin_QueryAccept,
    testpin_EnumMediaTypes,
    testpin_QueryInternalConnections,
    testpin_EndOfStream,
    testpin_BeginFlush,
    testpin_EndFlush,
    testpin_NewSegment
};

static void testsource_init(struct testpin *pin, const AM_MEDIA_TYPE *types, int type_count)
{
    memset(pin, 0, sizeof(*pin));
    pin->IPin_iface.lpVtbl = &testsource_vtbl;
    pin->ref = 1;
    pin->dir = PINDIR_OUTPUT;

    pin->IEnumMediaTypes_iface.lpVtbl = &testenummt_vtbl;
    pin->types = types;
    pin->type_count = type_count;
}

struct testfilter
{
    IBaseFilter IBaseFilter_iface;
    LONG ref;
    IFilterGraph *graph;
    WCHAR *name;

    IEnumPins IEnumPins_iface;
    struct testpin *pins;
    unsigned int pin_count, enum_idx;
};

static inline struct testfilter *impl_from_IEnumPins(IEnumPins *iface)
{
    return CONTAINING_RECORD(iface, struct testfilter, IEnumPins_iface);
}

static HRESULT WINAPI testenumpins_QueryInterface(IEnumPins *iface, REFIID iid, void **out)
{
    ok(0, "Unexpected iid %s.\n", wine_dbgstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI testenumpins_AddRef(IEnumPins * iface)
{
    struct testfilter *filter = impl_from_IEnumPins(iface);
    return InterlockedIncrement(&filter->ref);
}

static ULONG WINAPI testenumpins_Release(IEnumPins * iface)
{
    struct testfilter *filter = impl_from_IEnumPins(iface);
    return InterlockedDecrement(&filter->ref);
}

static HRESULT WINAPI testenumpins_Next(IEnumPins *iface, ULONG count, IPin **out, ULONG *fetched)
{
    struct testfilter *filter = impl_from_IEnumPins(iface);
    unsigned int i;

    for (i = 0; i < count; ++i)
    {
        if (filter->enum_idx + i >= filter->pin_count)
            break;

        out[i] = &filter->pins[filter->enum_idx + i].IPin_iface;
        IPin_AddRef(out[i]);
    }

    if (fetched)
        *fetched = i;
    filter->enum_idx += i;

    return (i == count) ? S_OK : S_FALSE;
}

static HRESULT WINAPI testenumpins_Skip(IEnumPins *iface, ULONG count)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testenumpins_Reset(IEnumPins *iface)
{
    struct testfilter *filter = impl_from_IEnumPins(iface);
    filter->enum_idx = 0;
    return S_OK;
}

static HRESULT WINAPI testenumpins_Clone(IEnumPins *iface, IEnumPins **out)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static const IEnumPinsVtbl testenumpins_vtbl =
{
    testenumpins_QueryInterface,
    testenumpins_AddRef,
    testenumpins_Release,
    testenumpins_Next,
    testenumpins_Skip,
    testenumpins_Reset,
    testenumpins_Clone,
};

static inline struct testfilter *impl_from_IBaseFilter(IBaseFilter *iface)
{
    return CONTAINING_RECORD(iface, struct testfilter, IBaseFilter_iface);
}

static HRESULT WINAPI testfilter_QueryInterface(IBaseFilter *iface, REFIID iid, void **out)
{
    struct testfilter *filter = impl_from_IBaseFilter(iface);
    if (winetest_debug > 1) trace("%p->QueryInterface(%s)\n", filter, wine_dbgstr_guid(iid));

    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IPersist)
            || IsEqualGUID(iid, &IID_IMediaFilter)
            || IsEqualGUID(iid, &IID_IBaseFilter))
    {
        *out = &filter->IBaseFilter_iface;
        IBaseFilter_AddRef(*out);
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI testfilter_AddRef(IBaseFilter *iface)
{
    struct testfilter *filter = impl_from_IBaseFilter(iface);
    return InterlockedIncrement(&filter->ref);
}

static ULONG WINAPI testfilter_Release(IBaseFilter *iface)
{
    struct testfilter *filter = impl_from_IBaseFilter(iface);
    return InterlockedDecrement(&filter->ref);
}

static HRESULT WINAPI testfilter_GetClassID(IBaseFilter *iface, CLSID *clsid)
{
    if (winetest_debug > 1) trace("%p->GetClassID()\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_Stop(IBaseFilter *iface)
{
    if (winetest_debug > 1) trace("%p->Stop()\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_Pause(IBaseFilter *iface)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_Run(IBaseFilter *iface, REFERENCE_TIME start)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_GetState(IBaseFilter *iface, DWORD timeout, FILTER_STATE *state)
{
    if (winetest_debug > 1) trace("%p->GetState()\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_SetSyncSource(IBaseFilter *iface, IReferenceClock *clock)
{
    if (winetest_debug > 1) trace("%p->SetSyncSource(%p)\n", iface, clock);
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_GetSyncSource(IBaseFilter *iface, IReferenceClock **clock)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_EnumPins(IBaseFilter *iface, IEnumPins **out)
{
    struct testfilter *filter = impl_from_IBaseFilter(iface);
    if (winetest_debug > 1) trace("%p->EnumPins()\n", filter);

    *out = &filter->IEnumPins_iface;
    IEnumPins_AddRef(*out);
    filter->enum_idx = 0;
    return S_OK;
}

static HRESULT WINAPI testfilter_FindPin(IBaseFilter *iface, const WCHAR *id, IPin **pin)
{
    ok(0, "Unexpected call.\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI testfilter_QueryFilterInfo(IBaseFilter *iface, FILTER_INFO *info)
{
    struct testfilter *filter = impl_from_IBaseFilter(iface);
    if (winetest_debug > 1) trace("%p->QueryFilterInfo()\n", filter);

    info->pGraph = filter->graph;
    if (filter->graph)
        IFilterGraph_AddRef(filter->graph);
    if (filter->name)
        lstrcpyW(info->achName, filter->name);
    else
        info->achName[0] = 0;
    return S_OK;
}

static HRESULT WINAPI testfilter_JoinFilterGraph(IBaseFilter *iface, IFilterGraph *graph, const WCHAR *name)
{
    struct testfilter *filter = impl_from_IBaseFilter(iface);
    if (winetest_debug > 1) trace("%p->JoinFilterGraph(%p, %s)\n", filter, graph, wine_dbgstr_w(name));

    filter->graph = graph;
    heap_free(filter->name);
    if (name)
    {
        filter->name = heap_alloc((lstrlenW(name)+1)*sizeof(WCHAR));
        lstrcpyW(filter->name, name);
    }
    else
        filter->name = NULL;
    return S_OK;
}

static HRESULT WINAPI testfilter_QueryVendorInfo(IBaseFilter * iface, WCHAR **info)
{
    return E_NOTIMPL;
}

static const IBaseFilterVtbl testfilter_vtbl =
{
    testfilter_QueryInterface,
    testfilter_AddRef,
    testfilter_Release,
    testfilter_GetClassID,
    testfilter_Stop,
    testfilter_Pause,
    testfilter_Run,
    testfilter_GetState,
    testfilter_SetSyncSource,
    testfilter_GetSyncSource,
    testfilter_EnumPins,
    testfilter_FindPin,
    testfilter_QueryFilterInfo,
    testfilter_JoinFilterGraph,
    testfilter_QueryVendorInfo
};

struct testfilter_cf
{
    IClassFactory IClassFactory_iface;
    struct testfilter *filter;
};

static void testfilter_init(struct testfilter *filter, struct testpin *pins, int pin_count)
{
    unsigned int i;

    memset(filter, 0, sizeof(*filter));
    filter->IBaseFilter_iface.lpVtbl = &testfilter_vtbl;
    filter->IEnumPins_iface.lpVtbl = &testenumpins_vtbl;
    filter->ref = 1;
    filter->pins = pins;
    filter->pin_count = pin_count;
    for (i = 0; i < pin_count; i++)
        pins[i].filter = &filter->IBaseFilter_iface;
}

static HRESULT WINAPI testfilter_cf_QueryInterface(IClassFactory *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IClassFactory))
    {
        *out = iface;
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI testfilter_cf_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI testfilter_cf_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI testfilter_cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    struct testfilter_cf *factory = CONTAINING_RECORD(iface, struct testfilter_cf, IClassFactory_iface);

    return IBaseFilter_QueryInterface(&factory->filter->IBaseFilter_iface, iid, out);
}

static HRESULT WINAPI testfilter_cf_LockServer(IClassFactory *iface, BOOL lock)
{
    return E_NOTIMPL;
}

static IClassFactoryVtbl testfilter_cf_vtbl =
{
    testfilter_cf_QueryInterface,
    testfilter_cf_AddRef,
    testfilter_cf_Release,
    testfilter_cf_CreateInstance,
    testfilter_cf_LockServer,
};

static void test_graph_builder_render(void)
{
    static const WCHAR testW[] = {'t','e','s','t',0};
    static const GUID sink1_clsid = {0x12345678};
    static const GUID sink2_clsid = {0x87654321};
    AM_MEDIA_TYPE source_type = {{0}};
    struct testpin source_pin, sink1_pin, sink2_pin, parser_pins[2];
    struct testfilter source, sink1, sink2, parser;
    struct testfilter_cf sink1_cf = { {&testfilter_cf_vtbl}, &sink1 };
    struct testfilter_cf sink2_cf = { {&testfilter_cf_vtbl}, &sink2 };

    IFilterGraph2 *graph = create_graph();
    REGFILTERPINS2 regpins = {0};
    REGPINTYPES regtypes = {0};
    REGFILTER2 regfilter = {0};
    IFilterMapper2 *mapper;
    DWORD cookie1, cookie2;
    HRESULT hr;
    ULONG ref;

    memset(&source_type.majortype, 0xcc, sizeof(GUID));
    testsource_init(&source_pin, &source_type, 1);
    testfilter_init(&source, &source_pin, 1);
    testsink_init(&sink1_pin);
    testfilter_init(&sink1, &sink1_pin, 1);
    testsink_init(&sink2_pin);
    testfilter_init(&sink2, &sink2_pin, 1);
    testsink_init(&parser_pins[0]);
    testsource_init(&parser_pins[1], &source_type, 1);
    testfilter_init(&parser, parser_pins, 2);

    IFilterGraph2_AddFilter(graph, &source.IBaseFilter_iface, NULL);
    IFilterGraph2_AddFilter(graph, &sink1.IBaseFilter_iface, NULL);
    IFilterGraph2_AddFilter(graph, &sink2.IBaseFilter_iface, NULL);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink2_pin.IPin_iface, "Got peer %p.\n", source_pin.peer);
    IFilterGraph2_Disconnect(graph, source_pin.peer);
    IFilterGraph2_Disconnect(graph, &source_pin.IPin_iface);

    IFilterGraph2_RemoveFilter(graph, &sink1.IBaseFilter_iface);
    IFilterGraph2_AddFilter(graph, &sink1.IBaseFilter_iface, NULL);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink1_pin.IPin_iface, "Got peer %p.\n", source_pin.peer);

    IFilterGraph2_Disconnect(graph, &source_pin.IPin_iface);
    IFilterGraph2_Disconnect(graph, &sink1_pin.IPin_iface);

    /* No preference is given to smaller chains. */

    IFilterGraph2_AddFilter(graph, &parser.IBaseFilter_iface, NULL);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &parser_pins[0].IPin_iface, "Got peer %p.\n", source_pin.peer);
    ok(parser_pins[1].peer == &sink1_pin.IPin_iface, "Got peer %p.\n", parser_pins[1].peer);
    IFilterGraph2_Disconnect(graph, source_pin.peer);
    IFilterGraph2_Disconnect(graph, &source_pin.IPin_iface);

    IFilterGraph2_RemoveFilter(graph, &sink1.IBaseFilter_iface);
    IFilterGraph2_AddFilter(graph, &sink1.IBaseFilter_iface, NULL);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink1_pin.IPin_iface, "Got peer %p.\n", source_pin.peer);

    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);

    /* Test enumeration of filters from the registry. */

    graph = create_graph();
    IFilterGraph2_AddFilter(graph, &source.IBaseFilter_iface, NULL);

    CoRegisterClassObject(&sink1_clsid, (IUnknown *)&sink1_cf.IClassFactory_iface,
            CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie1);
    CoRegisterClassObject(&sink2_clsid, (IUnknown *)&sink2_cf.IClassFactory_iface,
            CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie2);

    CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (void **)&mapper);

    regfilter.dwVersion = 2;
    regfilter.dwMerit = MERIT_UNLIKELY;
    regfilter.cPins2 = 1;
    regfilter.rgPins2 = &regpins;
    regpins.dwFlags = 0;
    regpins.cInstances = 1;
    regpins.nMediaTypes = 1;
    regpins.lpMediaType = &regtypes;
    regtypes.clsMajorType = &source_type.majortype;
    regtypes.clsMinorType = &MEDIASUBTYPE_NULL;
    hr = IFilterMapper2_RegisterFilter(mapper, &sink1_clsid, testW, NULL, NULL, NULL, &regfilter);
    if (hr == E_ACCESSDENIED)
    {
        skip("Not enough permission to register filters.\n");
        goto out;
    }
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    regpins.dwFlags = REG_PINFLAG_B_RENDERER;
    IFilterMapper2_RegisterFilter(mapper, &sink2_clsid, testW, NULL, NULL, NULL, &regfilter);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink2_pin.IPin_iface || source_pin.peer == &sink1_pin.IPin_iface,
            "Got peer %p.\n", source_pin.peer);

    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);

    /* Preference is given to filters already in the graph. */

    graph = create_graph();
    IFilterGraph2_AddFilter(graph, &source.IBaseFilter_iface, NULL);
    IFilterGraph2_AddFilter(graph, &sink2.IBaseFilter_iface, NULL);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink2_pin.IPin_iface, "Got peer %p.\n", source_pin.peer);

    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);

    /* No preference is given to renderer filters. */

    graph = create_graph();
    IFilterGraph2_AddFilter(graph, &source.IBaseFilter_iface, NULL);

    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink1_clsid);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink2_clsid);

    IFilterMapper2_RegisterFilter(mapper, &sink1_clsid, testW, NULL, NULL, NULL, &regfilter);
    regpins.dwFlags = 0;
    IFilterMapper2_RegisterFilter(mapper, &sink2_clsid, testW, NULL, NULL, NULL, &regfilter);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink2_pin.IPin_iface || source_pin.peer == &sink1_pin.IPin_iface,
            "Got peer %p.\n", source_pin.peer);

    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);

    /* Preference is given to filters with higher merit. */

    graph = create_graph();
    IFilterGraph2_AddFilter(graph, &source.IBaseFilter_iface, NULL);

    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink1_clsid);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink2_clsid);

    regfilter.dwMerit = MERIT_UNLIKELY;
    IFilterMapper2_RegisterFilter(mapper, &sink1_clsid, testW, NULL, NULL, NULL, &regfilter);
    regfilter.dwMerit = MERIT_PREFERRED;
    IFilterMapper2_RegisterFilter(mapper, &sink2_clsid, testW, NULL, NULL, NULL, &regfilter);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink2_pin.IPin_iface, "Got peer %p.\n", source_pin.peer);

    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);

    graph = create_graph();
    IFilterGraph2_AddFilter(graph, &source.IBaseFilter_iface, NULL);

    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink1_clsid);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink2_clsid);

    regfilter.dwMerit = MERIT_PREFERRED;
    IFilterMapper2_RegisterFilter(mapper, &sink1_clsid, testW, NULL, NULL, NULL, &regfilter);
    regfilter.dwMerit = MERIT_UNLIKELY;
    IFilterMapper2_RegisterFilter(mapper, &sink2_clsid, testW, NULL, NULL, NULL, &regfilter);

    hr = IFilterGraph2_Render(graph, &source_pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(source_pin.peer == &sink1_pin.IPin_iface, "Got peer %p.\n", source_pin.peer);

    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink1_clsid);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &sink2_clsid);

out:
    CoRevokeClassObject(cookie1);
    CoRevokeClassObject(cookie2);
    IFilterMapper2_Release(mapper);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
    ok(source.ref == 1, "Got outstanding refcount %d.\n", source.ref);
    ok(source_pin.ref == 1, "Got outstanding refcount %d.\n", source_pin.ref);
    ok(sink1.ref == 1, "Got outstanding refcount %d.\n", sink1.ref);
    ok(sink1_pin.ref == 1, "Got outstanding refcount %d.\n", sink1_pin.ref);
    ok(sink2.ref == 1, "Got outstanding refcount %d.\n", sink2.ref);
    ok(sink2_pin.ref == 1, "Got outstanding refcount %d.\n", sink2_pin.ref);
    ok(parser.ref == 1, "Got outstanding refcount %d.\n", parser.ref);
    ok(parser_pins[0].ref == 1, "Got outstanding refcount %d.\n", parser_pins[0].ref);
    ok(parser_pins[1].ref == 1, "Got outstanding refcount %d.\n", parser_pins[1].ref);
}

typedef struct IUnknownImpl
{
    IUnknown IUnknown_iface;
    int AddRef_called;
    int Release_called;
} IUnknownImpl;

static IUnknownImpl *IUnknownImpl_from_iface(IUnknown * iface)
{
    return CONTAINING_RECORD(iface, IUnknownImpl, IUnknown_iface);
}

static HRESULT WINAPI IUnknownImpl_QueryInterface(IUnknown * iface, REFIID riid, LPVOID * ppv)
{
    ok(0, "QueryInterface should not be called for %s\n", wine_dbgstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI IUnknownImpl_AddRef(IUnknown * iface)
{
    IUnknownImpl *This = IUnknownImpl_from_iface(iface);
    This->AddRef_called++;
    return 2;
}

static ULONG WINAPI IUnknownImpl_Release(IUnknown * iface)
{
    IUnknownImpl *This = IUnknownImpl_from_iface(iface);
    This->Release_called++;
    return 1;
}

static CONST_VTBL IUnknownVtbl IUnknownImpl_Vtbl =
{
    IUnknownImpl_QueryInterface,
    IUnknownImpl_AddRef,
    IUnknownImpl_Release
};

static void test_aggregate_filter_graph(void)
{
    HRESULT hr;
    IUnknown *pgraph;
    IUnknown *punk;
    IUnknownImpl unk_outer = { { &IUnknownImpl_Vtbl }, 0, 0 };

    hr = CoCreateInstance(&CLSID_FilterGraph, &unk_outer.IUnknown_iface, CLSCTX_INPROC_SERVER,
                          &IID_IUnknown, (void **)&pgraph);
    ok(hr == S_OK, "CoCreateInstance returned %x\n", hr);
    ok(pgraph != &unk_outer.IUnknown_iface, "pgraph = %p, expected not %p\n", pgraph, &unk_outer.IUnknown_iface);

    hr = IUnknown_QueryInterface(pgraph, &IID_IUnknown, (void **)&punk);
    ok(hr == S_OK, "CoCreateInstance returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 0, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 0, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);
    unk_outer.AddRef_called = 0;
    unk_outer.Release_called = 0;

    hr = IUnknown_QueryInterface(pgraph, &IID_IFilterMapper, (void **)&punk);
    ok(hr == S_OK, "CoCreateInstance returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 1, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 1, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);
    unk_outer.AddRef_called = 0;
    unk_outer.Release_called = 0;

    hr = IUnknown_QueryInterface(pgraph, &IID_IFilterMapper2, (void **)&punk);
    ok(hr == S_OK, "CoCreateInstance returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 1, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 1, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);
    unk_outer.AddRef_called = 0;
    unk_outer.Release_called = 0;

    hr = IUnknown_QueryInterface(pgraph, &IID_IFilterMapper3, (void **)&punk);
    ok(hr == S_OK, "CoCreateInstance returned %x\n", hr);
    ok(punk != &unk_outer.IUnknown_iface, "punk = %p, expected not %p\n", punk, &unk_outer.IUnknown_iface);
    IUnknown_Release(punk);

    ok(unk_outer.AddRef_called == 1, "IUnknownImpl_AddRef called %d times\n", unk_outer.AddRef_called);
    ok(unk_outer.Release_called == 1, "IUnknownImpl_Release called %d times\n", unk_outer.Release_called);

    IUnknown_Release(pgraph);
}

/* Test how methods from "control" interfaces (IBasicAudio, IBasicVideo,
 * IVideoWindow) are delegated to filters exposing those interfaces. */
static void test_control_delegation(void)
{
    IFilterGraph2 *graph = create_graph();
    IBasicAudio *audio, *filter_audio;
    IBaseFilter *renderer;
    IVideoWindow *window;
    IBasicVideo *video;
    HRESULT hr;
    LONG val;

    /* IBasicAudio */

    hr = IFilterGraph2_QueryInterface(graph, &IID_IBasicAudio, (void **)&audio);
    ok(hr == S_OK, "got %#x\n", hr);

    hr = IBasicAudio_put_Volume(audio, -10);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);
    hr = IBasicAudio_get_Volume(audio, &val);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);
    hr = IBasicAudio_put_Balance(audio, 10);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);
    hr = IBasicAudio_get_Balance(audio, &val);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);

    hr = CoCreateInstance(&CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&renderer);
    if (hr != VFW_E_NO_AUDIO_HARDWARE)
    {
        ok(hr == S_OK, "got %#x\n", hr);

        hr = IFilterGraph2_AddFilter(graph, renderer, NULL);
        ok(hr == S_OK, "got %#x\n", hr);

        hr = IBasicAudio_put_Volume(audio, -10);
        ok(hr == S_OK, "got %#x\n", hr);
        hr = IBasicAudio_get_Volume(audio, &val);
        ok(hr == S_OK, "got %#x\n", hr);
        ok(val == -10, "got %d\n", val);
        hr = IBasicAudio_put_Balance(audio, 10);
        ok(hr == S_OK || hr == VFW_E_MONO_AUDIO_HW, "got %#x\n", hr);
        hr = IBasicAudio_get_Balance(audio, &val);
        ok(hr == S_OK || hr == VFW_E_MONO_AUDIO_HW, "got %#x\n", hr);
        if (hr == S_OK)
            ok(val == 10, "got balance %d\n", val);

        hr = IBaseFilter_QueryInterface(renderer, &IID_IBasicAudio, (void **)&filter_audio);
        ok(hr == S_OK, "got %#x\n", hr);

        hr = IBasicAudio_get_Volume(filter_audio, &val);
        ok(hr == S_OK, "got %#x\n", hr);
        ok(val == -10, "got volume %d\n", val);

        hr = IFilterGraph2_RemoveFilter(graph, renderer);
        ok(hr == S_OK, "got %#x\n", hr);

        IBaseFilter_Release(renderer);
        IBasicAudio_Release(filter_audio);
    }

    hr = IBasicAudio_put_Volume(audio, -10);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);
    hr = IBasicAudio_get_Volume(audio, &val);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);
    hr = IBasicAudio_put_Balance(audio, 10);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);
    hr = IBasicAudio_get_Balance(audio, &val);
    ok(hr == E_NOTIMPL, "got %#x\n", hr);

    IBasicAudio_Release(audio);

    /* IBasicVideo and IVideoWindow */

    hr = IFilterGraph2_QueryInterface(graph, &IID_IBasicVideo, (void **)&video);
    ok(hr == S_OK, "got %#x\n", hr);
    hr = IFilterGraph2_QueryInterface(graph, &IID_IVideoWindow, (void **)&window);
    ok(hr == S_OK, "got %#x\n", hr);

    /* Unlike IBasicAudio, these return E_NOINTERFACE. */
    hr = IBasicVideo_get_BitRate(video, &val);
    ok(hr == E_NOINTERFACE, "got %#x\n", hr);
    hr = IVideoWindow_SetWindowForeground(window, OAFALSE);
    ok(hr == E_NOINTERFACE, "got %#x\n", hr);

    hr = CoCreateInstance(&CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&renderer);
    ok(hr == S_OK, "got %#x\n", hr);

    hr = IFilterGraph2_AddFilter(graph, renderer, NULL);
    ok(hr == S_OK, "got %#x\n", hr);

    hr = IBasicVideo_get_BitRate(video, &val);
    ok(hr == VFW_E_NOT_CONNECTED, "got %#x\n", hr);
    hr = IVideoWindow_SetWindowForeground(window, OAFALSE);
    ok(hr == VFW_E_NOT_CONNECTED, "got %#x\n", hr);

    hr = IFilterGraph2_RemoveFilter(graph, renderer);
    ok(hr == S_OK, "got %#x\n", hr);

    hr = IBasicVideo_get_BitRate(video, &val);
    ok(hr == E_NOINTERFACE, "got %#x\n", hr);
    hr = IVideoWindow_SetWindowForeground(window, OAFALSE);
    ok(hr == E_NOINTERFACE, "got %#x\n", hr);

    IBaseFilter_Release(renderer);
    IBasicVideo_Release(video);
    IVideoWindow_Release(window);
    IFilterGraph2_Release(graph);
}

START_TEST(filtergraph)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    test_render_run(avifile);
    test_render_run(mpegfile);
    test_enum_filters();
    test_graph_builder();
    test_graph_builder_render();
    test_aggregate_filter_graph();
    test_control_delegation();

    CoUninitialize();
    test_render_with_multithread();
}
