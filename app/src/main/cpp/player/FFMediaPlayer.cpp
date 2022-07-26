/**
 *
 * Created by 公众号：字节流动 on 2021/3/16.
 * https://github.com/githubhaohao/LearnFFmpeg
 * 最新文章首发于公众号：字节流动，有疑问或者技术交流可以添加微信 Byte-Flow ,领取视频教程, 拉你进技术交流群
 *
 * */


#include <render/video/NativeRender.h>
#include <render/audio/OpenSLRender.h>
#include <render/video/VideoGLRender.h>
#include <render/video/VRGLRender.h>
#include "FFMediaPlayer.h"
#include <android/bitmap.h>

void FFMediaPlayer::Init(JNIEnv *jniEnv, jobject obj, char *url, int videoRenderType, jobject surface) {
    jniEnv->GetJavaVM(&m_JavaVM);
    m_JavaObj = jniEnv->NewGlobalRef(obj);

    m_VideoDecoder = new VideoDecoder(url);


    if(videoRenderType == VIDEO_RENDER_OPENGL) {
        m_VideoDecoder->SetVideoRender(VideoGLRender::GetInstance());
    } else if (videoRenderType == VIDEO_RENDER_ANWINDOW) {
        m_VideoRender = new NativeRender(jniEnv, surface);
        m_VideoDecoder->SetVideoRender(m_VideoRender);
    } else if (videoRenderType == VIDEO_RENDER_3D_VR) {
        m_VideoDecoder->SetVideoRender(VRGLRender::GetInstance());
    }

    m_AudioDecoder = new AudioDecoder(url);
    m_AudioRender = new OpenSLRender();

    //m_AudioDecoder->SetAudioRender(m_AudioRender);

    m_VideoDecoder->SetMessageCallback(this, PostMessage);
    m_AudioDecoder->SetMessageCallback(this, PostMessage);
}

void FFMediaPlayer::UnInit() {
    LOGCATE("FFMediaPlayer::UnInit");
    if(m_VideoDecoder) {
        delete m_VideoDecoder;
        m_VideoDecoder = nullptr;
    }

    if(m_VideoRender) {
        delete m_VideoRender;
        m_VideoRender = nullptr;
    }

    if(m_AudioDecoder) {
        delete m_AudioDecoder;
        m_AudioDecoder = nullptr;
    }

    if(m_AudioRender) {
        delete m_AudioRender;
        m_AudioRender = nullptr;
    }

    VideoGLRender::ReleaseInstance();

    bool isAttach = false;

    JNIEnv *env = GetJNIEnv(&isAttach);

    if (env != nullptr) {
        env->DeleteGlobalRef(m_JavaObj);
    }

    if(isAttach)
        GetJavaVM()->DetachCurrentThread();

}

void FFMediaPlayer::Play() {
    LOGCATE("FFMediaPlayer::Play");
    if(m_VideoDecoder)
        m_VideoDecoder->Start();

    if(m_AudioDecoder)
        m_AudioDecoder->Start();
}

void FFMediaPlayer::Pause() {
    LOGCATE("FFMediaPlayer::Pause");
    if(m_VideoDecoder)
        m_VideoDecoder->Pause();

    if(m_AudioDecoder)
        m_AudioDecoder->Pause();

}

void FFMediaPlayer::Stop() {
    LOGCATE("FFMediaPlayer::Stop");
    if(m_VideoDecoder)
        m_VideoDecoder->Stop();

    if(m_AudioDecoder)
        m_AudioDecoder->Stop();
}

void FFMediaPlayer::SeekToPosition(float position) {
    LOGCATE("FFMediaPlayer::SeekToPosition position=%f", position);
    if(m_VideoDecoder)
        m_VideoDecoder->SeekToPosition(position);

    if(m_AudioDecoder)
        m_AudioDecoder->SeekToPosition(position);

}

