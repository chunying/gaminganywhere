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

package org.gaminganywhere.gaclient.util;

import org.gaminganywhere.gaclient.GAClient;
import org.gaminganywhere.gaclient.R;
import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.OvalShape;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.RelativeLayout;

public class GAController implements OnTouchListener {

	private Context context = null;
	private GAClient client = null;
	private int viewWidth = 0;
	private int viewHeight = 0;
	private float mappedX = (float) -1.0;
	private float mappedY = (float) -1.0;
	private float mappedDeltaX = (float) -1.0;
	private float mappedDeltaY = (float) -1.0;

	private RelativeLayout relativeLayout = null;
	private ImageView panel = null;
	private ImageView cursor = null;
	private boolean showMouse = true;
	private boolean enableTouchClick = true;
	private float mouseX = (float) -1.0;
	private float mouseY = (float) -1.0;

	private final long clickDetectionTime = 100;	/* in ms */
	private final float clickDetectionDist = 81;	/* in pixel^2 */
	
	public GAController(Context context) {
		this.context = context;
		relativeLayout = new RelativeLayout(getContext());
	}

	public static String getName() {
		return null;
	}
	
	public static String getDescription() {
		return null;
	}
	
	public Context getContext() {
		return this.context;
	}
	
	public ImageView getPanel() {
		return panel;
	}
	
	public void finish() {
		if(context != null) {
			((Activity) context).setResult(0);
			((Activity) context).finish();
		}
	}
	
	public void setClient(GAClient client) {
		this.client = client;
	}
	
	public View getView() {
		return relativeLayout;
	}

	public void onDimensionChange(int width, int height) {
		relativeLayout.removeAllViews();
		//
		mouseX = (float) (width / 2);
		mouseY = (float) (height / 2);
		// panel - the lowest UI, note the Z-order
		panel = null;
		panel = new ImageView(getContext());
		panel.setAlpha((float) 0);	// fully transparent
		panel.setOnTouchListener(this);
		placeView(panel, 0, 0, width, height);
		// cursor
		cursor = null;
		cursor = new ImageView(getContext());
		cursor.setImageResource(R.drawable.mouse32);
		placeView(cursor, width/2, height/2, 20, 32);
		if(showMouse)
			cursor.setVisibility(View.VISIBLE);
		else
			cursor.setVisibility(View.INVISIBLE);
		// move mouse to its correct position
		sendMouseMotion(mouseX, mouseY, 0, 0, 0, false);
	}

	public void setViewDimension(int width, int height) {
		viewWidth = width;
		viewHeight = height;
		Log.d("ga_log", String.format("controller: view dimension = %dx%d",
				viewWidth, viewHeight));
		onDimensionChange(width, height);
	}

	public int getViewWidth() {
		return viewWidth;
	}
	
	public int getViewHeight() {
		return viewHeight;
	}
	
	private boolean mapCoordinate(float x, float y) {
		return mapCoordinate(x, y, (float) 0.0, (float) 0.0);
	}
	
	private boolean mapCoordinate(float x, float y, float dx, float dy) {
		int pxsize, pysize;
		//
		if(client == null)
			return false;
		if(viewWidth <= 0 || viewHeight <= 0)
			return false;
		// map coordinates
		pxsize = client.getScreenWidth();
		pysize = client.getScreenHeight();
		if(pxsize <= 0 || pysize <= 0)
			return false;
		mappedX = x * pxsize / viewWidth;
		mappedY = y * pysize / viewHeight;
		mappedDeltaX = dx * pxsize / viewWidth;
		mappedDeltaY = dy * pysize / viewHeight;
		return true;
	}

	protected void moveView(View v, int left, int top, int width, int height) {
		RelativeLayout.LayoutParams params = null;
		params = new RelativeLayout.LayoutParams(width, height);
		params.leftMargin = left;
		params.topMargin = top;
		v.setLayoutParams(params);
	}
	
	protected void placeView(View v, int left, int top, int width, int height) {
		RelativeLayout.LayoutParams params = null;
		params = new RelativeLayout.LayoutParams(width, height);
		params.leftMargin = left;
		params.topMargin = top;
		relativeLayout.addView(v, params);
	}
	
	protected Button newButton(String label, int left, int top, int width, int height) {
		Button b = new Button(getContext());
		//b.setAlpha((float) 0.5);
		b.setTextSize(10);
		b.setText(label);
		placeView(b, left, top, width, height);
		return b;
	}
	
	protected ImageButton newImageButton(int left, int top, int width, int height) {
		ImageButton b = new ImageButton(getContext());
		//ShapeDrawable s = new ShapeDrawable();
		//s.setShape(new OvalShape());
		//b.setBackground(s);
		//b.setAlpha((float) 0.5);
		placeView(b, left, top, width, height);
		return b;
	}
	
