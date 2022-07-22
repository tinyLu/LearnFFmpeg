/**
 *
 * Created by 公众号：字节流动 on 2021/3/16.
 * https://github.com/githubhaohao/LearnFFmpeg
 * 最新文章首发于公众号：字节流动，有疑问或者技术交流可以添加微信 Byte-Flow ,领取视频教程, 拉你进技术交流群
 *
 * */


#include "DecoderBase.h"
#include "LogUtil.h"
#include "../../util/LogUtil.h"

void DecoderBase::Start() {
    if(m_Thread == nullptr) {
        StartDecodingThread();
    } else {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_DecoderState = STATE_DECODING;
        m_Cond.notify_all();
    }
}

void DecoderBase::Pause() {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_DecoderState = STATE_PAUSE;
}

void DecoderBase::Stop() {
    LOGCATE("DecoderBase::Stop");
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_DecoderState = STATE_STOP;
    m_Cond.notify_all();
}

void DecoderBase::SeekToPosition(float position) {
    LOGCATE("DecoderBase::SeekToPosition position=%f", position);
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_SeekPosition = position;
    m_DecoderState = STATE_DECODING;
    m_Cond.notify_all();
}

float DecoderBase::GetCurrentPosition() {
    //std::unique_lock<std::mutex> lock(m_Mutex);//读写保护
    //单位 ms
    return m_CurTimeStamp;
}

int DecoderBase::Init(const char *url, AVMediaType mediaType) {
    LOGCATE("DecoderBase::Init url=%s, mediaType=%d", url, mediaType);
    strcpy(m_Url,url);
    m_MediaType = mediaType;
    return 0;
}

void DecoderBase::UnInit() {
    LOGCATE("DecoderBase::UnInit m_MediaType=%d", m_MediaType);
    if(m_Thread) {
        Stop();
        m_Thread->join();
        delete m_Thread;
        m_Thread = nullptr;
    }
    LOGCATE("DecoderBase::UnInit end, m_MediaType=%d", m_MediaType);
}

