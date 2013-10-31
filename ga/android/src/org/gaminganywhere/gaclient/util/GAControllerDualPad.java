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

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class GAControllerDualPad extends GAController implements
	OnClickListener, Pad.PartitionEventListener
{
	
	private Button buttonEsc = null;
	private Button buttonBack = null;
	private Pad padLeft = null;	
	private Pad padRight = null;

	public GAControllerDualPad(Context context) {
		super(context);
	}

	public static String getName() {
		return "DualPad";
	}
	
	public static String getDescription() {
		return "Arrow keys and mouse buttons";
	}

	@Override
	public void onDimensionChange(int width, int height) {
		int keyBtnWidth = width/13;
		int keyBtnHeight = height/9;
		int padSize = height*2/5;
		// must be called first
		super.onDimensionChange(width,  height);
		//
		buttonEsc = null;
		buttonEsc = new Button(getContext());
		buttonEsc.setTextSize(10);
		buttonEsc.setText("ESC");
		buttonEsc.setOnClickListener(this);
		placeView(buttonEsc, width-keyBtnWidth/5-keyBtnWidth, keyBtnHeight/3, keyBtnWidth, keyBtnHeight);
		//
		buttonBack = null;
		buttonBack = new Button(getContext());
		buttonBack.setTextSize(10);
		buttonBack.setText("<<");
		buttonBack.setOnClickListener(this);
		placeView(buttonBack, keyBtnWidth/5, keyBtnHeight/3, keyBtnWidth, keyBtnHeight);
		//
		padLeft = null;
		padLeft = new Pad(getContext());
		padLeft.setAlpha((float) 0.5);
		padLeft.setOnTouchListener(this);
		padLeft.setPartition(12);
		padLeft.setPartitionEventListener(this);
		padLeft.setDrawPartitionAll(false);
		placeView(padLeft, width/30, height-padSize-height/30, padSize, padSize);
		//
		padRight = null;
		padRight = new Pad(getContext());
		padRight.setAlpha((float) 0.5);
		padRight.setOnTouchListener(this);
		padRight.setPartition(2);
		padRight.setPartitionEventListener(this);
		placeView(padRight, width-width/30-padSize, height-padSize-height/30, padSize, padSize);
	}
	
	@Override
	public boolean onTouch(View v, MotionEvent evt) {
		int count = evt.getPointerCount();
		if(count==1 && (v == padLeft || v == padRight)) {
			if(((Pad) v).onTouch(evt));
				return true;
		}
		// must be called last
		return super.onTouch(v, evt);
	}

	@Override
	public void onClick(View v) {
		if(v == buttonEsc) {
			sendKeyEvent(true, SDL2.Scancode.ESCAPE, 0x1b, 0, 0);
			sendKeyEvent(false, SDL2.Scancode.ESCAPE, 0x1b, 0, 0);
		} else if(v == buttonBack) {
			finish();
		}
	}

	private boolean keyLeft = false;
	private boolean keyRight = false;
	private boolean keyUp = false;
	private boolean keyDown = false;
	private void emulateArrowKeys(int action, int part) {
		boolean myKeyLeft, myKeyRight, myKeyUp, myKeyDown;
		myKeyLeft = keyLeft;
		myKeyRight = keyRight;
		myKeyUp = keyUp;
		myKeyDown = keyDown;
		switch(action) {
		case MotionEvent.ACTION_DOWN:
		case MotionEvent.ACTION_POINTER_DOWN:
		case MotionEvent.ACTION_MOVE:
			// partition mappings for keys:
			// - up: 11, 12, 1, 2
			// - right: 2, 3, 4, 5
			// - down: 5, 6, 7, 8
			// - left: 8, 9, 10, 11
			switch(part) {
			case 0:
				myKeyUp = myKeyRight = myKeyDown = myKeyLeft = false;
				break;
			// single keys
			case 12: case 1:
				myKeyUp = true;
				myKeyRight = myKeyDown = myKeyLeft = false;
				break;
			case 3: case 4:
				myKeyRight = true;
				myKeyUp = myKeyDown = myKeyLeft = false;
				break;
			case 6: case 7:
				myKeyDown = true;
				myKeyUp = myKeyRight = myKeyLeft = false;
				break;
			case 9: case 10:
				myKeyLeft = true;
				myKeyUp = myKeyRight = myKeyDown = false;
				break;
			// hybrid keys
			case 2:
				myKeyUp = myKeyRight = true;
				myKeyDown = myKeyLeft = false;
				break;
			case 5:
				myKeyRight = myKeyDown = true;
				myKeyUp = myKeyLeft = false;
				break;
			case 8:
				myKeyDown = myKeyLeft = true;
				myKeyUp = myKeyRight = false;
				break;
			case 11:
				myKeyLeft = myKeyUp = true;
				myKeyRight = myKeyDown = false;
				break;
			}
			break;
		case MotionEvent.ACTION_UP:
		case MotionEvent.ACTION_POINTER_UP:
			if(keyLeft)
				this.sendKeyEvent(false, SDL2.Scancode.LEFT, 0, 0, 0);
			if(keyRight)
				this.sendKeyEvent(false, SDL2.Scancode.RIGHT, 0, 0, 0);
			if(keyUp)
				this.sendKeyEvent(false, SDL2.Scancode.UP, 0, 0, 0);
			if(keyDown)
				this.sendKeyEvent(false, SDL2.Scancode.DOWN, 0, 0, 0);
			myKeyUp = myKeyRight = myKeyDown = myKeyLeft = false;
			break;
		}
		if(myKeyUp != keyUp) {
			this.sendKeyEvent(myKeyUp, SDL2.Scancode.UP, SDL2.Keycode.UP, 0, 0);
			//Log.d("ga_log", String.format("Key up %s", myKeyUp ? "down" : "up"));
		}
		if(myKeyDown != keyDown) {
			this.sendKeyEvent(myKeyDown, SDL2.Scancode.DOWN, SDL2.Keycode.DOWN, 0, 0);
			//Log.d("ga_log", String.format("Key down %s", myKeyDown ? "down" : "up"));
		}
		if(myKeyLeft != keyLeft) {
			this.sendKeyEvent(myKeyLeft, SDL2.Scancode.LEFT, SDL2.Keycode.LEFT, 0, 0);
			//Log.d("ga_log", String.format("Key left %s", myKeyLeft ? "down" : "up"));
		}
		if(myKeyRight != keyRight) {
			this.sendKeyEvent(myKeyRight, SDL2.Scancode.RIGHT, SDL2.Keycode.RIGHT, 0, 0);
			//Log.d("ga_log", String.format("Key right %s", myKeyRight ? "down" : "up"));
		}
		keyUp = myKeyUp;
		keyDown = myKeyDown;
		keyLeft = myKeyLeft;
		keyRight = myKeyRight;
	}
	
	private int mouseButton = -1;
	private void emulateMouseButtons(int action, int part) {
		switch(action) {
		case MotionEvent.ACTION_DOWN:
		//case MotionEvent.ACTION_POINTER_DOWN:
			if(part == 0 || part == 2)
				mouseButton = SDL2.Button.LEFT;
			else
				mouseButton = SDL2.Button.RIGHT;
			this.sendMouseKey(true, mouseButton, getMouseX(), getMouseY());
			break;
		case MotionEvent.ACTION_UP:
		//case MotionEvent.ACTION_POINTER_UP:
			if(mouseButton != -1) {
				sendMouseKey(false, mouseButton, getMouseX(), getMouseY());
				mouseButton = -1;
			}
			break;
		}
	}
	
	@Override
	public void onPartitionEvent(View v, int action, int part) {
//		String obj = "null";
//		if(v == padLeft)	obj = "padLeft";
//		if(v == padRight)	obj = "padRight";
//		Log.d("ga_log", String.format("[%s] partition event: action=%d, part=%d", obj, action, part));
		// Left: emulated arrow keys
		if(v == padLeft) {
			emulateArrowKeys(action, part);
			return;
		}
		// Right: emulated mouse buttons
		if(v == padRight) {
			emulateMouseButtons(action, part);
			return;
		}
	}
}
