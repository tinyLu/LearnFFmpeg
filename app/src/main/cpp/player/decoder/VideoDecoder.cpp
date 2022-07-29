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

#include "macro.h"

thread  *m_PushThread;

SafeQueue<RTMPPacket *> packets;
VideoChannel *videoChannel;
int isStart;

int readyPushing;
uint32_t start_time;

extern void initPush();
extern void releasePush();

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

        initPush();
    } else {
        LOGCATE("VideoDecoder::OnDecoderReady m_VideoRender == null");
    }
}

void VideoDecoder::OnDecoderDone() {
    LOGCATE("VideoDecoder::OnDecoderDone");

    releasePush();

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
            //LOGCATE("VideoDecoder::OnFrameAvailable 14--^^--");

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

            if (videoChannel) {
                LOGCATE("VideoDecoder::OnFrameAvailable 222");
                videoChannel->encodeDataI420(frame->data[0], frame->linesize[0],
                                             frame->data[1], frame->linesize[1],
                                             frame->data[2], frame->linesize[2]);
            }

            /*int32_t size = frame->width * frame->height * sizeof(uint8_t) * 3 >> 1;

            LOGCATE("VideoDecoder::OnFrameAvailable eeee %d", size);

            libyuv::I420ToNV21(frame->data[0], frame->linesize[0],
                               frame->data[1], frame->linesize[1],
                               frame->data[2], frame->linesize[2],
                               m_buffer, frame->width,
                               m_buffer + frame->width * frame->height, frame->width,
                               frame->width,
                               frame->height
            );


            if (videoChannel) {
                LOGCATE("VideoDecoder::OnFrameAvailable 111");
                videoChannel->encodeData((int8_t *)m_buffer);
            }*/

            //pthread_mutex_unlock(&callback_mutex);


            /*FFMediaPlayer *player = static_cast<FFMediaPlayer *>(m_MsgContext);
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
                m_MsgCallback(m_MsgContext, MSG_DECODER_BITMAP, 0, jByteArray);*/

            //env->DeleteLocalRef(jByteArray);
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

void releasePackets(RTMPPacket *&packet) {
    if (packet) {
        RTMPPacket_Free(packet);
        delete packet;
        packet = 0;
    }
}

void callback(RTMPPacket *packet) {
    if (packet) {
        //设置时间戳
        packet->m_nTimeStamp = RTMP_GetTime() - start_time;
        packets.push(packet);
    }
}

void setPushVideoEncInfo(int width, int height, int fps, int bitrate) {
    if (videoChannel) {
        videoChannel->setVideoEncInfo(width, height, fps, bitrate);
    }
}

void start() {
    char *url = (char *)"rtmp://10.180.90.38:1935/live/bbb";
    RTMP *rtmp = 0;

    LOGCATE("VideoDecoder::start url=>%s", url);

    do {
        rtmp = RTMP_Alloc();
        if (!rtmp) {
            LOGE("alloc rtmp失败");
            break;
        }
        RTMP_Init(rtmp);
        int ret = RTMP_SetupURL(rtmp, url);
        if (!ret) {
            LOGE("设置地址失败:%s", url);
            break;
        }
        //5s超时时间
        rtmp->Link.timeout = 5;
        RTMP_EnableWrite(rtmp);
        ret = RTMP_Connect(rtmp, 0);
        if (!ret) {
            LOGE("连接服务器:%s", url);
            break;
        }
        ret = RTMP_ConnectStream(rtmp, 0);
        if (!ret) {
            LOGE("连接流:%s", url);
            break;
        }
        //记录一个开始时间
        start_time = RTMP_GetTime();
        //表示可以开始推流了
        readyPushing = 1;
        packets.setWork(1);
        //保证第一个数据是 aac解码数据包
        //callback(audioChannel->getAudioTag());
        RTMPPacket *packet = 0;
        while (readyPushing) {
            packets.pop(packet);
            if (!readyPushing) {
                break;
            }
            if (!packet) {
                continue;
            }
            packet->m_nInfoField2 = rtmp->m_stream_id;
            //发送rtmp包 1：队列
            // 意外断网？发送失败，rtmpdump 内部会调用RTMP_Close
            // RTMP_Close 又会调用 RTMP_SendPacket
            // RTMP_SendPacket  又会调用 RTMP_Close
            // 将rtmp.c 里面WriteN方法的 Rtmp_Close注释掉
            ret = RTMP_SendPacket(rtmp, packet, 1);
            releasePackets(packet);
            if (!ret) {
                LOGE("发送失败");
                break;
            }
        }
        releasePackets(packet);
    } while (0);
    //
    isStart = 0;
    readyPushing = 0;
    packets.setWork(0);
    packets.clear();
    if (rtmp) {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }
    delete (url);

    LOGCATE("VideoDecoder::start ...>");
}

void initPush() {
    LOGCATE("VideoDecoder::initPush <...");
    videoChannel = new VideoChannel;
    videoChannel->setVideoCallback(callback);
    //准备一个队列,打包好的数据 放入队列，在线程中统一的取出数据再发送给服务器
    packets.setReleaseCallback(releasePackets);

    setPushVideoEncInfo(1920, 1080, 25, 2000 * 1024);

    isStart = 0;
    if (isStart) {
        return;
    }
    isStart = 1;

    m_PushThread = new thread(start);
    LOGCATE("VideoDecoder::initPush ...>");
}

void releasePush() {
    LOGCATE("VideoDecoder::releasePush <...");
    readyPushing = 0;
    //关闭队列工作
    packets.setWork(0);
    DELETE(videoChannel);

    if(m_PushThread) {
        m_PushThread->join();
        delete m_PushThread;
        m_PushThread = nullptr;
    }

    LOGCATE("VideoDecoder::releasePush ...>");
}


