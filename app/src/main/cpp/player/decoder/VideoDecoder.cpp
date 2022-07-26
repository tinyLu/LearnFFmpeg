/**
 *
 * Created by 公众号：字节流动 on 2021/3/16.
 * https://github.com/githubhaohao/LearnFFmpeg
 * 最新文章首发于公众号：字节流动，有疑问或者技术交流可以添加微信 Byte-Flow ,领取视频教程, 拉你进技术交流群
 *
 * */


#include <FFMediaPlayer.h>
#include "VideoDecoder.h"
#include <android/bitmap.h>
#include "libyuv.h"

void VideoDecoder::OnDecoderReady() {
    LOGCATE("VideoDecoder::OnDecoderReady");
    m_VideoWidth = GetCodecContext()->width;
    m_VideoHeight = GetCodecContext()->height;

    if(m_MsgContext && m_MsgCallback)
        m_MsgCallback(m_MsgContext, MSG_DECODER_READY, 0, nullptr);

    if(m_VideoRender != nullptr) {
        int dstSize[2] = {0};
        m_VideoRender->Init(m_VideoWidth, m_VideoHeight, dstSize);
        m_RenderWidth = dstSize[0];
        m_RenderHeight = dstSize[1];

        if(m_VideoRender->GetRenderType() == VIDEO_RENDER_ANWINDOW) {
            int fps = 25;
            long videoBitRate = m_RenderWidth * m_RenderHeight * fps * 0.2;
            m_pVideoRecorder = new SingleVideoRecorder("/sdcard/learnffmpeg_output.mp4", m_RenderWidth, m_RenderHeight, videoBitRate, fps);
            m_pVideoRecorder->StartRecord();
        }

        m_RGBAFrame = av_frame_alloc();
        int bufferSize = av_image_get_buffer_size(DST_PIXEL_FORMAT, m_RenderWidth, m_RenderHeight, 1);
        m_FrameBuffer = (uint8_t *) av_malloc(bufferSize * sizeof(uint8_t));
        av_image_fill_arrays(m_RGBAFrame->data, m_RGBAFrame->linesize,
                             m_FrameBuffer, DST_PIXEL_FORMAT, m_RenderWidth, m_RenderHeight, 1);

        m_SwsContext = sws_getContext(m_VideoWidth, m_VideoHeight, GetCodecContext()->pix_fmt,
                                      m_RenderWidth, m_RenderHeight, DST_PIXEL_FORMAT,
                                      SWS_FAST_BILINEAR, NULL, NULL, NULL);

        int32_t size = m_VideoWidth * m_VideoHeight * sizeof(uint8_t) * 3 >> 1;
        m_buffer = (uint8_t *) av_malloc(size);

    } else {
        LOGCATE("VideoDecoder::OnDecoderReady m_VideoRender == null");
    }
}

void VideoDecoder::OnDecoderDone() {
    LOGCATE("VideoDecoder::OnDecoderDone");

    if(m_MsgContext && m_MsgCallback)
        m_MsgCallback(m_MsgContext, MSG_DECODER_DONE, 0, nullptr);

    if(m_VideoRender)
        m_VideoRender->UnInit();

    if (m_buffer != nullptr) {
        free(m_buffer);
        m_buffer = nullptr;
    }

    if (jByteArray && m_MsgContext) {
        LOGCATE("VideoDecoder::OnDecoderDone try release jByteArray?");
        FFMediaPlayer *player = static_cast<FFMediaPlayer *>(m_MsgContext);
        bool isAttach = false;
        JNIEnv *env = player->GetJNIEnv(&isAttach);
        if (nullptr != env) {
            env->DeleteLocalRef(jByteArray);
            jByteArray = nullptr;
            LOGCATE("VideoDecoder::OnDecoderDone DeleteLocalRef jByteArray ok!");
        }
    }

    if(m_RGBAFrame != nullptr) {
        av_frame_free(&m_RGBAFrame);
        m_RGBAFrame = nullptr;
    }

    if(m_FrameBuffer != nullptr) {
        free(m_FrameBuffer);
        m_FrameBuffer = nullptr;
    }

    if(m_SwsContext != nullptr) {
        sws_freeContext(m_SwsContext);
        m_SwsContext = nullptr;
    }

    if(m_pVideoRecorder != nullptr) {
        m_pVideoRecorder->StopRecord();
        delete m_pVideoRecorder;
        m_pVideoRecorder = nullptr;
    }

}