	public boolean handleButtonTouch(int action, int scancode, int keycode, int mod, int unicode) {
		switch(action) {
		case MotionEvent.ACTION_DOWN:
		//case MotionEvent.ACTION_POINTER_DOWN:
			sendKeyEvent(true, scancode, keycode, mod, unicode);
			break;
		case MotionEvent.ACTION_UP:
		//case MotionEvent.ACTION_POINTER_UP:
			sendKeyEvent(false, scancode, keycode, mod, unicode);
			break;
		}
		return false;
	}
	
	public void setMouseVisibility(boolean visible) {
		showMouse = visible;
		if(cursor == null)
			return;
		if(visible)
			cursor.setVisibility(View.VISIBLE);
		else
			cursor.setVisibility(View.INVISIBLE);
	}
	
	public void setEnableTouchClick(boolean enable) {
		enableTouchClick = enable;
	}
	
	public float getMouseX() {
		return mouseX;
	}
	
	public float getMouseY() {
		return mouseY;
	}
	
	protected void moveMouse(float dx, float dy) {
		mouseX += dx;
		mouseY += dy;
		if(mouseX < 0)
			mouseX = 0;
		if(mouseY < 0)
			mouseY = 0;
		if(mouseX >= getViewWidth())
			mouseX = getViewWidth() - 1;
		if(mouseY >= getViewHeight())
			mouseY = getViewHeight() - 1;
		return;
	}
	
	protected void drawCursor(int x, int y) {
		if(cursor==null || showMouse==false)
			return;
		cursor.setVisibility(View.VISIBLE);
		moveView(cursor, x, y, 20, 32);
	}
	
	public void sendKeyEvent(boolean pressed, int scancode, int sym, int mod, int unicode) {
		if(client == null)
			return;
		client.sendKeyEvent(pressed, scancode, sym, mod, unicode);
	}
	
	public void sendMouseKey(boolean pressed, int button, float x, float y) {
		if(mapCoordinate(x, y) == false)
			return;
//		Log.d("ga_log", String.format("send-mouse-key: %s(%d) %f,%f",
//				pressed ? "down" : "up",
//				button, mappedX, mappedY));
		client.sendMouseKey(pressed, button, (int) mappedX, (int) mappedY);
	}
	
	public void sendMouseMotion(float x, float y, float xrel, float yrel, int state, boolean relative) {
		if(mapCoordinate(x, y, xrel, yrel) == false)
			return;
//		Log.d("ga_log", String.format("send-mouse-motion: %f,%f (%f,%f) state=%d (%s)",
//				mappedX, mappedY, mappedDeltaX, mappedDeltaY, state,
//				relative ? "relative" : "absolute"));
		client.sendMouseMotion((int) mappedX, (int) mappedY,
				(int) mappedDeltaX, (int) mappedDeltaY, state, relative);
	}
	
	public void sendMouseWheel(float dx, float dy) {
		if(mapCoordinate(dx, dy) == false)
			return;
		client.sendMouseWheel((int) mappedX, (int) mappedY);
	}

	private float lastX = (float) -1.0;
	private float lastY = (float) -1.0;
	private float initX = (float) -1.0;
	private float initY = (float) -1.0;
	private long lastTouchTime = -1;
	@Override
	public boolean onTouch(View v, MotionEvent evt) {
		int count = evt.getPointerCount();
		int action = evt.getActionMasked();
		float x = evt.getX();
		float y = evt.getY();
//		Log.d("ga_log", String.format("onTouch[panel]: count=%d, action=%d, x=%f, y=%f",
//				count, action, x, y));
		// touch on the panel
		if(v == panel) {
			switch(action) {
			case MotionEvent.ACTION_DOWN:
			//case MotionEvent.ACTION_POINTER_DOWN:
				if(count == 1) {
					initX = lastX = x;
					initY = lastY = y;
					lastTouchTime = System.currentTimeMillis();
				}
				break;
			case MotionEvent.ACTION_UP:
			//case MotionEvent.ACTION_POINTER_UP:
				if(count == 1) {
					long timeOffset = System.currentTimeMillis() - lastTouchTime; 
					float distOffset = (x-initX)*(x-initX)+(y-initY)*(y-initY);
					if(enableTouchClick
					&& timeOffset < clickDetectionTime
					&& distOffset < clickDetectionDist) {
						sendMouseKey(true, SDL2.Button.LEFT, getMouseX(), getMouseY());
						sendMouseKey(false, SDL2.Button.LEFT, getMouseX(), getMouseY());
					}
					lastX = -1;
					lastY = -1;
				}
				break;
			case MotionEvent.ACTION_MOVE:
				if(count == 1) {
					float dx = x-lastX;
					float dy = y-lastY;
					moveMouse(dx, dy);
					sendMouseMotion(mouseX, mouseY, dx, dy, 0, /*relative=*/false);
					drawCursor((int) mouseX, (int) mouseY);
					lastX = x;
					lastY = y;
				}
				break;
			}
			return true;
		}
		return false;
	}
}
