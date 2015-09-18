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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.gaminganywhere.gaclient.util.GAControllerBasic;
import org.gaminganywhere.gaclient.util.GAControllerDualPad;
import org.gaminganywhere.gaclient.util.GAControllerEmpty;
import org.gaminganywhere.gaclient.util.GAControllerLimbo;
import org.gaminganywhere.gaclient.util.GAControllerN64;
import org.gaminganywhere.gaclient.util.GAControllerPSP;
import org.gaminganywhere.gaclient.util.GAControllerPadABXY;
import org.gaminganywhere.gaclient.util.GAControllerNDS;

import android.os.Bundle;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.text.InputType;
import android.view.DragEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.SimpleAdapter;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity implements
	OnClickListener, OnItemSelectedListener, OnCheckedChangeListener, OnSeekBarChangeListener
{

	// for handling profile
	class Profile {
		String title, desc;
		
		public Profile(String title, String desc) {
			this.title = title;
			this.desc = desc;
			return;
		}
	}
	
	class ProfileAdapter extends ArrayAdapter<Profile> {

		public ProfileAdapter(Context context, int textViewResourceId,
				List<Profile> objects) {
			super(context, textViewResourceId, objects);
			return;
		}

		@Override
		public View getDropDownView(int position, View convertView,
				ViewGroup parent) {
			//return super.getDropDownView(position, convertView, parent);
			return initView(position, convertView);
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) {
			//return super.getView(position, convertView, parent);
			return initView(position, convertView);
		}

		View initView(int position, View convertView) {
			if(convertView == null) {
				convertView = View.inflate(getContext(),
						android.R.layout.simple_list_item_2, null);
			}
			TextView tv1 = (TextView) convertView.findViewById(android.R.id.text1);
			TextView tv2 = (TextView) convertView.findViewById(android.R.id.text2);
			//tv1.setTextColor(Color.BLACK);
			tv1.setText(getItem(position).title);
			//tv2.setTextColor(Color.LTGRAY);
			tv2.setText(getItem(position).desc);
			return convertView;
		}
	}
	
	private List<Profile> profile_list = null;
	private Spinner spinner_profile = null;
	private Button btn_connect = null;
	private CheckBox cb_builtin_audio = null;
	private CheckBox cb_builtin_video = null;
	private CheckBox cb_portrait_mode = null;
	private CheckBox cb_drop_late_vframe = null;
	private SeekBar sb_drop_late_vframe = null;
	private CheckBox cb_watchdog_timeout = null;
	private SeekBar sb_watchdog_timeout = null;
	private int [] watchdog_timeouts = {3, 5, 10, 20, 40, 60};
	private Spinner spinner_control = null;
	private ArrayList<HashMap<String,String>> list_control = new ArrayList<HashMap<String,String>>();
	private Button btn_padsetup = null;
	private int lastSelection = -1;
	
	private void showToast(String s) {
		Context context = getApplicationContext();
		Toast t = Toast.makeText(context, s, Toast.LENGTH_SHORT);
		t.show();
	}
	
	private void profileLoad() {
		ArrayList<HashMap<String,String>> list = GAConfigHelper.profileLoadSummary(this);
		if(profile_list == null)
			profile_list = new ArrayList<MainActivity.Profile>();
		profile_list.clear();
		if(list != null) {
			for(int i = 0; i < list.size(); i++) {
				HashMap<String,String> h = list.get(i);
				profile_list.add(new Profile(h.get("name"), h.get("description")));
			}
		}
		spinner_profile.setAdapter(new ProfileAdapter(this, 0, profile_list));
	}
	
	private void profileRename() {
		final EditText editor = new EditText(this);
		Profile p = (Profile) spinner_profile.getSelectedItem();
		if (p == null) {
			showToast("Please choose a profile");
			return;
		}
		editor.setInputType(InputType.TYPE_CLASS_TEXT);
		editor.setText(p.title);
		AlertDialog.Builder dlg = new AlertDialog.Builder(this);
		dlg.setCancelable(false);
		dlg.setTitle("Confirm");
		dlg.setMessage("Enter new name for '" + p.title + "'");
		dlg.setView(editor);
		dlg.setPositiveButton("OK", new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int which) {
				String newName = editor.getText().toString().trim();
				Profile p = (Profile) spinner_profile.getSelectedItem();
				// name is empty or the same?
				if(newName.equals(""))
					return;
				if(newName.equals(p.title))
					return;
				//
				if(GAConfigHelper.profileRename(getApplicationContext(), p.title, newName)) {
					showToast("Profile renamed");
					profileLoad();
				} else {
					showToast("Profile rename failed");
				}
			}
		});	
		dlg.setNegativeButton("Cancel", null);
		dlg.show();		
	}
	
	private void profileDelete() {
		Profile p = (Profile) spinner_profile.getSelectedItem();
		//
		if(p == null) {
			showToast("Please choose a profile");
			return;
		}
		//
		AlertDialog.Builder dlg = new AlertDialog.Builder(this);
		dlg.setCancelable(false);
		dlg.setTitle("Confirm");
		dlg.setMessage("Delete profile '" + p.title + "'");
		dlg.setPositiveButton("OK",  new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int which) {
				Profile p = (Profile) spinner_profile.getSelectedItem();
				if(GAConfigHelper.profileDelete(getApplicationContext(), p.title)) {
					showToast("Profile '" + p.title + "' deleted");
				} else {
					showToast("Profile '" + p.title + "' delete failed");
				}
				profileLoad();
			}
		});
		dlg.setNegativeButton("Cancel", null);
		dlg.show();
		dlg = null;
	}
	
	private void profileConnect() {
		Intent intent = null;
		Profile p = (Profile) spinner_profile.getSelectedItem();
		HashMap<String,String> ctrl = (HashMap<String,String>) spinner_control.getSelectedItem();
		if(p == null) {
			showToast("Please choose a profile");
			return;
		}
		//
		intent = new Intent(
				MainActivity.this, PlayerActivity.class);
		intent.putExtra("profile", p.title);
		intent.putExtra("builtinAudio", cb_builtin_audio.isChecked());
		intent.putExtra("builtinVideo", cb_builtin_video.isChecked());
		intent.putExtra("portraitMode", cb_portrait_mode.isChecked());
		intent.putExtra("controller", ctrl.get("name"));
		intent.putExtra("dropLateVFrame", getDropLateVFrameDelay());
		intent.putExtra("watchdogTimeout", getWatchdogTimeout());
		startActivityForResult(intent, 0);
	}
	
	private void profileEdit(boolean createNew) {
		Intent intent = null;
		Profile p = (Profile) spinner_profile.getSelectedItem();
		//
		if(p == null && createNew == false) {
			showToast("Please choose a profile");
			return;
		}
		//
		intent = new Intent(
				MainActivity.this, SettingsActivity.class);
		//
		if(p != null && createNew == false)
			intent.putExtra("profile", p.title);
		//
		startActivity(intent);
	}
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		// button
		btn_connect = (Button) findViewById(R.id.button_connect);
		btn_connect.setOnClickListener(this);
		// init spinner & profile
		spinner_profile = (Spinner) findViewById(R.id.spinner_profile);
		//
		cb_builtin_audio = (CheckBox) findViewById(R.id.cb_builtin_audio);
		cb_builtin_video = (CheckBox) findViewById(R.id.cb_builtin_video);
		cb_portrait_mode = (CheckBox) findViewById(R.id.cb_portrait_mode);
		cb_drop_late_vframe = (CheckBox) findViewById(R.id.cb_drop_late_vframe);
		sb_drop_late_vframe = (SeekBar) findViewById(R.id.sb_drop_late_frame);
		cb_watchdog_timeout = (CheckBox) findViewById(R.id.cb_watchdog_timeout);
		sb_watchdog_timeout = (SeekBar) findViewById(R.id.sb_watchdog_timeout);
		//
		cb_builtin_audio.setChecked(true);
		cb_builtin_video.setChecked(true);
		cb_portrait_mode.setChecked(false);
		//
		cb_drop_late_vframe.setChecked(false);
		cb_drop_late_vframe.setOnCheckedChangeListener(this);
		sb_drop_late_vframe.setEnabled(false);
		sb_drop_late_vframe.setMax(1500);
		sb_drop_late_vframe.setOnSeekBarChangeListener(this);
		sb_drop_late_vframe.setProgress(100);
		//
		cb_watchdog_timeout.setChecked(true);
		cb_watchdog_timeout.setOnCheckedChangeListener(this);
		sb_watchdog_timeout.setEnabled(true);
		sb_watchdog_timeout.setMax(watchdog_timeouts.length-1);
		sb_watchdog_timeout.setOnSeekBarChangeListener(this);
		sb_watchdog_timeout.setProgress(watchdog_timeouts.length-1);
		sb_watchdog_timeout.setProgress(0);
		//cb_watchdog_timeout.setText(String.format("Watchdog timeout: %s", watchdog_timeouts[0]));
		//
		do {
			HashMap<String,String> item = null;
			SimpleAdapter adapter = null;
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerBasic.getName());
			item.put("desc", GAControllerBasic.getDescription());
			list_control.add(item);
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerEmpty.getName());
			item.put("desc", GAControllerEmpty.getDescription());
			list_control.add(item);
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerDualPad.getName());
			item.put("desc", GAControllerDualPad.getDescription());
			list_control.add(item);
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerLimbo.getName());
			item.put("desc", GAControllerLimbo.getDescription());
			list_control.add(item);
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerN64.getName());
			item.put("desc", GAControllerN64.getDescription());
			list_control.add(item);
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerNDS.getName());
			item.put("desc", GAControllerNDS.getDescription());
			list_control.add(item);
			//
			item = new HashMap<String,String>();
			item.put("name", GAControllerPSP.getName());
			item.put("desc", GAControllerPSP.getDescription());
			list_control.add(item);
			//