void VideoDecoder::OnFrameAvailable(AVFrame *frame) {
    LOGCATE("VideoDecoder::OnFrameAvailable frame=%p", frame);
    if(m_VideoRender != nullptr && frame != nullptr) {
        NativeImage image;
        LOGCATE("VideoDecoder::OnFrameAvailable 12 frame[w,h]=[%d, %d],format=%d,[line0,line1,line2]=[%d, %d, %d]", frame->width, frame->height, GetCodecContext()->pix_fmt, frame->linesize[0], frame->linesize[1],frame->linesize[2]);

        if (nullptr != m_MsgContext) {
            LOGCATE("VideoDecoder::OnFrameAvailable 14--^^--");

            /*if (nullptr == bitMap) {
                bitMap = player->CreateBitmap(env, m_RenderWidth, m_RenderHeight);
                LOGCATE("VideoDecoder::OnFrameAvailable create bitmap");
            }

            void *dstPixels = 0;
            AndroidBitmapInfo dstBitmapInfo;
            AndroidBitmap_getInfo(env, bitMap, &dstBitmapInfo);
            LOGCATE("VideoDecoder::OnFrameAvailable dstWidth=%d, dstHeight=%d", dstBitmapInfo.width, dstBitmapInfo.height);


            AndroidBitmap_lockPixels(env, bitMap, &dstPixels);

            LOGCATE("VideoDecoder::OnFrameAvailable ccc");

            libyuv::I420ToABGR(frame->data[0], frame->linesize[0],
                               frame->data[1], frame->linesize[1],
                               frame->data[2], frame->linesize[2],
                               (uint8_t *)dstPixels, frame->width * 4,
                               frame->width, frame->height
                               );

            LOGCATE("VideoDecoder::OnFrameAvailable ddd");
            AndroidBitmap_unlockPixels(env, bitMap);*/

            //pthread_mutex_lock(&callback_mutex);

            int32_t size = frame->width * frame->height * sizeof(uint8_t) * 3 >> 1;

            LOGCATE("VideoDecoder::OnFrameAvailable eeee %d", size);

            libyuv::I420ToNV21(frame->data[0], frame->linesize[0],
                               frame->data[1], frame->linesize[1],
                               frame->data[2], frame->linesize[2],
                               m_buffer, frame->width,
                               m_buffer + frame->width * frame->height, frame->width,
                               frame->width,
                               frame->height
            );


            FFMediaPlayer *player = static_cast<FFMediaPlayer *>(m_MsgContext);
            bool isAttach = false;
            JNIEnv *env = player->GetJNIEnv(&isAttach);

            LOGCATE("VideoDecoder::OnFrameAvailable fff %d  %d", size, isAttach);

            if (nullptr != env) {
                if (nullptr == jByteArray) {

                    jbyteArray jArray = env->NewByteArray(size);

                    jByteArray = jArray;

                    //jobject  jo = env->NewGlobalRef(jArray);
                }

                env->SetByteArrayRegion(jByteArray, 0, size, (jbyte *)m_buffer);
            }

            if(m_MsgContext && m_MsgCallback)
                m_MsgCallback(m_MsgContext, MSG_DECODER_BITMAP, 0, jByteArray);

            //env->DeleteLocalRef(jByteArray);

            //pthread_mutex_unlock(&callback_mutex);

        }

        return;

        if(m_VideoRender->GetRenderType() == VIDEO_RENDER_ANWINDOW)
        {
            sws_scale(m_SwsContext, frame->data, frame->linesize, 0,
                      m_VideoHeight, m_RGBAFrame->data, m_RGBAFrame->linesize);

            image.format = IMAGE_FORMAT_RGBA;
            image.width = m_RenderWidth;
            image.height = m_RenderHeight;
            image.ppPlane[0] = m_RGBAFrame->data[0];
            image.pLineSize[0] = image.width * 4;

            LOGCATE("VideoDecoder::OnFrameAvailable 13 frame[w,h]=[%d, %d],format=%d,[line0,line1,line2]=[%d, %d, %d]", m_RGBAFrame->width, m_RGBAFrame->height, m_RGBAFrame->format, m_RGBAFrame->linesize[0], m_RGBAFrame->linesize[1],m_RGBAFrame->linesize[2]);
            return;
        } else if(GetCodecContext()->pix_fmt == AV_PIX_FMT_YUV420P || GetCodecContext()->pix_fmt == AV_PIX_FMT_YUVJ420P) {
            image.format = IMAGE_FORMAT_I420;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.pLineSize[1] = frame->linesize[1];
            image.pLineSize[2] = frame->linesize[2];
            image.ppPlane[0] = frame->data[0];
            image.ppPlane[1] = frame->data[1];
            image.ppPlane[2] = frame->data[2];
            if(frame->data[0] && frame->data[1] && !frame->data[2] && frame->linesize[0] == frame->linesize[1] && frame->linesize[2] == 0) {
                // on some android device, output of h264 mediacodec decoder is NV12 兼容某些设备可能出现的格式不匹配问题
                image.format = IMAGE_FORMAT_NV12;
            }
        } else if (GetCodecContext()->pix_fmt == AV_PIX_FMT_NV12) {
            image.format = IMAGE_FORMAT_NV12;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.pLineSize[1] = frame->linesize[1];
            image.ppPlane[0] = frame->data[0];
            image.ppPlane[1] = frame->data[1];
        } else if (GetCodecContext()->pix_fmt == AV_PIX_FMT_NV21) {
            image.format = IMAGE_FORMAT_NV21;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.pLineSize[1] = frame->linesize[1];
            image.ppPlane[0] = frame->data[0];
            image.ppPlane[1] = frame->data[1];
        } else if (GetCodecContext()->pix_fmt == AV_PIX_FMT_RGBA) {
            image.format = IMAGE_FORMAT_RGBA;
            image.width = frame->width;
            image.height = frame->height;
            image.pLineSize[0] = frame->linesize[0];
            image.ppPlane[0] = frame->data[0];
        } else {
            sws_scale(m_SwsContext, frame->data, frame->linesize, 0,
                      m_VideoHeight, m_RGBAFrame->data, m_RGBAFrame->linesize);
            image.format = IMAGE_FORMAT_RGBA;
            image.width = m_RenderWidth;
            image.height = m_RenderHeight;
            image.ppPlane[0] = m_RGBAFrame->data[0];
            image.pLineSize[0] = image.width * 4;
        }

        m_VideoRender->RenderVideoFrame(&image);

        if(m_pVideoRecorder != nullptr) {
            m_pVideoRecorder->OnFrame2Encode(&image);
        }
    }

    if(m_MsgContext && m_MsgCallback)
        m_MsgCallback(m_MsgContext, MSG_REQUEST_RENDER, 0, nullptr);
}