int DecoderBase::InitFFDecoder() {
    int result = -1;
    do {
        //1.创建封装格式上下文
        m_AVFormatContext = avformat_alloc_context();

        //2.打开文件
        if(avformat_open_input(&m_AVFormatContext, m_Url, NULL, NULL) != 0)
        {
            LOGCATE("DecoderBase::InitFFDecoder avformat_open_input fail.");
            break;
        }

        //3.获取音视频流信息
        if(avformat_find_stream_info(m_AVFormatContext, NULL) < 0) {
            LOGCATE("DecoderBase::InitFFDecoder avformat_find_stream_info fail.");
            break;
        }

        //4.获取音视频流索引
        for(int i=0; i < m_AVFormatContext->nb_streams; i++) {
            if(m_AVFormatContext->streams[i]->codecpar->codec_type == m_MediaType) {
                m_StreamIndex = i;
                break;
            }
        }

        if (m_MediaType == AVMEDIA_TYPE_VIDEO && open_direct_push) {
            result = avformat_alloc_output_context2(&m_OutCtx, NULL, "flv", "rtmp://10.180.90.38:1935/live/bbb");
            LOGCATE("DecoderBase::InitFFDecoder avformat_alloc_output_context2 result=%d", result);

            for (int i = 0; i < m_AVFormatContext->nb_streams; i++) {
                AVStream *in_stream = m_AVFormatContext->streams[i];
                AVStream *out_stream = avformat_new_stream(m_OutCtx, in_stream->codec->codec);

                if (!out_stream) {
                    LOGCATE("DecoderBase::InitFFDecoder avformat_new_stream error!");
                } else {
                    result = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
                    if (result < 0) {
                        LOGCATE("DecoderBase::InitFFDecoder avcodec_parameters_copy error! %d", result);
                    } else {
                        out_stream->codecpar->codec_tag = 0;
                        out_stream->codec->codec_tag = 0;
                        if (m_OutCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                            out_stream->codec->flags = out_stream->codec->flags | AV_CODEC_FLAG_GLOBAL_HEADER;
                        }


                        /*AVCodecParameters *out_codecPar = out_stream->codecpar;
                        out_codecPar->codec_type = AVMEDIA_TYPE_VIDEO;
                        out_codecPar->codec_id = AV_CODEC_ID_H264;
                        out_codecPar->bit_rate = 2000000;
                        out_codecPar->width = 1920;
                        out_codecPar->height = 1080;
                        out_codecPar->codec_tag = 0;
                        out_codecPar->format = AV_PIX_FMT_YUV420P;

                        AVCodecContext *avCtx = out_stream->codec;
                        avCtx->codec_type = AVMEDIA_TYPE_VIDEO;
                        avCtx->codec_id = AV_CODEC_ID_H264;
                        avCtx->bit_rate = 2000000;
                        avCtx->width = 1920;
                        avCtx->height = 1080;
                        avCtx->time_base.num = 1;
                        avCtx->time_base.den = 1;*/

                        //out_stream->time_base.den = 1;
                        //out_stream->time_base.num = 1;

                        result = avio_open(&m_OutCtx->pb, "rtmp://10.180.90.38:1935/live/bbb", AVIO_FLAG_WRITE);
                        if (result < 0) {
                            LOGCATE("DecoderBase::InitFFDecoder avio_open error! %d", result);
                            avError(result);
                        } else {
                            result = avformat_write_header(m_OutCtx, 0);
                            if (result < 0) {
                                LOGCATE("DecoderBase::InitFFDecoder avformat_write_header error! %d", result);
                                avError(result);
                            }
                        }
                    }
                }
            }
        }

        if(m_StreamIndex == -1) {
            LOGCATE("DecoderBase::InitFFDecoder Fail to find stream index.");
            break;
        }
        //5.获取解码器参数
        AVCodecParameters *codecParameters = m_AVFormatContext->streams[m_StreamIndex]->codecpar;

        //6.获取解码器
        m_AVCodec = avcodec_find_decoder(codecParameters->codec_id);
        if(m_AVCodec == nullptr) {
            LOGCATE("DecoderBase::InitFFDecoder avcodec_find_decoder fail.");
            break;
        }

        //7.创建解码器上下文
        m_AVCodecContext = avcodec_alloc_context3(m_AVCodec);
        if(avcodec_parameters_to_context(m_AVCodecContext, codecParameters) != 0) {
            LOGCATE("DecoderBase::InitFFDecoder avcodec_parameters_to_context fail.");
            break;
        }

        AVDictionary *pAVDictionary = nullptr;
        av_dict_set(&pAVDictionary, "buffer_size", "1024000", 0);
        av_dict_set(&pAVDictionary, "stimeout", "20000000", 0);
        av_dict_set(&pAVDictionary, "max_delay", "30000000", 0);
        av_dict_set(&pAVDictionary, "rtsp_transport", "tcp", 0);

        //8.打开解码器
        result = avcodec_open2(m_AVCodecContext, m_AVCodec, &pAVDictionary);
        if(result < 0) {
            LOGCATE("DecoderBase::InitFFDecoder avcodec_open2 fail. result=%d", result);
            break;
        }
        result = 0;

        m_Duration = m_AVFormatContext->duration / AV_TIME_BASE * 1000;//us to ms
        //创建 AVPacket 存放编码数据
        m_Packet = av_packet_alloc();
        //创建 AVFrame 存放解码后的数据
        m_Frame = av_frame_alloc();
    } while (false);

    if(result != 0 && m_MsgContext && m_MsgCallback)
        m_MsgCallback(m_MsgContext, MSG_DECODER_INIT_ERROR, 0, nullptr);

    return result;
}

void DecoderBase::UnInitDecoder() {
    LOGCATE("DecoderBase::UnInitDecoder");
    if(m_Frame != nullptr) {
        av_frame_free(&m_Frame);
        m_Frame = nullptr;
    }

    if(m_Packet != nullptr) {
        av_packet_free(&m_Packet);
        m_Packet = nullptr;
    }

    if(m_AVCodecContext != nullptr) {
        avcodec_close(m_AVCodecContext);
        avcodec_free_context(&m_AVCodecContext);
        m_AVCodecContext = nullptr;
        m_AVCodec = nullptr;
    }

    if(m_AVFormatContext != nullptr) {
        avformat_close_input(&m_AVFormatContext);
        avformat_free_context(m_AVFormatContext);
        m_AVFormatContext = nullptr;
    }

}

void DecoderBase::StartDecodingThread() {
    m_Thread = new thread(DoAVDecoding, this);
}

