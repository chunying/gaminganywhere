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

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.widget.ListAdapter;
import android.widget.Toast;

public class SettingsActivity extends PreferenceActivity {

	private SettingsFragment fragment = null;
	private String currProfile = null;
	
	public void showToast(String s) {
		Toast t = Toast.makeText(getBaseContext(), s, Toast.LENGTH_LONG);
		t.show();
	}
	
	public void goBack() {
		super.onBackPressed();
	}
	
	public void doResume() {
		super.onResume();
	}
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		Intent intent = getIntent();
		currProfile = intent.getStringExtra("profile");
		//SettingsFragment fragment = null;
		super.onCreate(savedInstanceState);
		// pass profile key to SettingsFragment
		if(fragment == null)
			fragment = new SettingsFragment();
		fragment.setContext(getBaseContext());
		fragment.setProfile(currProfile);
		// we don't have a preference header
		getFragmentManager().beginTransaction()
			.replace(android.R.id.content, fragment).commit();
	}

	private int findPreferenceOrder(PreferenceScreen parent, int base, String key) {
		int i, ret;
		ListAdapter ada = parent.getRootAdapter();
		for (i = 0; i < ada.getCount(); i++) {
			String prefkey = ((Preference)ada.getItem(i)).getKey();
			if ((prefkey != null) && (prefkey.equals(key)))
				return base + i;
			if (ada.getItem(i).getClass().equals(PreferenceScreen.class)) {
				ret = findPreferenceOrder((PreferenceScreen) ada.getItem(i), base + i, key);
				if(ret >= 0)
					return ret;
			}
		}
		return -1;
	}
	
	@Override
	public void onBackPressed() {
		// TODO Auto-generated method stub
		AlertDialog.Builder dlg = null;
		SharedPreferences spref = PreferenceManager.getDefaultSharedPreferences(getBaseContext());
		//
		if(fragment.isModified() == false) {
			super.onBackPressed();
			return;
		}
		if(spref.getString("pref_host", "").equals("")) {
			showToast("No server host provided");
			PreferenceScreen ps = (PreferenceScreen) fragment.findPreference("pref_settings");
			ps.onItemClick(null,  null, findPreferenceOrder(ps, 0, "pref_host"), 0);
			super.onResume();
			return;
		}
		if(spref.getString("pref_title", "").equals("")) {
			showToast("No profile name provided");
			PreferenceScreen ps = (PreferenceScreen) fragment.findPreference("pref_settings");
			ps.onItemClick(null,  null, findPreferenceOrder(ps, 0, "pref_title"), 0);
			super.onResume();
			return;
		}
		// confirm save?
		dlg = new AlertDialog.Builder(this);
		dlg.setCancelable(false);
		dlg.setTitle("Confirm");
		dlg.setMessage("Save changes?");
		dlg.setPositiveButton("Yes",  new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int which) {
				String okmsg = null, failmsg = null;
				if(currProfile == null) {
					okmsg = "Profile created";
					failmsg = "Profile creation failed";
				} else {
					okmsg = "Profile '" + currProfile + "' updated";
					failmsg = "Profile '" + currProfile + "' update failed";
				}
				if(fragment.profileSave())
					showToast(okmsg);
				else
					showToast(failmsg);
				goBack();
			}
		});
		dlg.setNegativeButton("No", new DialogInterface.OnClickListener() {			
			@Override
			public void onClick(DialogInterface dialog, int which) {
				showToast("Profile '" + currProfile + "' untouched");
				goBack();
			}
		});
		dlg.setNeutralButton("Cancel", new DialogInterface.OnClickListener() {
			@Override
			public void onClick(DialogInterface dialog, int which) {
				doResume();
			}
		});
		dlg.show();
		//// XXX: disable Back
		//super.onBackPressed();
		super.onResume();
	}

	@Override
	protected void onPause() {
		super.onPause();
		PreferenceManager.getDefaultSharedPreferences(getBaseContext())
			.registerOnSharedPreferenceChangeListener(fragment);
	}

	@Override
	protected void onResume() {
		super.onResume();
		PreferenceManager.getDefaultSharedPreferences(getBaseContext())
			.registerOnSharedPreferenceChangeListener(fragment);
	}

}
