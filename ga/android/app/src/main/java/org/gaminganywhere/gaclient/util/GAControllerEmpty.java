/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
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
import android.content.Context;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;

public class GAControllerEmpty extends GAController implements
	OnClickListener, PartitionEventListener
{
	public GAControllerEmpty(Context context) {
		super(context);
		setMouseVisibility(false);
	}
	
	public static String getName() {
		return "Empty";
	}
	
	public static String getDescription() {
		return "No control";
	}

	@Override
	public void onDimensionChange(int width, int height) {
		// must be called first!
		super.onDimensionChange(width,  height);
	}

	@Override
	public boolean onTouch(View v, MotionEvent evt) {
		// must be called last
		return super.onTouch(v, evt);
	}

	@Override
	public void onPartitionEvent(View v, int action, int part) {
		// do nothing
	}

	@Override
	public void onClick(View v) {
		// do nothing
	}
}
