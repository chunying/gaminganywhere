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

import org.gaminganywhere.gaclient.R;
import org.gaminganywhere.gaclient.util.Pad.PartitionEventListener;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ImageButton;

public class GAControllerN64 extends GAController implements
	OnClickListener, PartitionEventListener
 {
	private Button buttonEsc = null;
	private Button buttonBack = null;
	private Button buttonStart = null;
	private Pad padLeft = null;
	private Pad padRight = null;
	private Button buttonL = null;
	private Button buttonR = null;
	private Button buttonA = null;
	private Button buttonB = null;
	private Button buttonZ = null;

	public GAControllerN64(Context c) {
		super(c);
	}

	public static String getName() {
		return "N64";
	}

	public static String getDescription() {
		return "Emulated N64 controller";
	}

	@Override
	public void onDimensionChange(int width, int height) {
		final int nButtonW = 8;
		final int nButtonH = 10;
		int keyBtnWidth = width / (nButtonW + 1);
		int keyBtnHeight = height / (nButtonH + 1);
		int marginW = (width / (nButtonW + 1)) / (nButtonW + 1);
		int marginH = (height / (nButtonH + 1)) / (nButtonH + 1);
		int padSize = height * 7 / 15; //2 / 5;
		int cBtnSize = (int) (padSize * 0.7 / 2);
		// check width/height
		if (width < 0 || height < 0)
			return;
		// must be called first
		super.setMouseVisibility(false);
		super.onDimensionChange(width, height);
		//
		buttonEsc = newButton("ESC", width - marginW - keyBtnWidth, marginH,
				keyBtnWidth, keyBtnHeight);
		buttonEsc.setOnTouchListener(this);
		//
		buttonBack = newButton("<<", marginW, marginH, keyBtnWidth, keyBtnHeight);
		buttonBack.setOnClickListener(this);
		//
		buttonStart = newButton("START", marginW + (marginW + keyBtnWidth) * 4,
				height - marginH - keyBtnHeight, keyBtnWidth, keyBtnHeight);
		buttonStart.setOnTouchListener(this);
		//
		buttonL = newButton("L", marginW*2+cBtnSize, height-marginH*2-padSize-keyBtnHeight,
					padSize, keyBtnHeight);
		buttonL.setOnTouchListener(this);
		//
		buttonR = newButton("R", width-marginW*2-cBtnSize-padSize, height-marginH*2-padSize-keyBtnHeight,
					padSize, keyBtnHeight);
		buttonR.setOnTouchListener(this);
		//
		buttonA = newButton("A", width-marginW-cBtnSize, height-marginH-padSize, cBtnSize, cBtnSize);
		buttonA.setOnTouchListener(this);
		//
		buttonB = newButton("B", width-marginW-cBtnSize, height-padSize+cBtnSize, cBtnSize, cBtnSize);
		buttonB.setOnTouchListener(this);
		//
		buttonZ = newButton("Z", marginW, height-marginH-padSize, cBtnSize, cBtnSize);
		buttonZ.setOnTouchListener(this);
		//
		padLeft = new Pad(getContext());
		padLeft.setAlpha((float) 0.5);
		padLeft.setOnTouchListener(this);
		padLeft.setPartition(12);
		padLeft.setPartitionEventListener(this);
		padLeft.setDrawPartitionAll(false);
		placeView(padLeft, marginW*2+cBtnSize, height-marginH-padSize, padSize, padSize);
		//
		padRight = new Pad(getContext());
		padRight.setAlpha((float) 0.5);
		padRight.setOnTouchListener(this);
		padRight.setPartition(8);
		padRight.setPartitionEventListener(this);
		padRight.setDrawPartitionAll(false);
		padRight.setDrawPartitionLine(new int[] {1, 3, 5, 7});
		padRight.setDrawLabel(new int[] {3, 5, 1, 3, 5, 7, 7, 1}, new String[] {"D-Down", "D-Right", "D-Left", "D-Up"});
		placeView(padRight, width-marginW*2-cBtnSize-padSize, height-marginH-padSize, padSize, padSize);
	}

	@Override
	public boolean onTouch(View v, MotionEvent evt) {
		int count = evt.getPointerCount();
		int action = evt.getActionMasked();
		//
		if (v == buttonL)
			return handleButtonTouch(action, SDL2.Scancode.X, SDL2.Keycode.x, 0, 0);
		if (v == buttonR)
			return handleButtonTouch(action, SDL2.Scancode.C, SDL2.Keycode.c, 0, 0);
		if (v == buttonA)
			return handleButtonTouch(action, SDL2.Scancode.LSHIFT, SDL2.Keycode.LSHIFT, 0, 0);
		if (v == buttonB)
			return handleButtonTouch(action, SDL2.Scancode.LCTRL, SDL2.Keycode.LCTRL, 0, 0);
		if (v == buttonZ)
			return handleButtonTouch(action, SDL2.Scancode.Z, SDL2.Keycode.z, 0, 0);
		if (v == buttonStart)
			return handleButtonTouch(action, SDL2.Scancode.RETURN, SDL2.Keycode.RETURN, 0, 0);
		if (v == buttonEsc)
			return handleButtonTouch(action, SDL2.Scancode.ESCAPE, SDL2.Keycode.ESCAPE, 0, 0);
		if (count == 1 && (v == padLeft || v == padRight)) {
			if (((Pad) v).onTouch(evt))
				;
			return true;
		}
		// must be called last
		// XXX: not calling super.onTouch() because we do not need a mouse
		// return super.onTouch(v, evt);
		return false;
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
		switch (action) {
		case MotionEvent.ACTION_DOWN:
		case MotionEvent.ACTION_POINTER_DOWN:
		case MotionEvent.ACTION_MOVE:
			// partition mappings for keys:
			// - up: 11, 12, 1, 2
			// - right: 2, 3, 4, 5
			// - down: 5, 6, 7, 8
			// - left: 8, 9, 10, 11
			switch (part) {
			case 0:
				myKeyUp = myKeyRight = myKeyDown = myKeyLeft = false;
				break;
			// single keys
			case 12:
			case 1:
				myKeyUp = true;
				myKeyRight = myKeyDown = myKeyLeft = false;
				break;
			case 3:
			case 4:
				myKeyRight = true;
				myKeyUp = myKeyDown = myKeyLeft = false;
				break;
			case 6:
			case 7:
				myKeyDown = true;
				myKeyUp = myKeyRight = myKeyLeft = false;
				break;
			case 9:
			case 10:
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
			if (keyLeft)
				this.sendKeyEvent(false, SDL2.Scancode.LEFT, SDL2.Keycode.LEFT,
						0, 0);
			if (keyRight)
				this.sendKeyEvent(false, SDL2.Scancode.RIGHT,
						SDL2.Keycode.RIGHT, 0, 0);
			if (keyUp)
				this.sendKeyEvent(false, SDL2.Scancode.UP, SDL2.Keycode.UP, 0,
						0);
			if (keyDown)
				this.sendKeyEvent(false, SDL2.Scancode.DOWN, SDL2.Keycode.DOWN,
						0, 0);
			myKeyUp = myKeyRight = myKeyDown = myKeyLeft = false;
			break;
		}
		if (myKeyUp != keyUp) {
			this.sendKeyEvent(myKeyUp, SDL2.Scancode.UP, SDL2.Keycode.UP, 0, 0);
			// Log.d("ga_log", String.format("Key up %s", myKeyUp ? "down" :
			// "up"));
		}
		if (myKeyDown != keyDown) {
			this.sendKeyEvent(myKeyDown, SDL2.Scancode.DOWN, SDL2.Keycode.DOWN,
					0, 0);
			// Log.d("ga_log", String.format("Key down %s", myKeyDown ? "down" :
			// "up"));
		}
		if (myKeyLeft != keyLeft) {
			this.sendKeyEvent(myKeyLeft, SDL2.Scancode.LEFT, SDL2.Keycode.LEFT,
					0, 0);
			// Log.d("ga_log", String.format("Key left %s", myKeyLeft ? "down" :
			// "up"));
		}
		if (myKeyRight != keyRight) {
			this.sendKeyEvent(myKeyRight, SDL2.Scancode.RIGHT,
					SDL2.Keycode.RIGHT, 0, 0);
			// Log.d("ga_log", String.format("Key right %s", myKeyRight ? "down"
			// : "up"));
		}
		keyUp = myKeyUp;
		keyDown = myKeyDown;
		keyLeft = myKeyLeft;
		keyRight = myKeyRight;
	}

	private int lastScan = -1;
	private int lastKey = -1;
	private void emulateDPad(int action, int part) {
		switch(action) {
		case MotionEvent.ACTION_DOWN:
		//case MotionEvent.ACTION_POINTER_DOWN:
			switch(part) {
			case 2: case 3: // define 'Right' button
				lastScan = SDL2.Scancode.L;
				lastKey = SDL2.Keycode.l;
				break;
			case 4: case 5: // define 'Down' button
				lastScan = SDL2.Scancode.K;
				lastKey = SDL2.Keycode.k;
				break;
			case 6: case 7: // define 'Left' button
				lastScan = SDL2.Scancode.J;
				lastKey = SDL2.Keycode.j;
				break;
			case 1: case 8: // define 'Up' button
				lastScan = SDL2.Scancode.I;
				lastKey = SDL2.Keycode.i;
				break;
			}
			if(part >= 1 && part <= 8) {
				this.sendKeyEvent(true, lastScan, lastKey, 0, 0);
			}
			break;
		case MotionEvent.ACTION_UP:
		//case MotionEvent.ACTION_POINTER_UP:
			if(lastScan != -1) {
				sendKeyEvent(false, lastScan, lastKey, 0, 0);
				lastKey = -1;
				lastScan = -1;
			}
			break;
		}
	}

	@Override
	public void onPartitionEvent(View v, int action, int part) {
		// String obj = "null";
		// if(v == padLeft) obj = "padLeft";
		// if(v == padRight) obj = "padRight";
		// Log.d("ga_log",
		// String.format("[%s] partition event: action=%d, part=%d", obj,
		// action, part));
		// Left: emulated arrow keys
		if (v == padLeft) {
			emulateArrowKeys(action, part);
			return;
		} else if(v == padRight) {
			emulateDPad(action, part);
			return;
		}
	}

	@Override
	public void onClick(View v) {
		if (v == buttonBack) {
			finish();
		}
	}

}