//			item = new HashMap<String,String>();
//			item.put("name", GAControllerPadABXY.getName());
//			item.put("desc", GAControllerPadABXY.getDescription());
//			list_control.add(item);
			//
			adapter = new SimpleAdapter(this, list_control,
					android.R.layout.simple_list_item_2,
					new String[] {"name", "desc"},
					new int[] {android.R.id.text1, android.R.id.text2});
			spinner_control = (Spinner) findViewById(R.id.spinner_control);
			spinner_control.setAdapter(adapter);
		} while(false);
		btn_padsetup = (Button) findViewById(R.id.button_padsetup);
		btn_padsetup.setEnabled(false);
		//
		profileLoad();
	}

	public int getWatchdogTimeout() {
		if(cb_watchdog_timeout.isChecked() == false)
			return -1;
		return watchdog_timeouts[sb_watchdog_timeout.getProgress()];
	}
	
	public int getDropLateVFrameDelay() {
		if(cb_drop_late_vframe.isChecked() == false)
			return -1;
		return sb_drop_late_vframe.getProgress();
	}
	
	@Override
	protected void onResume() {
		profileLoad();
		if(lastSelection >= 0 && lastSelection < spinner_profile.getCount()) {
			spinner_profile.setSelection(lastSelection);
		} else {
			lastSelection = -1;
		}
		super.onResume();
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		// XXX: we have only one intent now (PlayerActivity), not checking requestCode
		showToast("Connection terminated");
		super.onActivityResult(requestCode, resultCode, data);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		lastSelection = spinner_profile.getSelectedItemPosition();
		switch(item.getItemId()) {
		case R.id.action_connect:
			profileConnect();
			break;
		case R.id.action_new:
			profileEdit(true);
			break;
		case R.id.action_edit:
			profileEdit(false);
			break;
		case R.id.action_rename:
			profileRename();
			break;
		case R.id.action_delete:
			profileDelete();
			break;
		}
		return super.onOptionsItemSelected(item);
	}

	@Override	// OnClickListener
	public void onClick(View v) {
		if(v.getId() == R.id.button_connect) {
			lastSelection = spinner_profile.getSelectedItemPosition();
			profileConnect();
		}
	}

	@Override	// OnItemSelectedListener
	public void onItemSelected(AdapterView<?> parent, View view, int pos,
			long id) {
		if(parent.getId() == R.id.spinner_profile) {
			Profile p = (Profile) parent.getItemAtPosition(pos);
			showToast("Profile '" + p.title + "' selected");
		}
	}

	@Override	// OnItemSelectedListener
	public void onNothingSelected(AdapterView<?> view) {
		// do nothing
		showToast("Please choose a profile");
	}

	@Override
	public void onCheckedChanged(CompoundButton v, boolean isChecked) {
		if(v == cb_watchdog_timeout) {
			if(isChecked) {
				cb_watchdog_timeout.setText(String.format("Watchdog timeout: %ss", getWatchdogTimeout()));
			} else {
				cb_watchdog_timeout.setText("Watchdog timeout: Disabled");
			}
			sb_watchdog_timeout.setEnabled(isChecked);
		} else if(v == cb_drop_late_vframe) {
			if(isChecked) {
				cb_drop_late_vframe.setText(String.format("Drop late video frame: %sms",
						sb_drop_late_vframe.getProgress()));
			} else {
				cb_drop_late_vframe.setText("Drop late video frame: Disabled");
			}
			sb_drop_late_vframe.setEnabled(isChecked);
		}
	}

	@Override
	public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
		if(sb == sb_watchdog_timeout) {
			int to = getWatchdogTimeout();
			if(to > 0) {
				cb_watchdog_timeout.setText(String.format("Watchdog timeout: %ss", to));
			} else {
				cb_watchdog_timeout.setText("Watchdog timeout: Disabled");
			}
		} else if(sb == sb_drop_late_vframe) {
			int to = getDropLateVFrameDelay();
			if(to > 0) {
				cb_drop_late_vframe.setText(String.format("Drop late video frame: %sms", to));
			} else {
				cb_drop_late_vframe.setText(String.format("Drop late video frame: Disabled", to));
			}
		}
		return;
	}

	@Override
	public void onStartTrackingTouch(SeekBar sb) {
		// TODO Auto-generated method stub
	}

	@Override
	public void onStopTrackingTouch(SeekBar sb) {
		// TODO Auto-generated method stub
	}
}
