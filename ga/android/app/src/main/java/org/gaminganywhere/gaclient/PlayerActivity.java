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

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import org.gaminganywhere.gaclient.util.GAController;
import org.gaminganywhere.gaclient.util.GAControllerBasic;
import org.gaminganywhere.gaclient.util.GAControllerDualPad;
import org.gaminganywhere.gaclient.util.GAControllerEmpty;
import org.gaminganywhere.gaclient.util.GAControllerLimbo;
import org.gaminganywhere.gaclient.util.GAControllerN64;
import org.gaminganywhere.gaclient.util.GAControllerNDS;
import org.gaminganywhere.gaclient.util.GAControllerPSP;
import org.gaminganywhere.gaclient.util.GAControllerPadABXY;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.Toast;

public class PlayerActivity extends Activity implements SurfaceHolder.Callback, GLSurfaceView.Renderer {

	private PowerManager.WakeLock mWakeLock = null;
	private GAClient client = null;
	private Surface surface = null;
	private Handler handler = null;
	private boolean builtinVideo = true;
	private int viewWidth = 0;
	private int viewHeight = 0;

	public static final int MSG_SHOWTOAST = 1;
	public static final int MSG_QUIT = 2;
	public static final int MSG_RENDER = 3;
	
	public static final int EXIT_NORMAL = 0;
	public static final int EXIT_TIMEOUT = 2;
	
	private FrameLayout topLayout = null;
	private GAController controller = null;
	private SurfaceView contentView = null;
	private static final int uiVisibility = 0
			| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
			| View.SYSTEM_UI_FLAG_FULLSCREEN
			| View.SYSTEM_UI_FLAG_LAYOUT_STABLE
			// XXX: commented out, unless we have a good solution to hide NAV bar permanently
			//| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
			//| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION 
			//| View.SYSTEM_UI_FLAG_LOW_PROFILE
			;
	
	private void myHandler(Message msg) {
		switch(msg.what){
		case MSG_SHOWTOAST:
			showToast((String) msg.obj);
			break;
		case MSG_QUIT:
			this.setResult(msg.arg1);
			this.finish();
			break;
		case MSG_RENDER:
			if(builtinVideo == false) {
				((GLSurfaceView) contentView).requestRender();
			}
			break;
		}
	}

	public Handler getHandler() {
		return handler;
	}
	
	private void showToast(String s) {
		Context context = getApplicationContext();
		Toast t = Toast.makeText(context, s, Toast.LENGTH_SHORT);
		t.show();
	}

	private void connect(Surface surface) {
		// create the AndroidClient object
		if(client == null) {
			Intent intent = getIntent();
			String s = intent.getStringExtra("profile");
			int watchdogTimeout = intent.getIntExtra("watchdogTimeout", 3);
			int dropLateVideoFrame = intent.getIntExtra("dropLateVFrame", -1);
			//
			if(s == null || s.equals("")) {
				showToast("Player: No profile provided");
				return;
			}
			client = new GAClient();
			client.setContext(getApplicationContext());
			client.setActivity(this);
			client.setSurface(surface);
			client.setSurfaceView(contentView);
			client.setHandler(handler);
			if(client.profileLoad(s) == false) {
				showToast("Load profile '" + s + "' failed");
				return;
			}
			client.setBuiltinAudio(intent.getBooleanExtra("builtinAudio", true));
			client.setBuiltinVideo(intent.getBooleanExtra("builtinVideo", true));
			//
			if(dropLateVideoFrame > 0) {
				client.setDropLateVideoFrame(dropLateVideoFrame);
			} else {
				client.setDropLateVideoFrame(-1);
			}
			//
			client.startRTSPClient();
			if(watchdogTimeout > 0) {
				client.watchdogSetTimeout(watchdogTimeout);
				client.startWatchdog();
			}
			controller.setClient(client);
		}
		return;
	}
	
