/**
 *
 * Created by 公众号：字节流动 on 2021/3/16.
 * https://github.com/githubhaohao/LearnFFmpeg
 * 最新文章首发于公众号：字节流动，有疑问或者技术交流可以添加微信 Byte-Flow ,领取视频教程, 拉你进技术交流群
 *
 * */

package com.byteflow.learnffmpeg;

import android.Manifest;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import com.byteflow.learnffmpeg.media.FFMediaPlayer;
import com.byteflow.learnffmpeg.media.MyGLSurfaceView;
import com.byteflow.learnffmpeg.util.CommonUtils;
import com.pedro.encoder.Frame;
import com.pedro.rtmp.utils.ConnectCheckerRtmp;
import com.rtmp.sender.ImageUtils;
import com.rtmp.sender.VideoSenderManager;

import java.util.logging.LogManager;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MEDIA_PARAM_VIDEO_DURATION;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MEDIA_PARAM_VIDEO_HEIGHT;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MEDIA_PARAM_VIDEO_WIDTH;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MSG_DECODER_DONE;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MSG_DECODER_INIT_ERROR;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MSG_DECODER_READY;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MSG_DECODING_BITMAP;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MSG_DECODING_TIME;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.MSG_REQUEST_RENDER;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.VIDEO_GL_RENDER;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.VIDEO_RENDER_ANWINDOW;
import static com.byteflow.learnffmpeg.media.FFMediaPlayer.VIDEO_RENDER_OPENGL;

public class GLMediaPlayerActivity extends AppCompatActivity implements GLSurfaceView.Renderer, FFMediaPlayer.EventCallback, MyGLSurfaceView.OnGestureCallback{
    private static final String TAG = "MediaPlayerActivity";
    private static final String[] REQUEST_PERMISSIONS = {
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
    };
    private static final int PERMISSION_REQUEST_CODE = 1;
    private MyGLSurfaceView mGLSurfaceView = null;
    private FFMediaPlayer mMediaPlayer = null;
    private SeekBar mSeekBar = null;
    private boolean mIsTouch = false;
    private ImageView mImageV;
    private String mVideoPath = Environment.getExternalStorageDirectory().getAbsolutePath() + "/byteflow/one_piece.mp4";

