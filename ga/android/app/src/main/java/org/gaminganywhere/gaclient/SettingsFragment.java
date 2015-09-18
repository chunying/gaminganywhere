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

import java.util.HashMap;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.CheckBoxPreference;
import android.preference.EditTextPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.text.InputType;
import android.widget.Toast;

public class SettingsFragment extends PreferenceFragment implements OnSharedPreferenceChangeListener {

	private String current_profile = null;
	private Context context = null;
	private boolean modified = false;
	private boolean isUpdate = false;
	
	private void showToast(String s) {
		Toast t = Toast.makeText(context, s, Toast.LENGTH_SHORT);
		t.show();
	}
	
	public void setProfile(String s) {
		current_profile = s;
	}
	
	public void setContext(Context ctx) {
		context = ctx;
	}

	public boolean isModified() {
		return modified;
	}

	private void profileConfigListPreference(String name, String summary, Object value) {
		ListPreference p = null;
		if((p = (ListPreference) findPreference(name)) != null) {
			p.setSummary(summary);
			p.setValue(value.toString());
		}
	}
	
	private void profileConfigEditTextPreference(String name, String summary, Object value, int type) {
		EditTextPreference p = null;
		if((p = (EditTextPreference) findPreference(name)) != null) {
			p.setSummary(summary);
			p.setText(value.toString());
			if(type != 0)
				p.getEditText().setInputType(type);
		}
	}
	
	private void profileConfigCheckBoxPreference(String name, boolean checked) {
		CheckBoxPreference p = null;
		if((p = (CheckBoxPreference) findPreference(name)) != null) {
			p.setChecked(checked);
		}
	}
	
	public void profileLoadDefault() {
		//
		profileConfigEditTextPreference("pref_title", "", "", InputType.TYPE_CLASS_TEXT);
		//
		profileConfigListPreference("pref_protocol", "rtsp", "rtsp");
		profileConfigEditTextPreference("pref_host", "", "",
				InputType.TYPE_TEXT_VARIATION_URI);
		profileConfigEditTextPreference("pref_port", "8554", "8554",
				InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
		profileConfigEditTextPreference("pref_object", "/desktop", "/desktop",
				InputType.TYPE_TEXT_VARIATION_URI);
		profileConfigCheckBoxPreference("pref_rtpovertcp", false);
		//
		profileConfigCheckBoxPreference("pref_ctrlenable", true);
		profileConfigListPreference("pref_ctrlprotocol", "udp", "udp");
		profileConfigEditTextPreference("pref_ctrlport", "8555", "8555",
				InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
		profileConfigCheckBoxPreference("pref_ctrlrelative", false);
		//
		profileConfigListPreference("pref_audio_channels", "2", "2");
		profileConfigListPreference("pref_audio_samplerate", "44100", "44100");
		//
		isUpdate = false;
		//showToast("Default value loaded");
	}

	public boolean profileLoad(String key) {
		HashMap<String,String> cfg = null;
		//
		EditTextPreference p = null;
		if((p = (EditTextPreference) findPreference("pref_title")) != null) {
			p.getEditText().setEnabled(false);
		}
		//
		if((cfg = GAConfigHelper.profileLoad(context, key)) == null) {
			showToast("Load profile '" + key + "' failed");
			return false;
		}
		//
		profileConfigEditTextPreference("pref_title", key, key, InputType.TYPE_CLASS_TEXT);
		//
		profileConfigListPreference("pref_protocol", cfg.get("protocol"), cfg.get("protocol"));
		profileConfigEditTextPreference("pref_host", cfg.get("host"), cfg.get("host"),
				InputType.TYPE_TEXT_VARIATION_URI);
		profileConfigEditTextPreference("pref_port", cfg.get("port"), cfg.get("port"),
				InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
		profileConfigEditTextPreference("pref_object", cfg.get("object"), cfg.get("object"),
				InputType.TYPE_TEXT_VARIATION_URI);
		profileConfigCheckBoxPreference("pref_rtpovertcp", Integer.parseInt(cfg.get("rtpovertcp")) != 0);
		//
		profileConfigCheckBoxPreference("pref_ctrlenable", Integer.parseInt(cfg.get("ctrlenable")) != 0);
		profileConfigListPreference("pref_ctrlprotocol", cfg.get("ctrlprotocol"), cfg.get("ctrlprotocol"));
		profileConfigEditTextPreference("pref_ctrlport", cfg.get("ctrlport"), cfg.get("ctrlport"),
				InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL);
		profileConfigCheckBoxPreference("pref_ctrlrelative", Integer.parseInt(cfg.get("ctrlrelative")) != 0);
		//
		profileConfigListPreference("pref_audio_channels", cfg.get("audio_channels"), cfg.get("audio_channels"));
		profileConfigListPreference("pref_audio_samplerate", cfg.get("audio_samplerate"), cfg.get("audio_samplerate"));
		//
		isUpdate = true;
		return true;
	}

	public boolean profileSave() {
		SharedPreferences spref = PreferenceManager.getDefaultSharedPreferences(context);
		HashMap<String,String> config = new HashMap<String,String>();
		String key = spref.getString("pref_title", "").trim();
		//
		if(key.equals("")) {
			showToast("Profile name cannot be null");
			return false;
		}
		if(spref.getString("pref_host", "").equals("")) {
			showToast("Server host cannot be null");
			return false;
		}
		//
		config.clear();
		config.put("name", spref.getString("pref_title", ""));
		config.put("protocol", spref.getString("pref_protocol", "rtsp"));
		config.put("host", spref.getString("pref_host", ""));
		config.put("port", spref.getString("pref_port", "8554"));
		config.put("object", spref.getString("pref_object", "/desktop"));
		config.put("rtpovertcp", spref.getBoolean("pref_rtpovertcp", false) ? "1" : "0");
		config.put("ctrlenable", spref.getBoolean("pref_ctrlenable", true) ? "1" : "0");
		config.put("ctrlprotocol", spref.getString("pref_ctrlprotocol", "udp"));
		config.put("ctrlport", spref.getString("pref_ctrlport", "8555"));
		config.put("ctrlrelative", spref.getBoolean("pref_ctrlrelative", false) ? "1" : "0");
		config.put("audio_channels", spref.getString("pref_audio_channels", "2"));
		config.put("audio_samplerate", spref.getString("pref_audio_samplerate", "44100"));
		//
		if(GAConfigHelper.profileSave(context, key, config, isUpdate) == false)
			return false;
		return true;
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		SharedPreferences spref = PreferenceManager.getDefaultSharedPreferences(context); 
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.preference_profile);
		//
		if(current_profile == null)
			profileLoadDefault();
		else
			profileLoad(current_profile);
		//
		spref.registerOnSharedPreferenceChangeListener(this);
	}

	@Override
	public void onSharedPreferenceChanged(SharedPreferences spref,
			String key) {
		Preference p = null;
		if((p = findPreference(key)) == null)
			return;
		// trim?
		if(p.getClass() == EditTextPreference.class) {
			String olds = spref.getString(key, "");
			String news = olds.trim();
			if(olds.equals(news) == false) {
					profileConfigEditTextPreference(key, news, news, 0);
			}
		}
		//
		if(key.equals("pref_object")) {
			String s = spref.getString(key, "").trim();
			if(s.charAt(0) != '/')
				profileConfigEditTextPreference("pref_object", "/" + s, "/" + s,
						InputType.TYPE_TEXT_VARIATION_URI);
		}
		if(p.getClass() != CheckBoxPreference.class) {
			p.setSummary(spref.getString(key, "<error>"));
		}
		modified = true;
	}
	
}