	private void disconnect() {
		controller.setClient(null);
		if(client != null) {
			client.stopWatchdog();
			client.stopRTSPClient();
			client.setSurface(null);
			client = null;
		}
		return;
	}
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);
		
		Intent intent = getIntent();
		builtinVideo = intent.getBooleanExtra("builtinVideo", true);

		final PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
		mWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, "GAClient");

		if(intent.getBooleanExtra("portraitMode", false) == false) {
			setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		} else {
			setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		}
		this.getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);
		this.getWindow().requestFeature(Window.FEATURE_NO_TITLE);

		DisplayMetrics displaymetrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics(displaymetrics);
		viewWidth = displaymetrics.widthPixels;
		viewHeight = displaymetrics.heightPixels;
		Log.d("ga_log", String.format("View dimension = %dx%d", viewWidth, viewHeight));

		topLayout = new FrameLayout(this);

		if(builtinVideo) {
			contentView = new SurfaceView(this);
			contentView.getHolder().addCallback(this);
			//contentView.setClickable(false);
			surface = contentView.getHolder().getSurface();
			Log.d("ga_log", "Player: use built-in MediaCodec video decoder.");
		} else {
			contentView = new GLSurfaceView(this);
			contentView.getHolder().addCallback(this);
			((GLSurfaceView) contentView).setRenderer(this);
			((GLSurfaceView) contentView).setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
			Log.d("ga_log", "Player: use ffmpeg video decoer.");
		}
		contentView.setSystemUiVisibility(uiVisibility);
		contentView.setKeepScreenOn(true);

		// for controller placement
		do {
			String cname = intent.getStringExtra("controller");
			if(cname == null) {
				controller = new GAControllerBasic(this);
			} else if(cname.equals(GAControllerEmpty.getName())) {
				controller = new GAControllerEmpty(this);
			} else if(cname.equals(GAControllerDualPad.getName())) {
				controller = new GAControllerDualPad(this);
			} else if(cname.equals(GAControllerLimbo.getName())){
				controller = new GAControllerLimbo(this);
			} else if(cname.equals(GAControllerN64.getName())){
				controller = new GAControllerN64(this);
			} else if(cname.equals(GAControllerNDS.getName())){
				controller = new GAControllerNDS(this);
			} else if(cname.equals(GAControllerPadABXY.getName())) {
				controller = new GAControllerPadABXY(this);
			} else if(cname.equals(GAControllerPSP.getName())) {
				controller = new GAControllerPSP(this);
			} else {
				controller = new GAControllerBasic(this);
			}
			controller.setViewDimension(viewWidth, viewHeight);
		} while(false);
		
		topLayout.addView(contentView);
		topLayout.addView(controller.getView());
		
		setContentView(topLayout);
	}

	@Override
	public void onBackPressed() {
		this.setResult(0);
		super.onBackPressed();
	}

	@Override
	protected void onResume() {
		handler = new Handler() {
			public void handleMessage(Message msg) {
				myHandler(msg);
				super.handleMessage(msg);
			}
		};
		if(builtinVideo==false)
			((GLSurfaceView) contentView).onResume();
		mWakeLock.acquire();
		connect(this.surface);
		super.onResume();
	}

	@Override
	protected void onPause() {
		disconnect();
		mWakeLock.release();
		handler = null;
		if(builtinVideo==false)
			((GLSurfaceView) contentView).onPause();
		super.onPause();
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
	}

	// surfaceHolder callbacks
	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
		Log.d("ga_log", "surface changed: format=" + Integer.toString(format)
				+", " + Integer.toString(width) + "x" + Integer.toString(height));
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		Log.d("ga_log", "surface created.");
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		Log.d("ga_log", "surface destroyed.");
	}

	// GL
	private int frames = 0;
	private long lastFPSt = 0;
	
	@Override
	public void onDrawFrame(GL10 gl) {
		long currFPSt = System.currentTimeMillis();
		if (client == null)
			return;
		if(currFPSt - lastFPSt > 10000) {
			if(lastFPSt > 0) {
				Log.d("ga_log", "Render: fps = "
						+ Double.toString(1000.0 * frames / (currFPSt - lastFPSt)));
			}
			lastFPSt = currFPSt;
			frames = 0;
		}
		if(client.GLrender())
			frames++;
	}

	@Override
	public void onSurfaceChanged(GL10 gl, int width, int height) {
		Log.d("ga_log", "GL surface changed, "
				+ "width=" + Integer.toString(width)
				+ "; height=" + Integer.toString(height));
		if (client != null)
			client.GLresize(width, height);
	}

	@Override
	public void onSurfaceCreated(GL10 gl, EGLConfig config) {
		Log.d("ga_log", "GL surface created.");
	}
}