    private boolean mOpenDirectPush = true;
    private boolean mVideoSenderPrepare = false;
    private VideoSenderManager videoSenderManager = new VideoSenderManager(new ConnectCheckerRtmp() {
        @Override
        public void onConnectionStartedRtmp(@NonNull String rtmpUrl) {

        }

        @Override
        public void onConnectionSuccessRtmp() {

        }

        @Override
        public void onConnectionFailedRtmp(@NonNull String reason) {

        }

        @Override
        public void onNewBitrateRtmp(long bitrate) {

        }

        @Override
        public void onDisconnectRtmp() {

        }

        @Override
        public void onAuthErrorRtmp() {

        }

        @Override
        public void onAuthSuccessRtmp() {

        }
    });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_gl_media_player);
        mImageV = findViewById(R.id.imageView);
        mGLSurfaceView = findViewById(R.id.surface_view);
        mGLSurfaceView.setEGLContextClientVersion(3);
        mGLSurfaceView.setRenderer(this);
        mGLSurfaceView.addOnGestureCallback(this);
        mGLSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

        mSeekBar = findViewById(R.id.seek_bar);
        mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {

            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                mIsTouch = true;
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                Log.d(TAG, "onStopTrackingTouch() called with: progress = [" + seekBar.getProgress() + "]");
                if(mMediaPlayer != null) {
                    mMediaPlayer.seekToPosition(mSeekBar.getProgress());
                    mIsTouch = false;
                }

            }
        });
        mMediaPlayer = new FFMediaPlayer();
        mMediaPlayer.addEventCallback(this);

        //mMediaPlayer.init("rtsp://admin:a12345678@y52t229909.zicp.vip:554/Streaming/Channels/101", VIDEO_RENDER_OPENGL, null);
        mMediaPlayer.init(/*mVideoPath*/"rtsp://192.168.101.222:8554", VIDEO_RENDER_OPENGL, null);
        //mMediaPlayer.init(/*mVideoPath*/"rtmp://10.180.90.38:1935/live/aaa", VIDEO_RENDER_OPENGL, null);
        //mMediaPlayer.init(/*mVideoPath*/"rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4", VIDEO_RENDER_OPENGL, null);
    }

    @Override
    protected void onResume() {
        Log.e(TAG, "onResume() called");
        super.onResume();
        if (!hasPermissionsGranted(REQUEST_PERMISSIONS)) {
            ActivityCompat.requestPermissions(this, REQUEST_PERMISSIONS, PERMISSION_REQUEST_CODE);
        } else {
            if(mMediaPlayer != null)
                mMediaPlayer.play();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (!hasPermissionsGranted(REQUEST_PERMISSIONS)) {
                Toast.makeText(this, "We need the permission: WRITE_EXTERNAL_STORAGE", Toast.LENGTH_SHORT).show();
            } else {
                //if(mMediaPlayer != null)
                    //mMediaPlayer.play();
            }
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if(mMediaPlayer != null)
            mMediaPlayer.pause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if(mMediaPlayer != null)
            mMediaPlayer.unInit();

        if (!mOpenDirectPush) {
            videoSenderManager.stopStream();
        }
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
        FFMediaPlayer.native_OnSurfaceCreated(VIDEO_GL_RENDER);
    }

    @Override
    public void onSurfaceChanged(GL10 gl10, int w, int h) {
        Log.d(TAG, "onSurfaceChanged() called with: gl10 = [" + gl10 + "], w = [" + w + "], h = [" + h + "]");
        FFMediaPlayer.native_OnSurfaceChanged(VIDEO_GL_RENDER, w, h);
    }

    @Override
    public void onDrawFrame(GL10 gl10) {
        FFMediaPlayer.native_OnDrawFrame(VIDEO_GL_RENDER);
    }

    @Override
    public void onPlayerEvent(final int msgType, final float msgValue, final byte[] bitmap) {
        Log.d(TAG, "onPlayerEvent() called with: msgType = [" + msgType + "], msgValue = [" + msgValue + "]  mVideoSenderPrepare:" + mVideoSenderPrepare);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                switch (msgType) {
                    case MSG_DECODER_INIT_ERROR:
                        break;
                    case MSG_DECODER_READY:
                        onDecoderReady();
                        break;
                    case MSG_DECODER_DONE:
                        break;
                    case MSG_REQUEST_RENDER:
                        mGLSurfaceView.requestRender();
                        break;
                    case MSG_DECODING_TIME:
                        if(!mIsTouch)
                            mSeekBar.setProgress((int) msgValue);
                        break;
                    case MSG_DECODING_BITMAP:
                        if (null != bitmap) {

                            if (!mOpenDirectPush) {
                                if (mVideoSenderPrepare) {
                                    //videoSenderManager.inputYUVData(new Frame(ImageUtils.bitmapToNv21(bitmap, bitmap.getWidth(), bitmap.getHeight()), 0, false, ImageFormat.NV21));
                                    videoSenderManager.inputYUVData(new Frame(bitmap, 0, false, ImageFormat.NV21));
                                } else {
                                    callVideoSenderPrepare(null);
                                }
                            }

                            //bitmap.recycle();

                            /*runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mImageV.setImageBitmap(bitmap);
                                }
                            });*/
                        }
                        break;
                    default:
                        break;
                }
            }
        });
    }

    private synchronized void callVideoSenderPrepare(final Bitmap bitmap) {
        if (!mVideoSenderPrepare /*&& null != bitmap*/) {
            if (videoSenderManager.prepareVideo(1920, 1080, 25, 1024 * 1024, 0)) {
                videoSenderManager.startStream("rtmp://10.180.90.38:1935/live/bbb");
                mVideoSenderPrepare = true;
            }
        }
    }

    private void onDecoderReady() {
        int videoWidth = (int) mMediaPlayer.getMediaParams(MEDIA_PARAM_VIDEO_WIDTH);
        int videoHeight = (int) mMediaPlayer.getMediaParams(MEDIA_PARAM_VIDEO_HEIGHT);
        if(videoHeight * videoWidth != 0)
            mGLSurfaceView.setAspectRatio(videoWidth, videoHeight);

        int duration = (int) mMediaPlayer.getMediaParams(MEDIA_PARAM_VIDEO_DURATION);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mSeekBar.setMin(0);
        }
        mSeekBar.setMax(duration);
    }

    protected boolean hasPermissionsGranted(String[] permissions) {
        for (String permission : permissions) {
            if (ActivityCompat.checkSelfPermission(this, permission)
                    != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void onGesture(int xRotateAngle, int yRotateAngle, float scale) {
         FFMediaPlayer.native_SetGesture(VIDEO_GL_RENDER, xRotateAngle, yRotateAngle, scale);
    }

    @Override
    public void onTouchLoc(float touchX, float touchY) {
        FFMediaPlayer.native_SetTouchLoc(VIDEO_GL_RENDER, touchX, touchY);
    }
}
