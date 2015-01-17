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

import org.gaminganywhere.gaclient.util.Pad.PartitionEventListener;
import org.gaminganywhere.gaclient.util.SDL2.SDLKey;

import android.content.Context;
import android.location.Location;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.View.OnKeyListener;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;

public class GAControllerHWKbdMouse extends GAController implements
	OnClickListener, PartitionEventListener
{
	boolean kbdActive = true;
	private Button buttonEsc = null;
	private Button buttonKbd = null;
	private EditText edittext = null;
	private Pad padLeft = null;
	
	public GAControllerHWKbdMouse(Context context) {
		super(context);
	}
	
	public static String getName() {
		return "GAControllerHWKbdMouse";
	}
	
	public static String getDescription() {
		return "Hardware Keyboard and Mouse";
	}
	@Override
	public void onDimensionChange(int width, int height) {
		int keyBtnWidth = width/13;
		int keyBtnHeight = height/9;
		int padSize = height*2/5;
		// must be called first!
		super.onDimensionChange(width,  height);
		// button ESC
		buttonEsc = null;
		buttonEsc = new Button(getContext());
		buttonEsc.setTextSize(10);
		buttonEsc.setText("ESC");
		buttonEsc.setOnClickListener(this);
		buttonEsc.setFocusable(false);
		
		buttonKbd = new Button(getContext());
		buttonKbd.setTextSize(10);
		buttonKbd.setText("KBD");
		buttonKbd.setOnClickListener(this);
		buttonKbd.setFocusable(false);
		/** @author alberto pajuelo 
		 * Implementation of sending physical keys to the server.
		 * We create a text field to grab keys input.
		 * **/
		edittext = new EditText(getContext());
		
		
		placeView(buttonEsc, width-keyBtnWidth/5-keyBtnWidth, keyBtnHeight/3, keyBtnWidth, keyBtnHeight);
		placeView(buttonKbd, 0, 0, 100, 100);
		/** @author alberto pajuelo 
		 * Implementation of sending physical keys to the server.
		 * Set a key listener for listening physical keys input
		 * **/
		edittext.setOnKeyListener(new OnKeyListener() {
			//FIXME: there is some kind of bug on Android that prevents letters from soft keyboard to trigger onKey event, only DEL, ENTER, and numbers work for now...
			@Override
			public boolean onKey(View v, int keyCode, KeyEvent event) {
				//translation form android keycodes to sdl keys
				SDLKey sdlkey =  SDL2.AndroidKeyCodeToSDLKey(keyCode);
				boolean down = false;		
				if (event.getAction()!=KeyEvent.ACTION_DOWN) {					
					down = false;
				} else {
					down = true;																																																																																																																																																																																																																																																																																																																						
				}
				sendKeyEvent(down,sdlkey.getSecanCode(),sdlkey.getKeyCode() , 0, 0);
				return false;
			}																																																																																																																																																																																																																																																																																																																																													
		});
		
//		TextWatcher textWatcher = new TextWatcher() {
//			@Override
//		    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
//			
//		    }
//		    @Override
//		    public void onTextChanged(CharSequence s, int start, int before,int count) {        
//		    	String chars = s.toString();
//		    	char character = chars.substring(chars.length() - 1).charAt(0);
//		    	
//		    	SDLKey sdlkey =  SDL2.CharToSDLKey(character);
//		    	if(sdlkey != null) {
//		    		sendKeyEvent(true,sdlkey.getSecanCode(),sdlkey.getKeyCode() , 0, 0);
//			    	sendKeyEvent(false,sdlkey.getSecanCode(),sdlkey.getKeyCode() , 0, 0);
//		    	}
//		    }
//			@Override
//			public void afterTextChanged(Editable arg0) {
//				// TODO Auto-generated method stub
//				
//			}       
//		};
//		edittext.addTextChangedListener(textWatcher);
		placeView(edittext, 10, 10, 1, 1);																																																																																																																																																																																						
		//
		padLeft = null;
		padLeft = new Pad(getContext());
		padLeft.setAlpha((float) 0.5);
		padLeft.setOnTouchListener(this);
		padLeft.setPartition(2);
		padLeft.setPartitionEventListener(this);
		//placeView(padLeft, width/30, height-padSize-height/30, padSize, padSize);
	}

	@Override
	public boolean onTouch(View v, MotionEvent evt) {
		int count = evt.getPointerCount();
		if(count==1 && v == padLeft) {
			if(((Pad) v).onTouch(evt));
				return true;
		}
		// must be called last
		return super.onTouch(v, evt);
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
		if(v == padLeft) {
			emulateMouseButtons(action, part);
			return;
		}
	}

	@Override
	public void onClick(View v) {
		InputMethodManager imm = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);

		if(v == buttonEsc) {
			sendKeyEvent(true, SDL2.Scancode.ESCAPE, 0x1b, 0, 0);
			sendKeyEvent(false, SDL2.Scancode.ESCAPE, 0x1b, 0, 0);
		} 
		if(v == buttonKbd) {
			if(kbdActive) {
				edittext.setEnabled(false);
				kbdActive=false;
			} else {
				edittext.setEnabled(true);
				edittext.requestFocus();
				imm.showSoftInput(edittext, InputMethodManager.SHOW_IMPLICIT);
				kbdActive=true;
			}
		}
	}

}