long FFMediaPlayer::GetMediaParams(int paramType) {
    LOGCATE("FFMediaPlayer::GetMediaParams paramType=%d", paramType);
    long value = 0;
    switch(paramType)
    {
        case MEDIA_PARAM_VIDEO_WIDTH:
            value = m_VideoDecoder != nullptr ? m_VideoDecoder->GetVideoWidth() : 0;
            break;
        case MEDIA_PARAM_VIDEO_HEIGHT:
            value = m_VideoDecoder != nullptr ? m_VideoDecoder->GetVideoHeight() : 0;
            break;
        case MEDIA_PARAM_VIDEO_DURATION:
            value = m_VideoDecoder != nullptr ? m_VideoDecoder->GetDuration() : 0;
            break;
    }
    return value;
}

JNIEnv *FFMediaPlayer::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == m_JavaVM) {
        LOGCATE("FFMediaPlayer::GetJNIEnv m_JavaVM == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = m_JavaVM->GetEnv((void **)&env, JNI_VERSION_1_4);
    if (status != JNI_OK) {
        status = m_JavaVM->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGCATE("FFMediaPlayer::GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

jobject FFMediaPlayer::CreateBitmap(JNIEnv *env, int width, int height) {
    // 找到 Bitmap.class 和 该类中的 createBitmap 方法
    jclass clz_bitmap = env->FindClass("android/graphics/Bitmap");
    jmethodID mtd_bitmap = env->GetStaticMethodID(
            clz_bitmap, "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");

    // 配置 Bitmap
    jstring str_config = env->NewStringUTF("ARGB_8888");
    jclass clz_config = env->FindClass("android/graphics/Bitmap$Config");
    jmethodID mtd_config = env->GetStaticMethodID(
            clz_config, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
    jobject obj_config = env->CallStaticObjectMethod(clz_config, mtd_config, str_config);

    // 创建 Bitmap 对象
    jobject bitmap = env->CallStaticObjectMethod(
            clz_bitmap, mtd_bitmap, width, height, obj_config);

    LOGCATE("FFMediaPlayer::CreateBitmap %d, %d", width, height);
    return bitmap;
}

jobject FFMediaPlayer::GetJavaObj() {
    return m_JavaObj;
}

JavaVM *FFMediaPlayer::GetJavaVM() {
    return m_JavaVM;
}

void FFMediaPlayer::PostMessage(void *context, int msgType, float msgCode, jbyteArray bitmap) {
    if(context != nullptr)
    {
        FFMediaPlayer *player = static_cast<FFMediaPlayer *>(context);
        bool isAttach = false;
        JNIEnv *env = player->GetJNIEnv(&isAttach);
        LOGCATE("FFMediaPlayer::PostMessage env=%p", env);
        if(env == nullptr)
            return;

        if (msgType == MSG_DECODER_READY) {
            player->bitMap = player->CreateBitmap(env, player->m_VideoDecoder->GetVideoWidth(), player->m_VideoDecoder->GetVideoHeight());

            //player->m_VideoDecoder->bitMap = player->bitMap;

            LOGCATE("FFMediaPlayer::PostMessage aaa");
            AndroidBitmapInfo dstBitmapInfo;
            AndroidBitmap_getInfo(env, player->bitMap, &dstBitmapInfo);
            LOGCATE("FFMediaPlayer::PostMessage dstWidth=%d, dstHeight=%d", dstBitmapInfo.width, dstBitmapInfo.height);
        } else if (msgType == MSG_DECODER_BITMAP) {
            LOGCATE("FFMediaPlayer::PostMessage bbb bitmap");
        }

        jobject javaObj = player->GetJavaObj();
        //jmethodID mid = env->GetMethodID(env->GetObjectClass(javaObj), JAVA_PLAYER_EVENT_CALLBACK_API_NAME, "(IFLandroid/graphics/Bitmap;)V");
        jmethodID mid = env->GetMethodID(env->GetObjectClass(javaObj), JAVA_PLAYER_EVENT_CALLBACK_API_NAME, "(IF[B)V");
        env->CallVoidMethod(javaObj, mid, msgType, msgCode, bitmap);
        if(isAttach)
            player->GetJavaVM()->DetachCurrentThread();

    }
}


