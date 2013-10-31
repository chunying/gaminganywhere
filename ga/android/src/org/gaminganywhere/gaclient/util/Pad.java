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
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;

public class Pad extends View {
	private Paint paint = new Paint();
	private int currWidth = -1;
	private int currHeight = -1;
	private int currRadius = -1;
	private int centerX = -1;
	private int centerY = -1;
	private boolean touched = false;
	private float touchedX = -1;
	private float touchedY = -1;
	//
	private int partition = 1;
	private double[] vectorX = null;
	private double[] vectorY = null;
	private PartitionEventListener partListener = null;
	//
	private boolean drawPartitionAll = true;
	private int[] drawPartitionLine = null;
	//
	private String[] drawLabelText = null;
	private int[] drawLabelBetween = null;
	private double[] drawLabelX = null;
	private double[] drawLabelY = null;
	//
	private int colorPad = Color.BLACK;
	private int colorPadLine = Color.WHITE;
	private int colorHotspot = Color.CYAN; 
	private int colorText = Color.WHITE;
	private static final double radiusRatio = 1.0;
	private static final double centerPartSizeRatio = 0.28;
	private static final float hotspotRatio = (float) 0.4;

	interface PartitionEventListener {
		public abstract void onPartitionEvent(View v, int action, int part);
	}

	public void setPartitionEventListener(PartitionEventListener listener) {
		this.partListener = listener;
	}
	
	public Pad(Context context) {
		super(context);
	}
	
	public void setDrawPartitionAll(boolean drawAll) { drawPartitionAll = drawAll; }
	public void setDrawPartitionLine(int[] draw) { drawPartitionLine = draw; }
	public void setDrawLabel(int[] drawBetween, String[] drawText) {
		if((drawBetween==null) || (drawText==null)
		|| (drawBetween.length != drawText.length*2)) {
			drawLabelBetween = null;
			drawLabelText = null;
			Log.d("ga_log", "setDrawLabel: invalid configuration.");
			return;
		}
		drawLabelBetween = drawBetween;
		drawLabelText = drawText;
		drawLabelX = null;
		drawLabelY = null;
		//Log.d("ga_log", String.format("setDrawLabel: %d label configured", drawLabelText.length));
	}
	public void setColorPad(int color) { colorPad = color; }
	public void setColorPadLine(int color) { colorPadLine = color; }
	public void setColorHotspot(int color) { colorHotspot = color; }
	public void setColorText(int color) { colorText = color; }
	public boolean setPartition(int n) {
		int i;
		double deg = 2*Math.PI / n;
		double cos = Math.cos(-deg);
		double sin = Math.sin(-deg);
		if(n < 1)
			return false;
		// rotation clockwise (unit is negative)
		partition = n;
		vectorX = new double[n+1];
		vectorY = new double[n+1];
		vectorX[0] = vectorX[n] = 0;
		vectorY[0] = vectorY[n] = currRadius;
		for(i = 1; i < n; i++) {
			vectorX[i] = vectorX[i-1]*cos - vectorY[i-1]*sin;
			vectorY[i] = vectorX[i-1]*sin + vectorY[i-1]*cos;
		}
		//
		return true;
	}
	
	public double cross(double x1, double y1, double x2, double y2) {
		return x1 * y2 - x2 * y1;
	}

	public void getMidUnitVector(double x1, double y1, double x2, double y2, double[] ret) {
		double nx, ny, len;
		if(cross(x1, y1, x2, y2) == 0) {
			// parallel, rotate 90 deg
			nx = -y1;
			ny = x1;
		} else {
			nx = (x1 + x2) / 2.0;
			ny = (y1 + y2) / 2.0;
		}
		len = Math.sqrt(nx*nx + ny*ny);
		ret[0] = (nx / len);
		ret[1] = (ny / len);
		return;
	}
	
	private void computeLabelPosition() {
		int i;
		float textHeight;
		if(drawLabelText == null || drawLabelBetween == null)
			return;
		// tune text size
		int size = 0;
		do {
			size += 2;
			paint.setTextSize(size);
		} while(paint.measureText("AAAAA") < 2 * currRadius * centerPartSizeRatio);
		textHeight = paint.ascent() + paint.descent();
		//
		//Log.d("ga_log", "PainTextSize = " + Integer.toString(size));
		drawLabelX = new double[drawLabelText.length];
		drawLabelY = new double[drawLabelText.length];
		for(i = 0; i < drawLabelText.length; i++) {
			int l0 = drawLabelBetween[i*2+0];
			int l1 = drawLabelBetween[i*2+1];
			if(l0 < 0 || l1 < 0 || l0 >= partition || l1 >= partition)
				continue;
			double[] uv = new double[2];
			getMidUnitVector(vectorX[l0],vectorY[l0], vectorX[l1], vectorY[l1], uv);
			uv[0] *= currRadius * (0.5 + 0.5 * centerPartSizeRatio);
			uv[1] *= currRadius * (0.5 + 0.5 * centerPartSizeRatio);
			drawLabelX[i] = uv[0] - paint.measureText(drawLabelText[i])/2;
			drawLabelY[i] = uv[1] + textHeight/2;
		}	
	}