void DecoderBase::DecodingLoop() {
    LOGCATE("DecoderBase::DecodingLoop start, m_MediaType=%d", m_MediaType);
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_DecoderState = STATE_DECODING;
        lock.unlock();
    }

    for(;;) {
        while (m_DecoderState == STATE_PAUSE) {
            std::unique_lock<std::mutex> lock(m_Mutex);
            LOGCATE("DecoderBase::DecodingLoop waiting, m_MediaType=%d", m_MediaType);
            m_Cond.wait_for(lock, std::chrono::milliseconds(10));
            m_StartTimeStamp = GetSysCurrentTime() - m_CurTimeStamp;
        }

        if(m_DecoderState == STATE_STOP) {
            break;
        }

        if(m_StartTimeStamp == -1)
            m_StartTimeStamp = GetSysCurrentTime();

        if (open_direct_push) {
            start_time = av_gettime();
            frame_index = 0;
        }

        if(DecodeOnePacket() != 0) {
            //解码结束，暂停解码器
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_DecoderState = STATE_PAUSE;
        }
    }
    LOGCATE("DecoderBase::DecodingLoop end");
}

void DecoderBase::UpdateTimeStamp() {
    LOGCATE("DecoderBase::UpdateTimeStamp");
    std::unique_lock<std::mutex> lock(m_Mutex);
    if(m_Frame->pkt_dts != AV_NOPTS_VALUE) {
        m_CurTimeStamp = m_Frame->pkt_dts;
    } else if (m_Frame->pts != AV_NOPTS_VALUE) {
        m_CurTimeStamp = m_Frame->pts;
    } else {
        m_CurTimeStamp = 0;
    }

    m_CurTimeStamp = (int64_t)((m_CurTimeStamp * av_q2d(m_AVFormatContext->streams[m_StreamIndex]->time_base)) * 1000);

    if(m_SeekPosition > 0 && m_SeekSuccess)
    {
        m_StartTimeStamp = GetSysCurrentTime() - m_CurTimeStamp;
        m_SeekPosition = 0;
        m_SeekSuccess = false;
    }
}

long DecoderBase::AVSync() {
    LOGCATE("DecoderBase::AVSync");
    long curSysTime = GetSysCurrentTime();
    //基于系统时钟计算从开始播放流逝的时间
    long elapsedTime = curSysTime - m_StartTimeStamp;

    if(m_MsgContext && m_MsgCallback && m_MediaType == AVMEDIA_TYPE_AUDIO)
        m_MsgCallback(m_MsgContext, MSG_DECODING_TIME, m_CurTimeStamp * 1.0f / 1000, nullptr);

    long delay = 0;

    //向系统时钟同步
    if(m_CurTimeStamp > elapsedTime) {
        //休眠时间
        auto sleepTime = static_cast<unsigned int>(m_CurTimeStamp - elapsedTime);//ms
        //限制休眠时间不能过长
        sleepTime = sleepTime > DELAY_THRESHOLD ? DELAY_THRESHOLD :  sleepTime;
        av_usleep(sleepTime * 1000);
    }
    delay = elapsedTime - m_CurTimeStamp;

    return delay;
}

