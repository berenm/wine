/*
 *
 * Copyright 2014 Austin English
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
#include "config.h"

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "initguid.h"
#include "mfreadwrite.h"
#include "mfapi.h"
#include "mferror.h"

#include "wine/debug.h"
#include "wine/mfplat.h"

#if HAVE_LIBAVFORMAT_AVFORMAT_H
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif

#include <assert.h>

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}

typedef struct _srcreader
{
    IMFSourceReader IMFSourceReader_iface;
    LONG ref;
    IMFByteStream *stream;

#if HAVE_LIBAVFORMAT_AVFORMAT_H
    LPVOID buffer;
    SIZE_T buffer_size;
    AVIOContext *avio_ctx;
    AVFormatContext *fmt_ctx;

#define STREAM_COUNT 32
    BOOL selected[STREAM_COUNT];
    AVCodecContext *dec_ctx[STREAM_COUNT];
#endif
} srcreader;

static AVStream *GetStreamFromIndex(srcreader* reader, DWORD index)
{
    int stream_index = index;
    if (!reader->fmt_ctx)
        return NULL;

    if (index == MF_SOURCE_READER_MEDIASOURCE)
        stream_index = 0;
    else if (index == MF_SOURCE_READER_FIRST_VIDEO_STREAM)
        stream_index = av_find_best_stream(reader->fmt_ctx, AVMEDIA_TYPE_VIDEO, 0, -1, NULL, 0);
    else if (index == MF_SOURCE_READER_FIRST_AUDIO_STREAM)
        stream_index = av_find_best_stream(reader->fmt_ctx, AVMEDIA_TYPE_AUDIO, 0, -1, NULL, 0);

    if (stream_index < 0 || stream_index >= reader->fmt_ctx->nb_streams)
        return NULL;

    return reader->fmt_ctx->streams[stream_index];
}

static inline srcreader *impl_from_IMFSourceReader(IMFSourceReader *iface)
{
    return CONTAINING_RECORD(iface, srcreader, IMFSourceReader_iface);
}

static HRESULT WINAPI src_reader_QueryInterface(IMFSourceReader *iface, REFIID riid, void **out)
{
    srcreader *This = impl_from_IMFSourceReader(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), out);

    if(IsEqualGUID(riid, &IID_IUnknown) ||
       IsEqualGUID(riid, &IID_IMFSourceReader))
    {
        *out = &This->IMFSourceReader_iface;
    }
    else
    {
        FIXME("(%s, %p)\n", debugstr_guid(riid), out);
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*out);
    return S_OK;
}

static ULONG WINAPI src_reader_AddRef(IMFSourceReader *iface)
{
    srcreader *This = impl_from_IMFSourceReader(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    return ref;
}

static ULONG WINAPI src_reader_Release(IMFSourceReader *iface)
{
    srcreader *This = impl_from_IMFSourceReader(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    if (!ref)
    {
#if HAVE_LIBAVFORMAT_AVFORMAT_H
        if (This->fmt_ctx)
            avformat_close_input(&This->fmt_ctx);
        if (This->avio_ctx)
            av_freep(&This->avio_ctx);
        if (This->buffer)
            av_freep(This->buffer);
#endif
        if (This->stream)
            IMFByteStream_Release(This->stream);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI src_reader_GetStreamSelection(IMFSourceReader *iface, DWORD index, BOOL *selected)
{
    AVStream *stream;
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %p\n", This, index, selected);

    stream = GetStreamFromIndex(This, index);
    if (!stream)
        return MF_E_INVALIDSTREAMNUMBER;

    if (stream->index >= STREAM_COUNT)
        return MF_E_INVALIDSTREAMNUMBER;

    if (selected)
        *selected = This->selected[stream->index];

    return S_OK;
}

static HRESULT WINAPI src_reader_SetStreamSelection(IMFSourceReader *iface, DWORD index, BOOL selected)
{
    AVStream *stream;
    AVCodec *dec;
    AVCodecContext **dec_ctx;
    AVDictionary *opts;
    int ret;
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %d\n", This, index, selected);

    stream = GetStreamFromIndex(This, index);
    if (!stream)
        return MF_E_INVALIDSTREAMNUMBER;

    if (stream->index >= STREAM_COUNT)
        return MF_E_INVALIDSTREAMNUMBER;

    if (This->selected[stream->index] == selected)
        return S_OK;

    dec_ctx = &This->dec_ctx[stream->index];
    if (selected)
    {
        dec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!dec)
            return EINVAL;

        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx)
            return ENOMEM;

        ret = avcodec_parameters_to_context(*dec_ctx, stream->codecpar);
        if (ret < 0) {
            avcodec_free_context(dec_ctx);
            return ret;
        }

        av_dict_set(&opts, "refcounted_frames", "1", 0);
        ret = avcodec_open2(*dec_ctx, dec, &opts);
        if (ret < 0) {
            avcodec_free_context(dec_ctx);
            return ret;
        }
    }
    else
    {
        avcodec_free_context(dec_ctx);
    }

    This->selected[stream->index] = selected;

    return S_OK;
}

static HRESULT WINAPI src_reader_GetNativeMediaType(IMFSourceReader *iface, DWORD index, DWORD typeindex,
            IMFMediaType **type)
{
#if HAVE_LIBAVFORMAT_AVFORMAT_H
    AVStream *stream;
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %d, %p\n", This, index, typeindex, type);

    stream = GetStreamFromIndex(This, index);
    if (!stream)
        return MF_E_INVALIDSTREAMNUMBER;

    if (typeindex > 0)
        return MF_E_NO_MORE_TYPES;

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        stream->codecpar->codec_id == AV_CODEC_ID_H264)
    {
        MFCreateMediaType(type);
        IMFMediaType_SetGUID(*type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
        IMFMediaType_SetUINT64(*type, &MF_MT_FRAME_SIZE,
                               (((UINT64)stream->codecpar->width & 0xffffffff) << 32) |
                               (((UINT64)stream->codecpar->height & 0xffffffff) << 0));
        IMFMediaType_SetUINT64(*type, &MF_MT_FRAME_RATE,
                               (((UINT64)stream->avg_frame_rate.num & 0xffffffff) << 32) |
                               (((UINT64)stream->avg_frame_rate.den & 0xffffffff) << 0));
        if (stream->codecpar->sample_aspect_ratio.num == 0)
        {
            IMFMediaType_SetUINT64(*type, &MF_MT_PIXEL_ASPECT_RATIO, 0x0000000100000001llu);
        }
        else
        {
            IMFMediaType_SetUINT64(*type, &MF_MT_PIXEL_ASPECT_RATIO,
                                   (((UINT64)stream->codecpar->sample_aspect_ratio.num & 0xffffffff) << 32) |
                                   (((UINT64)stream->codecpar->sample_aspect_ratio.den & 0xffffffff) << 0));
        }
        return S_OK;
    }

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
        stream->codecpar->codec_id == AV_CODEC_ID_AAC)
    {
        MFCreateMediaType(type);
        IMFMediaType_SetGUID(*type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
        IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, &MFAudioFormat_AAC);
        return S_OK;
    }
#else
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %d, %p\n", This, index, typeindex, type);
#endif

    return E_NOTIMPL;
}

static HRESULT WINAPI src_reader_GetCurrentMediaType(IMFSourceReader *iface, DWORD index, IMFMediaType **type)
{
#if HAVE_LIBAVFORMAT_AVFORMAT_H
    AVStream *stream;
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %p\n", This, index, type);

    stream = GetStreamFromIndex(This, index);
    if (stream == NULL)
        return MF_E_INVALIDSTREAMNUMBER;

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        stream->codecpar->format == AV_PIX_FMT_YUV420P)
    {
        MFCreateMediaType(type);
        IMFMediaType_SetGUID(*type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, &MFVideoFormat_YV12);
        return S_OK;
    }

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
        stream->codecpar->format == AV_SAMPLE_FMT_FLT)
    {
        MFCreateMediaType(type);
        IMFMediaType_SetGUID(*type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
        IMFMediaType_SetGUID(*type, &MF_MT_SUBTYPE, &MFAudioFormat_Float);
        return S_OK;
    }
#else
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %p\n", This, index, type);
#endif

    return E_NOTIMPL;
}

static HRESULT WINAPI src_reader_SetCurrentMediaType(IMFSourceReader *iface, DWORD index, DWORD *reserved,
        IMFMediaType *type)
{
#if HAVE_LIBAVFORMAT_AVFORMAT_H
    AVStream *stream;
    GUID majorType;
    GUID subType;
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %p, %p\n", This, index, reserved, type);

    stream = GetStreamFromIndex(This, index);
    if (stream == NULL)
        return MF_E_INVALIDSTREAMNUMBER;

    IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &majorType);
    IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subType);

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        IsEqualGUID(&majorType, &MFMediaType_Video) &&
        IsEqualGUID(&subType, &MFVideoFormat_YV12))
    {
        TRACE("%p, 0x%08x, %p stream->codecpar->format = AV_PIX_FMT_YUV420P;\n", This, index, type);
        stream->codecpar->format = AV_PIX_FMT_YUV420P;
        return S_OK;
    }

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
        IsEqualGUID(&majorType, &MFMediaType_Audio) &&
        IsEqualGUID(&subType, &MFAudioFormat_Float))
    {
        TRACE("%p, 0x%08x, %p stream->codecpar->format = AV_SAMPLE_FMT_FLT;\n", This, index, type);
        stream->codecpar->format = AV_SAMPLE_FMT_FLT;
        return S_OK;
    }
#else
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %p\n", This, index, type);
#endif

    return E_NOTIMPL;
}

static HRESULT WINAPI src_reader_SetCurrentPosition(IMFSourceReader *iface, REFGUID format, REFPROPVARIANT position)
{
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, %s, %p\n", This, debugstr_guid(format), position);
    return E_NOTIMPL;
}

static int decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;

    *got_frame = 0;

    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }

        if (*got_frame) {
            if (frame->width != width || frame->height != height || frame->format != pix_fmt) {
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name(frame->format));
                return -1;
            }

            printf("video_frame%s n:%d coded_n:%d\n",
                   cached ? "(cached)" : "",
                   video_frame_count++, frame->coded_picture_number);

            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          pix_fmt, width, height);

            /* write to rawvideo file */
            fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    } else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);

        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                   cached ? "(cached)" : "",
                   audio_frame_count++, frame->nb_samples,
                   av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */
            fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
        }
    }

    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && refcount)
        av_frame_unref(frame);

    return decoded;
}