	public int getPartition(float x, float y) {
		int left, mid, right;
		double vx;
		double vy;
		if(partition < 1 || vectorX==null || vectorY==null)
			return -1;
		// binary search for the area
		vx = x - centerX;
		vy = y - centerY;
		vy = -1.0 * vy;
		// click on center?
		if(vx*vx + vy*vy < centerPartSizeRatio*centerPartSizeRatio*currRadius*currRadius/4)
			return 0;
		// not center ... find the correct partition
		left = 0;
		right = partition;
		while(right-left > 1) {
			mid = (left + right) / 2;
			if(cross(vectorX[left], vectorY[left], vx, vy) <= 0
			&& cross(vectorX[mid], vectorY[mid], vx, vy) >= 0) {
				right = mid;
			} else {
				left = mid;
			}
		}
		return 1+left;
	}
	
	public boolean onTouch(MotionEvent evt) {
		int part = -1;
		int action = evt.getActionMasked();
		float x = evt.getX();
		float y = evt.getY();
		// need to handle out of range?
//		float dx = x - centerX;
//		float dy = y - centerY;
//		if(dx*dx + dy*dy > currRadius*currRadius*1.1025) {
//			touchedX = -1;
//			touchedY = -1;
//			touched = false;
//			this.postInvalidate();
//			return true;
//		}
		//
		part = getPartition(x, y);
		//
		switch(action) {
		case MotionEvent.ACTION_DOWN:
		case MotionEvent.ACTION_POINTER_DOWN:
			touchedX = x;
			touchedY = y;
			touched = true;
			this.postInvalidate();
			if(partListener != null) {
				partListener.onPartitionEvent(this, action, part);
			}
			break;
		case MotionEvent.ACTION_UP:
		case MotionEvent.ACTION_POINTER_UP:
			touchedX = -1;
			touchedY = -1;
			touched = false;
			if(partListener != null) {
				partListener.onPartitionEvent(this, action, part);
			}
			this.postInvalidate();
			break;
		case MotionEvent.ACTION_MOVE:
			touchedX = x;
			touchedY = y;
			this.postInvalidate();
			if(partListener != null) {
				partListener.onPartitionEvent(this, action, part);
			}
			break;
		}
		return true;
	}
	
	@Override
	public void onDraw(Canvas canvas) {
		int i;
		int width = this.getWidth();
		int height = this.getHeight();
		int radius = (int) ((width < height ? width : height) / 2 * radiusRatio);
		// assume width == height!
		if(width != currWidth || height != currHeight) {
			centerX = width/2;
			centerY = height/2;
			currWidth = width;
			currHeight = height;
			currRadius = radius;
			setPartition(partition);
		}
		// draw big circle
		paint.setColor(colorPad);
		canvas.drawCircle(centerX, centerY, radius, paint);
		// draw lines
		paint.setColor(colorPadLine);
		if(drawPartitionAll) {
			for(i = 0; i < partition; i++) {
				canvas.drawLine((float) centerX, (float) centerY,
						(float) vectorX[i]+centerX, (float) -vectorY[i]+centerY, paint);
			}
		} else if(drawPartitionLine != null) {
			for(i = 0; i < drawPartitionLine.length; i++) {
				if(drawPartitionLine[i] < 0 || drawPartitionLine[i] >= partition)
					continue;
				canvas.drawLine((float) centerX, (float) centerY,
						(float) vectorX[drawPartitionLine[i]]+centerX,
						(float) -vectorY[drawPartitionLine[i]]+centerY, paint);
			}
		}
		// draw label
		paint.setColor(colorText);
		if(drawLabelBetween != null && drawLabelText != null) {
			if(drawLabelX == null)
				computeLabelPosition();
			for(i = 0; i < drawLabelText.length; i++) {
				canvas.drawText(drawLabelText[i],
						centerX + (float) drawLabelX[i],
						centerY - (float) drawLabelY[i],
						paint);
			}
		}
		// draw center small circle
		paint.setColor(colorPadLine);
		canvas.drawCircle(centerX, centerY, (int) ((centerPartSizeRatio+0.01)*radius), paint);
		paint.setColor(colorPad);
		canvas.drawCircle(centerX, centerY, (int) ((centerPartSizeRatio-0.01)*radius), paint);
		//
		if(touched) {
			paint.setColor(colorHotspot);
			canvas.drawCircle(touchedX, touchedY, radius*hotspotRatio, paint);
		}
	}
}
