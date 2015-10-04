/*
 * Copyright (c) 2013 Chun-Ying Huang
 *
 * This file is part of GamingAnywhere (GA).
 *
 * GA is free software; you can redistribute it and/or modify it
 * under the terms of the 3-clause BSD License as published by the
 * Free Software Foundation: http://directory.fsf.org/wiki/License:BSD_3Clause
 *
 * GA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the 3-clause BSD License along with GA;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package org.gaminganywhere.gaclient;

import java.nio.ByteBuffer;
import java.util.HashMap;
import android.app.Activity;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;

public class GAClient {

	private Context context = null;
	private Activity activity = null;
	private Surface surface = null;
	private SurfaceView surfaceView = null;
	private Handler handler = null;
	private HashMap<String, String> gaconfig = null;
	//
	private int audioSamplerate = 0;
	private int audioChannels = 0;
	//
	private int screenWidth = 0;
	private int screenHeight = 0;
		
	public GAClient() {
		initGAClient(this);
	}
	
	public void setContext(Context c) {
		this.context = c;
	}
	
	public void setActivity(Activity a) {
		this.activity = a;
	}
	
	public void setSurface(Surface s) {
		this.surface = s;
	}
	
	public void setSurfaceView(SurfaceView sv) {
		this.surfaceView = sv;
	}
	
	public void setHandler(Handler h) {
		this.handler = h;
	}
	
	public void setScreenDimension(int width, int height) {
		screenWidth = width;
		screenHeight = height;
		Log.d("ga_log", "screen-dimension: "
				+ Integer.toString(width) + "x"
				+ Integer.toString(height));
	}
	
	public int getScreenWidth() {
		return this.screenWidth;
	}
	
	public int getScreenHeight() {
		return this.screenHeight;
	}
	
	public boolean profileLoad(String key) {
		if(context == null) {
			Log.e("androidclient", "No context provded");
			return false;
		}
		if((gaconfig = GAConfigHelper.profileLoad(context, key)) == null) {
			return false;
		}
		//
		resetConfig();
		setProtocol(gaconfig.get("protocol"));
		setHost(gaconfig.get("host"));
		setPort(Integer.parseInt(gaconfig.get("port")));
		setObjectPath(gaconfig.get("object"));
		setRTPOverTCP(Integer.parseInt(gaconfig.get("rtpovertcp")) != 0);
		setCtrlEnable(Integer.parseInt(gaconfig.get("ctrlenable")) != 0);
		setCtrlProtocol(gaconfig.get("ctrlprotocol").equals("tcp") ? true : false);
		setCtrlPort(Integer.parseInt(gaconfig.get("ctrlport")));
		setAudioCodec(
				Integer.parseInt(gaconfig.get("audio_samplerate")),
				Integer.parseInt(gaconfig.get("audio_channels")));
		//
		audioSamplerate = Integer.parseInt(gaconfig.get("audio_samplerate"));
		audioChannels = Integer.parseInt(gaconfig.get("audio_channels"));
		//
		return true;
	}

	// watchdog
	private boolean quitWatchdog = true;
	private int watchdogTick = 0;
	private Thread watchdogThread = null;
	private int watchdogTimeout = 3;	// default

	public boolean watchdogSetTimeout(int to) {
		watchdogTimeout = to;
		return true;
	}
	
	private void watchdogThreadProc() {
		int lastTick = watchdogTick;
		int idle = 0;
		Log.d("ga_log", "watchdog: started.");
		while(!Thread.interrupted() && !quitWatchdog) {
			try {
				Thread.sleep(1000);
				if(watchdogTimeout < 0) {
					// disabled
					lastTick = watchdogTick;
					continue;
				}
				if(watchdogTick != lastTick) {
					idle = 0;
					lastTick = watchdogTick;
				} else {
					idle++;
					if((idle % 5) == 1)
						showToast(String.format("Idle detected, wait for %d seconds ...", watchdogTimeout-idle+1));
				}
				if(idle >= watchdogTimeout) {
					Log.d("ga_log", "watchdog: idle detected, goback.");
					goBack(-1);
					break;
				}
			} catch(Exception e) {}
		}
		Log.d("ga_log", "watchdog: terminated.");
	}
	
	public boolean startWatchdog() {
		//
		quitWatchdog = false;
		watchdogThread = new Thread(new Runnable() {
				public void run() {
					watchdogThreadProc();
				}
			});
		watchdogThread.start();
		//
		return true;
	}
	
	public void stopWatchdog() {
		quitWatchdog = true;
		if(watchdogThread != null) {
			watchdogThread.interrupt();
			try {
				watchdogThread.join();
			} catch(Exception e) {
				e.printStackTrace();
			}
		}
		watchdogThread = null;
		return;
	}
	
	public void kickWatchdog() {
		watchdogTick++;
	}
	
	// media stuff
	private MediaFormat audioFormat = null;
	private MediaFormat videoFormat = null;
	private MediaCodec adecoder = null;
	private MediaCodec vdecoder = null;
	private ByteBuffer[] inputVBuffers = null;
	private ByteBuffer[] inputABuffers = null;
	private int audioTrackMinBufSize = -1;
	private AudioTrack audioTrack = null;

	private boolean videoRendered = false;
	private boolean quitAudioRenderer = true;
	private boolean quitVideoRenderer = true;
	private Thread audioRendererThread = null;
	private Thread videoRendererThread = null;
	private Thread rtspThread = null;
	
	private boolean builtinAudioDecoder = false;
	private boolean builtinVideoDecoder = false;
	
	private void audioRenderSoftware() {
		// software-based decoder
		byte[] stream = null;
		if(audioTrack == null || audioTrackMinBufSize <= 0) {
			Log.d("ga_log", "audio renderer: audio track not initialized.");
			return;
		}
		stream = new byte[audioTrackMinBufSize];
		while(!Thread.interrupted() && !quitAudioRenderer) {
			int filled;
			if((filled = audioBufferFill(stream, 2048)) > 0) {
				audioTrack.write(stream, 0, filled);
			}
		}
		stream = null;
		return;
	}
	
	private void audioRenderBuiltin() {
		MediaCodec.BufferInfo bufinfo = new MediaCodec.BufferInfo();
		ByteBuffer[] outputBuffers = null;
		int outbufIdx;
		outputBuffers = adecoder.getOutputBuffers();
		Log.d("ga_log", "audioRenderer started.");
		while(!Thread.interrupted() && !quitAudioRenderer) {
			outbufIdx = adecoder.dequeueOutputBuffer(bufinfo, 10000);
			switch(outbufIdx) {
			case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
				Log.d("ga_log", "audioRenderer: output buffers changed.");
				outputBuffers = adecoder.getOutputBuffers();
				break;
			case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
				audioTrack.setPlaybackRate(adecoder.getOutputFormat().getInteger(MediaFormat.KEY_SAMPLE_RATE));
				Log.d("ga_log", "audioRenderer: format changed - " + adecoder.getOutputFormat());
				break;
			case MediaCodec.INFO_TRY_AGAIN_LATER:
				//Log.d("ga_log", "decodeAudio: try again later.");
				break;
			default:
				// decoded or rendered
				if(bufinfo.size > 0) {
					byte[] ad = new byte[bufinfo.size];
					outputBuffers[outbufIdx].get(ad);
					outputBuffers[outbufIdx].clear();
					if(ad.length > 0 && videoRendered) {
						audioTrack.write(ad, 0, ad.length);
					}
				}
				adecoder.releaseOutputBuffer(outbufIdx, false);
				break;
			}
		}
		//
		//adecoder.queueInputBuffer(0, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
		outbufIdx = adecoder.dequeueOutputBuffer(bufinfo, 10000);
		if(outbufIdx >= 0) {
			adecoder.releaseOutputBuffer(outbufIdx, false);
		}
		bufinfo = null;
		outputBuffers = null;
		return;
	}

	private void audioRenderThreadProc() {		
		if(builtinAudioDecoder == false) {
			audioRenderSoftware();
		} else {
			audioRenderBuiltin();
		}
		Log.d("ga_log", "audioRenderer terminated.");
		return;
	}

	private void videoRendererThreadProc() {
		MediaCodec.BufferInfo bufinfo = new MediaCodec.BufferInfo();
		//ByteBuffer[] outputBuffers = null;	// unused?a
		int outbufIdx;
		//outputBuffers = vdecoder.getOutputBuffers();
		videoRendered = false;
		Log.d("ga_log", "videoRenderer started.");
		while(!Thread.interrupted() && !quitVideoRenderer) {
			outbufIdx = vdecoder.dequeueOutputBuffer(bufinfo, 500000);
			switch (outbufIdx) {
			case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
				Log.d("ga_log", "decodeVideo: output buffers changed.");
				// outputBuffers = vdecoder.getOutputBuffers();
				break;
			case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
				Log.d("ga_log", "decodeVideo: format changed - " + vdecoder.getOutputFormat());
				break;
			case MediaCodec.INFO_TRY_AGAIN_LATER:
				// Log.d("ga_log", "decodeVideo: try again later.");
				break;
			default:
				// decoded or rendered
				videoRendered = true;
				vdecoder.releaseOutputBuffer(outbufIdx, true);
				break;
			}
		}
		// flush decoder
		//vdecoder.queueInputBuffer(0, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
		outbufIdx = vdecoder.dequeueOutputBuffer(bufinfo, 10000);
		if(outbufIdx >= 0) {
			vdecoder.releaseOutputBuffer(outbufIdx, true);
		}
		bufinfo = null;
		videoRendered = false;
		//
		Log.d("ga_log", "videoRenderer terminated.");
	}
	
	private Thread stopRenderer(Thread t) {
		if(t != null) {
			if(t.isAlive()) {
				t.interrupt();
				try {
					t.join();
				} catch(Exception e) {
					e.printStackTrace();
				}
			}
			t = null;
		}
		return null;
	}
	
	private boolean startAudioRenderer() {
		if(builtinAudioDecoder && adecoder == null)
			return false;
		//
		stopAudioRenderer();
		//
		quitAudioRenderer = false;
		audioRendererThread = new Thread(new Runnable() {
				public void run() {
					audioRenderThreadProc();
				}
			});
		audioRendererThread.start();
		//
		return true;
	}

	private void stopAudioRenderer() {
		quitAudioRenderer = true;
		audioRendererThread = stopRenderer(audioRendererThread);
	}
	
	private boolean startVideoRenderer() {
		if(vdecoder == null)
			return false;
		//
		stopVideoRenderer();
		//
		quitVideoRenderer = false;
		videoRendererThread = new Thread(new Runnable() {
				public void run() {
					videoRendererThreadProc();
				}
			});
		videoRendererThread.start();
		//
		return true;
	}
	
	private void stopVideoRenderer() {
		quitVideoRenderer = true;
		videoRendererThread = stopRenderer(videoRendererThread);
	}

	public void startRTSPClient() {
		rtspThread = new Thread(new Runnable() {
			public void run() {
				Log.d("ga_log", "rtspClient: started.");
				rtspConnect();
				goBack(-1);
			}
		});
		rtspThread.start();
	}

	public void stopRTSPClient() {
		rtspDisconnect();
		try {
			rtspThread.join();
			Log.d("ga_log", "rtspClient: stopped.");
		} catch(Exception e) {
			e.printStackTrace();
		}
		stopAudioDecoder();
		stopAudio();
		stopVideoDecoder();
		//
		return;
	}
	
	public void showToast(String s) {
		if(handler != null) {
			Message m = new Message();
			m.what = PlayerActivity.MSG_SHOWTOAST;
			m.obj = s;
			handler.sendMessage(m);
		}
	}
	
	public void goBack(int exitCode) {
		if(handler != null) {
			Message m = new Message();
			m.what = PlayerActivity.MSG_QUIT;
			m.arg1 = -1;
			handler.sendMessage(m);
		}
	}
	
	public void requestRender() {
		if(surfaceView != null) {
			((GLSurfaceView) surfaceView).requestRender();
			return;
		}
	}
	
	public Object initAudio(String mime, int sampleRate, int channelCount, boolean builtinDecoder) {
		int channelConfig;
		sampleRate = audioSamplerate;
		channelCount = audioChannels;
		if(channelCount == 1) {
			channelConfig = AudioFormat.CHANNEL_OUT_MONO;
		} else if(channelCount == 2) {
			channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
		} else {
			Log.e("g_log", "initAudio: unsupported channel count ("
					+ Integer.toString(channelCount) + ")");
			return null;
		}
		//
		audioTrackMinBufSize = 
				AudioTrack.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
		audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate, channelConfig,
				AudioFormat.ENCODING_PCM_16BIT, audioTrackMinBufSize,
				AudioTrack.MODE_STREAM);
		if(audioTrack == null) {
			Log.e("ga_log", "create AudioTrack failed.");
			return null;
		} else {
			Log.d("ga_log", "AudioTrack created: minbufsize=" + Integer.toString(audioTrackMinBufSize));
		}
		//
		if((this.builtinAudioDecoder = builtinDecoder) == false) {
			audioTrack.play();
			startAudioRenderer();
			return audioTrack;
		}
		//
		audioFormat = MediaFormat.createAudioFormat(mime, sampleRate, channelCount);
		if(audioFormat == null) {
			Log.e("ga_log", "create audio mediaformat failed.");
		} else {
			audioTrack.play();
			Log.d("ga_log", "create audio mediaformat success"
					+ " [" + mime + "@" + Integer.toString(sampleRate) + "hz," + Integer.toString(channelCount) + "ch]");
		}
		return audioFormat;
	}
	
	public MediaCodec startAudioDecoder() {
		if(audioFormat == null)
			return null;
		//
		try {
		adecoder = MediaCodec.createDecoderByType(audioFormat.getString(MediaFormat.KEY_MIME));
		} catch(Exception e) {
		return null;
		}
		if(adecoder == null)
			return null;
		adecoder.configure(audioFormat, null, null, 0);
		adecoder.start();
		inputABuffers = adecoder.getInputBuffers();
		//
		startAudioRenderer();
		//
		Log.d("ga_log", "audio decoder started.");
		return adecoder;
	}
	
	private int currAInbufIdx = -1;
	private long currAPts = -1;
	public int decodeAudio(byte[] data, int size, long presentationTimeUs, int flag) {
		int inbufIdx, ret = 0;
		watchdogTick++;
		if(adecoder == null)
			return -1;
		try {
			do {
				int pos, remaining;
				if(currAInbufIdx == -1) {
					inbufIdx = adecoder.dequeueInputBuffer(10000);
					if(inbufIdx < 0) {
						ret = -1;
						break;
					}
					currAInbufIdx = inbufIdx;
					currAPts = presentationTimeUs;
					inputABuffers[currAInbufIdx].clear();
				}
				pos = inputABuffers[currAInbufIdx].position();
				remaining = inputABuffers[currAInbufIdx].remaining();
				//if(remaining >= size) {
				if(remaining >= size && pos < audioTrackMinBufSize/*2048*/) {
					inputABuffers[currAInbufIdx].put(data, 0, size);
				} else {
//					Log.d("ga_log", "decodeAudio: submit,"
//							+ " pts=" + Long.toString(currAPts)
//							+ " position="+inputABuffers[currAInbufIdx].position()
//							+ " capacity="+inputABuffers[currAInbufIdx].capacity()
//							);
					adecoder.queueInputBuffer(currAInbufIdx, 0, inputABuffers[currAInbufIdx].position(), currAPts, 0);
					//
					inbufIdx = adecoder.dequeueInputBuffer(10000);
					if(inbufIdx >= 0) {
						currAInbufIdx = inbufIdx;
						currAPts = presentationTimeUs;
						inputABuffers[inbufIdx].clear();
						inputABuffers[inbufIdx].put(data, 0, size);
					} else {
						currAInbufIdx = -1;
						currAPts = -1;
						ret = -1;
					}
				}
			} while(false);
			//
		} catch(Exception e) {
			Log.d("ga_log", e.toString());
			e.printStackTrace();
			return -1;
		}
		return ret;
	}
	
	public void stopAudioDecoder() {
		if(adecoder == null)
			return;
		// flush decoder
		stopAudioRenderer();
		//
		adecoder.stop();
		adecoder.release();
		adecoder = null;
		Log.d("ga_log", "audio decoder stopped.");
	}
	
	public void stopAudio() {
		if(audioTrack != null) {
			audioTrack.stop();
			audioTrack.release();
			audioTrack = null;
		}
		if(builtinAudioDecoder == false)
			stopAudioRenderer();
		Log.d("ga_log", "audio stopped.");
	}
		
	public MediaFormat initVideo(String mime, int width, int height) {
		videoFormat = MediaFormat.createVideoFormat(mime, width, height);
		if(videoFormat == null)
			Log.d("ga_log", "create video mediaformat failed.");
		else
			Log.d("ga_log", "create video mediaformat success"
					+ " [" + mime + "@" + Integer.toString(width) + "x" + Integer.toString(height) + "]");
		setScreenDimension(width, height);
		return videoFormat;
	}
	
	public MediaFormat videoSetByteBuffer(String name, byte data[], int len) {
		if(videoFormat != null) {
			ByteBuffer bb = ByteBuffer.wrap(data, 0, len);
			videoFormat.setByteBuffer(name, bb);
			Log.d("ga_log", "videoSetByteBuffer: name="+name+"; size="+Integer.toString(len));
		}
		return videoFormat;
	}
	
	public MediaCodec startVideoDecoder() {
		if(videoFormat == null)
			return null;
		//
		try {
		vdecoder = MediaCodec.createDecoderByType(videoFormat.getString(MediaFormat.KEY_MIME));
		} catch(Exception e) {
		return null;
		}
		if(vdecoder == null)
			return null;
		try {
			vdecoder.configure(videoFormat, surface, null, 0);
		} catch(Exception e) {
			Log.e("ga_log", "vdecoder.configure: " + e.toString());
			return null;
		}
		vdecoder.start();
		inputVBuffers = vdecoder.getInputBuffers();
		//
		startVideoRenderer();
		//
		Log.d("ga_log", "video decoder started.");
		return vdecoder;
	}

	private int currVInbufIdx = -1;
	private long currVPts = -1;
	private int currVFlag = 0;

	public int decodeVideo(byte[] data, int size, long presentationTimeUs, boolean rtpMarker, int flag) {
		int inbufIdx, ret = 0;
		watchdogTick++;
		if(vdecoder == null)
			return -1;
		try {
			do {
				int pos, remaining;
				if(currVInbufIdx == -1) {
					inbufIdx = vdecoder.dequeueInputBuffer(1000000/*1s*/);
					if(inbufIdx < 0) {
						Log.d("ga_log",
								String.format("decodeVideo@1: frame dropped, marker=%d, flag=%d, size=%d",
										rtpMarker ? 1 : 0,
										flag == MediaCodec.BUFFER_FLAG_SYNC_FRAME ? 1 : 0,
										size));
						ret = -1;
						break;
					}
					currVInbufIdx = inbufIdx;
					currVPts = presentationTimeUs;
					currVFlag = flag;
					inputVBuffers[currVInbufIdx].clear();
				}
				pos = inputVBuffers[currVInbufIdx].position();
				remaining = inputVBuffers[currVInbufIdx].remaining();
				if(flag==currVFlag && remaining >= size && currVPts == presentationTimeUs
				&& rtpMarker == false
				/*&&(pos < vbufferLevel || vbufferLevel<=0)*/) {
					 /* Queue without decoding */
					inputVBuffers[currVInbufIdx].put(data, 0, size);
				} else {
					boolean queued = false;
					if(flag==currVFlag && remaining >= size && currVPts == presentationTimeUs
					&& rtpMarker) {
						inputVBuffers[currVInbufIdx].put(data, 0, size);
						queued = true;
					}
//					Log.d("ga_log", "decodeVideo: submit,"
//							+ " pts=" + Long.toString(currVPts)
//							+ " position="+inputVBuffers[currVInbufIdx].position()
//							+ " capacity="+inputVBuffers[currVInbufIdx].capacity()
//							);
					vdecoder.queueInputBuffer(currVInbufIdx, 0, inputVBuffers[currVInbufIdx].position(), currVPts, currVFlag);
					//
					inbufIdx = vdecoder.dequeueInputBuffer(1000000/*1s*/);
					if(inbufIdx >= 0) {
						currVInbufIdx = inbufIdx;
						currVPts = presentationTimeUs;
						currVFlag = flag;
						inputVBuffers[currVInbufIdx].clear();
						if(queued == false) {
							inputVBuffers[inbufIdx].put(data, 0, size);
						}
					} else {
						currVInbufIdx = -1;
						currVPts = -1;
						ret = -1;
						Log.d("ga_log",
								String.format("decodeVideo@2: frame dropped, marker=%d, flag=%d, size=%d",
										rtpMarker ? 1 : 0,
										flag == MediaCodec.BUFFER_FLAG_SYNC_FRAME ? 1 : 0,
										size));
					}
				}
			} while(false);
		} catch(Exception e) {
			Log.d("ga_log", e.toString());
			e.printStackTrace();
			return -1;
		}
		return ret;
	}	

	public void stopVideoDecoder() {
		if(vdecoder == null)
			return;
		//
		stopVideoRenderer();
		//
		vdecoder.stop();
		vdecoder.release();
		vdecoder = null;
		currVInbufIdx = -1;
		currVPts = -1;
		//
		Log.d("ga_log", "video decoder stopped.");
	}
	
	/* JNI stuff */
	static {
		// full
		System.loadLibrary("gnustl_shared");
		System.loadLibrary("mp3lame");
		System.loadLibrary("opus");
		System.loadLibrary("ogg");
		System.loadLibrary("vorbis");
		System.loadLibrary("vorbisenc");
		System.loadLibrary("vorbisfile");
		System.loadLibrary("x264");
		// minimum
		System.loadLibrary("gaclient");
	}
	//
	public native boolean initGAClient(Object thiz);
	//
	public native void resetConfig();
	public native void setProtocol(String proto);
	public native void setHost(String proto);
	public native void setPort(int port);
	public native void setObjectPath(String objpath);
	public native void setRTPOverTCP(boolean enabled);
	public native void setCtrlEnable(boolean enabled);
	public native void setCtrlProtocol(boolean tcp);
	public native void setCtrlPort(int port);
	private native void setBuiltinAudioInternal(boolean enable);
	public void setBuiltinAudio(boolean enable) {
		builtinAudioDecoder = enable;
		setBuiltinAudioInternal(enable);
	}
	private native void setBuiltinVideoInternal(boolean enable);
	public void setBuiltinVideo(boolean enable) {
		builtinVideoDecoder = enable;
		setBuiltinVideoInternal(enable);
	}
	public native void setAudioCodec(int samplerate, int channels);
	public native void setDropLateVideoFrame(int ms);
	// control methods
	public native void sendKeyEvent(boolean pressed, int scancode, int sym, int mod, int unicode);
	public native void sendMouseKey(boolean pressed, int button, int x, int y);
	public native void sendMouseMotion(int x, int y, int xrel, int yrel, int state, boolean relative);
	public native void sendMouseWheel(int dx, int dy);
	// GL
	public native void GLresize(int width, int height);
	private native boolean GLrenderInternal();
	public boolean GLrender() {
		if(GLrenderInternal()) {
			videoRendered = true;
			return true;
		}
		return false;
			
	}
	//
	private native int audioBufferFill(byte[] stream, int size);
	//
	public native boolean rtspConnect();
	public native void rtspDisconnect();
}