static HRESULT WINAPI src_reader_ReadSample(IMFSourceReader *iface, DWORD index,
        DWORD flags, DWORD *actualindex, DWORD *sampleflags, LONGLONG *timestamp,
        IMFSample **sample)
{
    AVStream *stream;
    AVPacket pkt;
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, 0x%08x, %p, %p, %p, %p\n", This, index, flags, actualindex,
          sampleflags, timestamp, sample);

    stream = GetStreamFromIndex(This, index);
    if (stream == NULL)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!This->selected[stream->index])
        return MF_E_INVALIDREQUEST;

    if (flags != 0)
        return E_NOTIMPL;

    if (This->callback)
    {
        if (actualindex)
            return E_INVALIDARG;
        if (sampleflags)
            return E_INVALIDARG;
        if (timestamp)
            return E_INVALIDARG;
    }
    else
    {
        return E_NOTIMPL;
        // if (!sampleflags)
        //     return E_POINTER;
        // if (actualindex)
        //     *actualindex = stream->index;
        // if (timestamp)
        //     *timestamp = 0;
        // *sampleflags = 0;
    }

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    while (av_read_frame(This->fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;

        do {
            ret = decode_packet(&got_frame, 0);
            if (ret < 0)
                break;
            pkt.data += ret;
            pkt.size -= ret;
        } while (pkt.size > 0);

        av_packet_unref(&orig_pkt);
    }

    pkt.data = NULL;
    pkt.size = 0;
    do {
        decode_packet(&got_frame, 1);
    } while (got_frame);

    return E_NOTIMPL;
}

