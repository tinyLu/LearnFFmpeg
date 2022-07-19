package com.rtmp.sender;

import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;

import com.pedro.encoder.Frame;
import com.pedro.encoder.input.video.GetCameraData;
import com.pedro.encoder.video.FormatVideoEncoder;
import com.pedro.encoder.video.GetVideoData;
import com.pedro.encoder.video.VideoEncoder;
import com.pedro.rtmp.rtmp.RtmpClient;
import com.pedro.rtmp.utils.ConnectCheckerRtmp;
import com.pedro.rtplibrary.util.FpsListener;

import java.nio.ByteBuffer;

public class VideoSenderManager implements GetVideoData , GetCameraData {
    private static final String TAG = "Camera1Base";

    private final RtmpClient rtmpClient;

    protected VideoEncoder videoEncoder;
    private final FpsListener fpsListener = new FpsListener();

    private boolean streaming = false;
    private int previewWidth, previewHeight;

    public VideoSenderManager(ConnectCheckerRtmp connectChecker) {
        rtmpClient = new RtmpClient(connectChecker);
        videoEncoder = new VideoEncoder(this);
    }

    @Override
    public void onSpsPpsVps(ByteBuffer sps, ByteBuffer pps, ByteBuffer vps) {
        rtmpClient.setVideoInfo(sps, pps, vps);
    }

    @Override
    public void getVideoData(ByteBuffer h264Buffer, MediaCodec.BufferInfo info) {
        fpsListener.calculateFps();
        if (streaming) getH264DataRtp(h264Buffer, info);
    }

    @Override
    public void onVideoFormat(MediaFormat mediaFormat) {
    }

    @Override
    public void inputYUVData(Frame frame) {
        videoEncoder.inputYUVData(frame);
    }

    protected void getH264DataRtp(ByteBuffer h264Buffer, MediaCodec.BufferInfo info) {
        rtmpClient.sendVideo(h264Buffer, info);
    }

    /**
     * Need be called after @prepareVideo or/and @prepareAudio. This method override resolution of
     *
     * @param url of the stream like: protocol://ip:port/application/streamName
     *
     * RTSP: rtsp://192.168.1.1:1935/live/pedroSG94 RTSPS: rtsps://192.168.1.1:1935/live/pedroSG94
     * RTMP: rtmp://192.168.1.1:1935/live/pedroSG94 RTMPS: rtmps://192.168.1.1:1935/live/pedroSG94
     * @startPreview to resolution seated in @prepareVideo. If you never startPreview this method
     * startPreview for you to resolution seated in @prepareVideo.
     */
    public void startStream(String url) {
        streaming = true;
        startEncoders();
        startStreamRtp(url);
    }

    private void startEncoders() {
        videoEncoder.start();
    }

    public void requestKeyFrame() {
        if (videoEncoder.isRunning()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                videoEncoder.requestKeyframe();
            }
        }
    }

    protected void startStreamRtp(String url) {
        if (videoEncoder.getRotation() == 90 || videoEncoder.getRotation() == 270) {
            rtmpClient.setVideoResolution(videoEncoder.getHeight(), videoEncoder.getWidth());
        } else {
            rtmpClient.setVideoResolution(videoEncoder.getWidth(), videoEncoder.getHeight());
        }
        rtmpClient.setFps(videoEncoder.getFps());
        rtmpClient.setOnlyVideo(true);
        rtmpClient.connect(url);
    }

    /**
     * Stop stream started with @startStream.
     */
    public void stopStream() {
        if (streaming) {
            streaming = false;
            stopStreamRtp();
        }
        videoEncoder.stop();
    }

    protected void stopStreamRtp() {
        rtmpClient.disconnect();
    }

    /**
     * Same to call: rotation = 0; if (Portrait) rotation = 90; prepareVideo(640, 480, 30, 1200 *
     * 1024, false, rotation);
     *
     * @return true if success, false if you get a error (Normally because the encoder selected
     * doesn't support any configuration seated or your device hasn't a H264 encoder).
     */
    public boolean prepareVideo() {
        return prepareVideo(640, 480, 30, 1200 * 1024, 90);
    }

    /**
     * backward compatibility reason
     */
    public boolean prepareVideo(int width, int height, int fps, int bitrate, int iFrameInterval,
                                int rotation) {
        return prepareVideo(width, height, fps, bitrate, iFrameInterval, rotation, -1, -1);
    }

    public boolean prepareVideo(int width, int height, int fps, int bitrate, int rotation) {
        return prepareVideo(width, height, fps, bitrate, 2, rotation);
    }

    public boolean prepareVideo(int width, int height, int bitrate) {
        return prepareVideo(width, height, 30, bitrate, 2, 0);
    }

    /**
     * Call this method before use @startStream. If not you will do a stream without video. NOTE:
     * Rotation with encoder is silence ignored in some devices.
     *
     * @param width resolution in px.
     * @param height resolution in px.
     * @param fps frames per second of the stream.
     * @param bitrate H264 in bps.
     * @param rotation could be 90, 180, 270 or 0. You should use CameraHelper.getCameraOrientation
     * with SurfaceView or TextureView and 0 with OpenGlView or LightOpenGlView. NOTE: Rotation with
     * encoder is silence ignored in some devices.
     * @return true if success, false if you get a error (Normally because the encoder selected
     * doesn't support any configuration seated or your device hasn't a H264 encoder).
     */
    public boolean prepareVideo(int width, int height, int fps, int bitrate, int iFrameInterval,
                                int rotation, int avcProfile, int avcProfileLevel) {
        if (width != previewWidth || height != previewHeight
                || fps != videoEncoder.getFps() || rotation != videoEncoder.getRotation()) {
            stopPreview();
        }
        FormatVideoEncoder formatVideoEncoder = FormatVideoEncoder.YUV420Dynamical;
        return videoEncoder.prepareVideoEncoder(width, height, fps, bitrate, rotation, iFrameInterval,
                formatVideoEncoder, avcProfile, avcProfileLevel);
    }

    /**
     * Stop camera preview. Ignored if streaming or already stopped. You need call it after
     *
     * @stopStream to release camera properly if you will close activity.
     */
    public void stopPreview() {
        if (!isStreaming()) {
            previewWidth = 0;
            previewHeight = 0;
        } else {
            Log.e(TAG, "Streaming or preview stopped, ignored");
        }
    }
    /**
     * Get stream state.
     *
     * @return true if streaming, false if not streaming.
     */
    public boolean isStreaming() {
        return streaming;
    }
}
