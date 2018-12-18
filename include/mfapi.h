/*
 * Copyright (C) 2015 Austin English
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

#ifndef __WINE_MFAPI_H
#define __WINE_MFAPI_H

#include <mfobjects.h>
#include <mmreg.h>
#include <avrt.h>

#if !defined(MF_VERSION)
/* Default to Windows XP */
#define MF_SDK_VERSION 0x0001
#define MF_API_VERSION 0x0070
#define MF_VERSION (MF_SDK_VERSION << 16 | MF_API_VERSION)
#endif

#define MFSTARTUP_NOSOCKET 0x1
#define MFSTARTUP_LITE     (MFSTARTUP_NOSOCKET)
#define MFSTARTUP_FULL     0

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |  \
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif

#define DEFINE_MEDIATYPE_GUID(name, format) \
    DEFINE_GUID(name, format, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

#ifndef DIRECT3D_VERSION
#define D3DFMT_X8R8G8B8     22
#endif

DEFINE_MEDIATYPE_GUID(MFVideoFormat_WMV3,      MAKEFOURCC('W','M','V','3'));
DEFINE_MEDIATYPE_GUID(MFVideoFormat_H264,      MAKEFOURCC('H','2','6','4'));
DEFINE_MEDIATYPE_GUID(MFVideoFormat_RGB32,     D3DFMT_X8R8G8B8);
DEFINE_MEDIATYPE_GUID(MFVideoFormat_YV12,      0x32315659);
DEFINE_MEDIATYPE_GUID(MFAudioFormat_Float,     0x3);
DEFINE_MEDIATYPE_GUID(MFAudioFormat_AAC,       0x1610);


DEFINE_GUID(MF_MT_AVG_BITRATE,         0x20332624, 0xfb0d, 0x4d9e, 0xbd, 0x0d, 0xcb, 0xf6, 0x78, 0x6c, 0x10, 0x2e);
DEFINE_GUID(MF_MT_FRAME_RATE,          0xc459a2e8, 0x3d2c, 0x4e44, 0xb1, 0x32, 0xfe, 0xe5, 0x15, 0x6c, 0x7b, 0xb0);
DEFINE_GUID(MF_MT_FRAME_SIZE,          0x1652c33d, 0xd6b2, 0x4012, 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d);
DEFINE_GUID(MF_MT_INTERLACE_MODE,      0xe2724bb8, 0xe676, 0x4806, 0xb4, 0xb2, 0xa8, 0xd6, 0xef, 0xb4, 0x4c, 0xcd);
DEFINE_GUID(MF_MT_MAJOR_TYPE,          0x48eba18e, 0xf8c9, 0x4687, 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f);
DEFINE_GUID(MF_MT_PIXEL_ASPECT_RATIO,  0xc6376a1e, 0x8d0a, 0x4027, 0xbe, 0x45, 0x6d, 0x9a, 0x0a, 0xd3, 0x9b, 0xb6);
DEFINE_GUID(MF_MT_SUBTYPE,             0xf7e34c9a, 0x42e8, 0x4714, 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5);
DEFINE_GUID(MF_MT_PAN_SCAN_ENABLED,    0x4b7f6bc3, 0x8b13, 0x40b2, 0xa9, 0x93, 0xab, 0xf6, 0x30, 0xb8, 0x20, 0x4e);
DEFINE_GUID(MF_MT_PAN_SCAN_APERTURE,   0x79614dde, 0x9187, 0x48fb, 0xb8, 0xc7, 0x4d, 0x52, 0x68, 0x9d, 0xe6, 0x49);
DEFINE_GUID(MF_MT_DEFAULT_STRIDE,      0x644b4e48, 0x1e02, 0x4516, 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6);
DEFINE_GUID(MF_MT_AUDIO_NUM_CHANNELS,  0x37e48bf5, 0x645e, 0x4c5b, 0x89, 0xde, 0xad, 0xa9, 0xe2, 0x9b, 0x69, 0x6a);
DEFINE_GUID(MF_MT_MINIMUM_DISPLAY_APERTURE, 0xd7388766, 0x18fe, 0x48c6, 0xa1, 0x77, 0xee, 0x89, 0x48, 0x67, 0xc8, 0xc4);

DEFINE_GUID(MF_PD_DURATION, 0x6c990d33, 0xbb8e, 0x477a, 0x85, 0x98, 0x0d, 0x5d, 0x96, 0xfc, 0xd8, 0x8a);

DEFINE_GUID(MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, 0x6d23f5c8, 0xc5d7, 0x4a9b, 0x99, 0x71, 0x5d, 0x11, 0xf8, 0xbc, 0xa8, 0x80);
DEFINE_GUID(MF_SOURCE_READER_DISABLE_DXVA,                0xaa456cfd, 0x3943, 0x4a1e, 0xa7, 0x7d, 0x18, 0x38, 0xc0, 0xea, 0x2e, 0x35);
DEFINE_GUID(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,     0xfb394f3d, 0xccf1, 0x42ee, 0xbb, 0xb3, 0xf9, 0xb8, 0x45, 0xd5, 0x68, 0x1d);
DEFINE_GUID(MF_SOURCE_READER_ASYNC_CALLBACK,              0x1e3dbeac, 0xbb43, 0x4c35, 0xb5, 0x07, 0xcd, 0x64, 0x44, 0x64, 0xc9, 0x65);

DEFINE_MEDIATYPE_GUID(MFMediaType_Video, MAKEFOURCC('v','i','d','s'));
DEFINE_MEDIATYPE_GUID(MFMediaType_Audio, MAKEFOURCC('a','u','d','s'));

typedef unsigned __int64 MFWORKITEM_KEY;

HRESULT WINAPI MFCancelWorkItem(MFWORKITEM_KEY key);
HRESULT WINAPI MFCopyImage(BYTE *dest, LONG deststride, const BYTE *src, LONG srcstride, DWORD width, DWORD lines);
HRESULT WINAPI MFCreateAttributes(IMFAttributes **attributes, UINT32 size);
HRESULT WINAPI MFCreateEventQueue(IMFMediaEventQueue **queue);
HRESULT WINAPI MFCreateFile(MF_FILE_ACCESSMODE accessmode, MF_FILE_OPENMODE openmode, MF_FILE_FLAGS flags,
                            LPCWSTR url, IMFByteStream **bytestream);
HRESULT WINAPI MFCreateMediaEvent(MediaEventType type, REFGUID extended_type, HRESULT status,
                                  const PROPVARIANT *value, IMFMediaEvent **event);
HRESULT WINAPI MFCreateMediaType(IMFMediaType **type);
HRESULT WINAPI MFCreateSample(IMFSample **sample);
HRESULT WINAPI MFCreateMemoryBuffer(DWORD max_length, IMFMediaBuffer **buffer);
HRESULT WINAPI MFGetTimerPeriodicity(DWORD *periodicity);
HRESULT WINAPI MFTEnum(GUID category, UINT32 flags, MFT_REGISTER_TYPE_INFO *input_type,
                       MFT_REGISTER_TYPE_INFO *output_type, IMFAttributes *attributes,
                       CLSID **pclsids, UINT32 *pcount);
HRESULT WINAPI MFTEnumEx(GUID category, UINT32 flags, const MFT_REGISTER_TYPE_INFO *input_type,
                         const MFT_REGISTER_TYPE_INFO *output_type, IMFActivate ***activate,
                         UINT32 *pcount);
HRESULT WINAPI MFLockPlatform(void);
HRESULT WINAPI MFTRegister(CLSID clsid, GUID category, LPWSTR name, UINT32 flags, UINT32 cinput,
                           MFT_REGISTER_TYPE_INFO *input_types, UINT32 coutput,
                           MFT_REGISTER_TYPE_INFO *output_types, IMFAttributes *attributes);
HRESULT WINAPI MFTRegisterLocal(IClassFactory *factory, REFGUID category, LPCWSTR name,
                           UINT32 flags, UINT32 cinput, const MFT_REGISTER_TYPE_INFO *input_types,
                           UINT32 coutput, const MFT_REGISTER_TYPE_INFO* output_types);
HRESULT WINAPI MFShutdown(void);
HRESULT WINAPI MFStartup(ULONG version, DWORD flags);
HRESULT WINAPI MFUnlockPlatform(void);
HRESULT WINAPI MFTUnregister(CLSID clsid);
HRESULT WINAPI MFTUnregisterLocal(IClassFactory *factory);
HRESULT WINAPI MFGetPluginControl(IMFPluginControl**);

#endif /* __WINE_MFAPI_H */