static HRESULT WINAPI src_reader_Flush(IMFSourceReader *iface, DWORD index)
{
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x\n", This, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI src_reader_GetServiceForStream(IMFSourceReader *iface, DWORD index, REFGUID service,
        REFIID riid, void **object)
{
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %s, %s, %p\n", This, index, debugstr_guid(service), debugstr_guid(riid), object);
    return E_NOTIMPL;
}

static HRESULT WINAPI src_reader_GetPresentationAttribute(IMFSourceReader *iface, DWORD index,
        REFGUID guid, PROPVARIANT *attr)
{
    srcreader *This = impl_from_IMFSourceReader(iface);
    FIXME("%p, 0x%08x, %s, %p\n", This, index, debugstr_guid(guid), attr);

    if (IsEqualGUID(guid, &MF_PD_DURATION))
    {
        AVStream* stream = GetStreamFromIndex(This, index);
        if (!stream)
            return E_INVALIDARG;

        TRACE("%p, 0x%08x, MF_PD_DURATION %p attr->vt = VT_UI8; attr->uhVal.QuadPart = %llu;\n", This, index, attr, 10000000 * stream->duration * stream->time_base.num / stream->time_base.den);
        attr->vt = VT_UI8;
        attr->uhVal.QuadPart = 10000000 * stream->duration * stream->time_base.num / stream->time_base.den;
        return S_OK;
    }

    if (IsEqualGUID(guid, &MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS))
    {
        TRACE("%p, 0x%08x, MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS %p attr->vt = VT_UI4; attr->ulVal = 0;\n", This, index, attr);
        attr->vt = VT_UI4;
        attr->ulVal = 0;
        return S_OK;
    }

    return E_NOTIMPL;
}