int DecoderBase::DecodeOnePacket() {
    LOGCATE("DecoderBase::DecodeOnePacket m_MediaType=%d", m_MediaType);
    if(m_SeekPosition > 0) {
        //seek to frame
        int64_t seek_target = static_cast<int64_t>(m_SeekPosition * 1000000);//微秒
        int64_t seek_min = INT64_MIN;
        int64_t seek_max = INT64_MAX;
        int seek_ret = avformat_seek_file(m_AVFormatContext, -1, seek_min, seek_target, seek_max, 0);
        if (seek_ret < 0) {
            m_SeekSuccess = false;
            LOGCATE("DecoderBase::DecodeOneFrame error while seeking m_MediaType=%d", m_MediaType);
        } else {
            if (-1 != m_StreamIndex) {
                avcodec_flush_buffers(m_AVCodecContext);
            }
            ClearCache();
            m_SeekSuccess = true;
            LOGCATE("DecoderBase::DecodeOneFrame seekFrame pos=%f, m_MediaType=%d", m_SeekPosition, m_MediaType);
        }
    }
    int result = av_read_frame(m_AVFormatContext, m_Packet);

    //输入输出视频流
    AVStream *in_stream, *out_stream;

    while(result == 0) {

        if (open_direct_push && m_MediaType == AVMEDIA_TYPE_VIDEO) {
            if (m_Packet->pts == AV_NOPTS_VALUE) {
                AVRational time_base1 = m_AVFormatContext->streams[m_StreamIndex]->time_base;
                int64_t  calc_duration = (double) AV_TIME_BASE / av_q2d(m_AVFormatContext->streams[m_StreamIndex]->r_frame_rate);

                // 配置参数
                m_Packet->pts = (double )(frame_index * calc_duration) / (double )(av_q2d(time_base1) * AV_TIME_BASE);
                m_Packet->dts = m_Packet->pts;
                m_Packet->duration = (double )calc_duration / (double )(av_q2d(time_base1) * AV_TIME_BASE);
            }

            if(m_Packet->stream_index == m_StreamIndex) {
                if(avcodec_send_packet(m_AVCodecContext, m_Packet) == AVERROR_EOF) {
                    //解码结束
                    result = -1;
                    goto __EXIT;
                }

                AVRational time_base = m_AVFormatContext->streams[m_StreamIndex]->time_base;
                AVRational time_base_q = {1, AV_TIME_BASE};
                int64_t pts_time = av_rescale_q(m_Packet->dts, time_base, time_base_q);
                int64_t now_time = av_gettime() - start_time;

                AVRational avr = m_AVFormatContext->streams[m_StreamIndex]->time_base;

                LOGCATE("DecoderBase::DecodeOneFrame avr.num=%d  avr.den=%d %ld %ld %ld", avr.num, avr.den, m_Packet->dts, m_Packet->pts, pts_time);
                if (pts_time > now_time) {
                    av_usleep((unsigned int)(pts_time - now_time));
                }
            }

            in_stream = m_AVFormatContext->streams[m_Packet->stream_index];
            out_stream = m_OutCtx->streams[m_Packet->stream_index];

            m_Packet->pts = av_rescale_q_rnd(m_Packet->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            m_Packet->dts = av_rescale_q_rnd(m_Packet->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            m_Packet->duration = (int) av_rescale_q(m_Packet->duration, in_stream->time_base, out_stream->time_base);
            m_Packet->pos = -1;

            if(m_Packet->stream_index == m_StreamIndex) {
                frame_index++;
            }

            result = av_interleaved_write_frame(m_OutCtx, m_Packet);

            LOGCATE("DecoderBase::DecodeOneFrame av_interleaved_write_frame result=%d frame_index=%d", result,frame_index);

            if (result < 0) {
                LOGCATE("DecoderBase::DecodeOneFrame av_interleaved_write_frame error! %d", result);
                avError(result);
                break;
            }
        } else {
            if(m_Packet->stream_index == m_StreamIndex) {

                if(avcodec_send_packet(m_AVCodecContext, m_Packet) == AVERROR_EOF) {
                    //解码结束
                    result = -1;
                    goto __EXIT;
                }

                //一个 packet 包含多少 frame?
                int frameCount = 0;
                while (avcodec_receive_frame(m_AVCodecContext, m_Frame) == 0) {
                    //更新时间戳
                    UpdateTimeStamp();
                    //同步
                    AVSync();
                    //渲染
                    LOGCATE("DecoderBase::DecodeOnePacket 000 m_MediaType=%d", m_MediaType);
                    OnFrameAvailable(m_Frame);
                    LOGCATE("DecoderBase::DecodeOnePacket 0001 m_MediaType=%d", m_MediaType);
                    frameCount ++;
                }
                LOGCATE("BaseDecoder::DecodeOneFrame frameCount=%d", frameCount);
                //判断一个 packet 是否解码完成
                if(frameCount > 0) {
                    result = 0;
                    goto __EXIT;
                }
            }
        }

        av_packet_unref(m_Packet);
        result = av_read_frame(m_AVFormatContext, m_Packet);
    }

__EXIT:
    av_packet_unref(m_Packet);
    return result;
}

void DecoderBase::DoAVDecoding(DecoderBase *decoder) {
    LOGCATE("DecoderBase::DoAVDecoding");
    do {
        if(decoder->InitFFDecoder() != 0) {
            break;
        }
        decoder->OnDecoderReady();
        decoder->DecodingLoop();
    } while (false);

    decoder->UnInitDecoder();
    decoder->OnDecoderDone();

}

int DecoderBase::avError(int errNum) {
    char buf[1024];
    //获取错误信息
    av_strerror(errNum, buf, sizeof(buf));
    LOGCATE("DecoderBase::avError %d %s", errNum, buf);
    return -1;
}