struct IMFSourceReaderVtbl srcreader_vtbl =
{
    src_reader_QueryInterface,
    src_reader_AddRef,
    src_reader_Release,
    src_reader_GetStreamSelection,
    src_reader_SetStreamSelection,
    src_reader_GetNativeMediaType,
    src_reader_GetCurrentMediaType,
    src_reader_SetCurrentMediaType,
    src_reader_SetCurrentPosition,
    src_reader_ReadSample,
    src_reader_Flush,
    src_reader_GetServiceForStream,
    src_reader_GetPresentationAttribute
};

static int srcreader_ByteStreamReadPacket(void *opaque, uint8_t *buf, int buf_size)
{
    srcreader *This = (srcreader *)opaque;

    ULONG byte_read;
    IMFByteStream_Read(This->stream, buf, buf_size, &byte_read);

    return byte_read;
}

HRESULT WINAPI MFCreateSourceReaderFromByteStream(IMFByteStream *stream, IMFAttributes *attributes, IMFSourceReader **reader)
{
    srcreader *object;

    TRACE("%p, %p, %p\n", stream, attributes, reader);

    object = HeapAlloc( GetProcessHeap(), 0, sizeof(*object) );
    if(!object)
        return E_OUTOFMEMORY;

    object->ref = 1;
    object->IMFSourceReader_iface.lpVtbl = &srcreader_vtbl;
    object->stream = stream;
    IMFByteStream_AddRef(stream);

#if HAVE_LIBAVFORMAT_AVFORMAT_H
    // av_register_all();

    object->buffer_size = 4096;
    object->buffer = av_malloc(object->buffer_size);
    if (!object->buffer)
    {
        HeapFree(GetProcessHeap(), 0, object);
        return E_OUTOFMEMORY;
    }

    object->avio_ctx = avio_alloc_context(object->buffer, object->buffer_size, 0, object, &srcreader_ByteStreamReadPacket, NULL, NULL);
    if (!object->avio_ctx)
    {
        av_freep(object->buffer);
        HeapFree(GetProcessHeap(), 0, object);
        return E_OUTOFMEMORY;
    }

    object->fmt_ctx = avformat_alloc_context();
    if (!object->fmt_ctx)
    {
        av_freep(&object->avio_ctx);
        av_freep(object->buffer);
        HeapFree(GetProcessHeap(), 0, object);
        return E_OUTOFMEMORY;
    }

    object->fmt_ctx->pb = object->avio_ctx;
    object->fmt_ctx->probesize = object->buffer_size;

    if (avformat_open_input(&object->fmt_ctx, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input\n");
        assert(0);
    }

    if (avformat_find_stream_info(object->fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        assert(0);
    }

    av_dump_format(object->fmt_ctx, 0, "stream", 0);
#endif

    *reader = &object->IMFSourceReader_iface;
    return S_OK;
}

HRESULT WINAPI MFCreateSourceReaderFromMediaSource(IMFMediaSource *source, IMFAttributes *attributes,
                                                   IMFSourceReader **reader)
{
    return MFCreateSourceReaderFromByteStream(IMFMediaSource_GetStream(source), attributes, reader);
}
