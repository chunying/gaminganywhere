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

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteDatabase.CursorFactory;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

public class GAConfigHelper extends SQLiteOpenHelper {

	public static final int dbVersion = 1;
	public static final String dbName = "gaconfig.sqlite";
	
	public static final String fields[] = new String[] {"name",
		/* 1*/"protocol", "host", "port", "object", "rtpovertcp",
		/* 6*/"ctrlenable", "ctrlprotocol", "ctrlport", "ctrlrelative",
		/*10*/"audio_channels", "audio_samplerate"};

	public static ArrayList<HashMap<String,String>> profileLoadSummary(Context context) {
		GAConfigHelper helper = null;
		SQLiteDatabase db = null;
		Cursor c = null;
		ArrayList<HashMap<String,String>> profiles = null;
		try {
			helper = new GAConfigHelper(context, null, null, 0);
			db = helper.getReadableDatabase();
			c = db.query("profile", fields,
					null, null, null, null, /*orderby*/"name");
			//
			profiles = new ArrayList<HashMap<String,String>>();
			profiles.clear();
			//
			if(c.moveToFirst()) do {
				HashMap<String,String> h = new HashMap<String,String>();
				String title = c.getString(0);
				String desc = c.getString(1) + "://"
							+ c.getString(2)
							+ ":" + c.getString(3)
							+ c.getString(4);
				h.put("name", title);
				h.put("description", desc != null ? desc : "");
				profiles.add(h);
			} while(c.moveToNext());
		}
		catch(Exception e) {}
		if(c != null)		c.close();
		if(db != null)		db.close();
		if(helper != null)	helper.close();
		//
		return profiles;
	}
	
	public static HashMap<String,String> profileLoad(Context context, String key) {
		GAConfigHelper helper = null;
		SQLiteDatabase db = null;
		Cursor c = null;
		HashMap<String, String> map = null;
		//
		if (context == null) {
			Log.e("GAConfigHelper.profileLoad", "No context provided");
			return null;
		}
		//
		helper = new GAConfigHelper(context, null, null, 0);
		db = helper.getReadableDatabase();
		c = db.query("profile", GAConfigHelper.fields, "name = '" + key + "'",
				null, null, null, null);
		//
		if (c.moveToFirst() == false) {
			c.close();
			db.close();
			helper.close();
			return null;
		}
		//
		map = new HashMap<String,String>();
		map.clear();
		map.put("name", c.getString(0));
		map.put("protocol", c.getString(1));
		map.put("host", c.getString(2));
		map.put("port", c.getString(3));
		map.put("object", c.getString(4));
		map.put("rtpovertcp", c.getString(5));
		map.put("ctrlenable", c.getString(6));
		map.put("ctrlprotocol", c.getString(7));
		map.put("ctrlport", c.getString(8));
		map.put("ctrlrelative", c.getString(9));
		map.put("audio_channels", c.getString(10));
		map.put("audio_samplerate", c.getString(11));
		//
		c.close();
		db.close();
		helper.close();
		//
		return map;
	}
	
	public static boolean profileSave(Context context, String key, HashMap<String,String> config, boolean isUpdate) {
		boolean ret = false;
		GAConfigHelper helper = null;
		SQLiteDatabase db = null;
		do { try {
			helper = new GAConfigHelper(context, null, null, 0);
			db = helper.getWritableDatabase();
			if(key.trim().equals(""))
				break;
			if(config.get("host").trim().equals(""))
				break;
			if(isUpdate == false) {
				// check existing?
				Cursor c = db.query("profile", null,
							"name = '" + key + "'",
							null, null, null, null);
				if(c.moveToFirst() != false) {
					break;
				}
				c.close();
				// create new
				db.execSQL("INSERT INTO profile ("
						+ "name, protocol, host, port, object, rtpovertcp, "
						+ "ctrlenable, ctrlprotocol, ctrlport, ctrlrelative, "
						+ "audio_channels, audio_samplerate) "
						+ "VALUES ("
						+ "'" + config.get("name") + "', "
						+ "'" + config.get("protocol") + "', "
						+ "'" + config.get("host") + "', "
						+ 		config.get("port") + ", "
						+ "'" + config.get("object") + "', "
						+		config.get("rtpovertcp") + ", "
						+		config.get("ctrlenable") + ", "
						+ "'" + config.get("ctrlprotocol") + "', "
						+ 		config.get("ctrlport") + ", "
						+		config.get("ctrlrelative") + ", "
						+ 		config.get("audio_channels") + ", "
						+ 		config.get("audio_samplerate")
						+ ")");
			} else {
				// update existing
				db.execSQL("UPDATE profile SET "
						// server
						+ "protocol = '" + config.get("protocol") + "',"
						+ "host = '"+ config.get("host") + "',"
						+ "port = "+ config.get("port") + ","
						+ "object = '"+ config.get("object") + "',"
						+ "rtpovertcp = "+ config.get("rtpovertcp") + ","
						// controller
						+ "ctrlenable = "+ config.get("ctrlenable") + ","
						+ "ctrlprotocol = '"+ config.get("ctrlprotocol") + "',"
						+ "ctrlport = "+ config.get("ctrlport") + ","
						+ "ctrlrelative = "+ config.get("ctrlrelative") + ","
						// decoders
						+ "audio_channels = "+ config.get("audio_channels") + ","
						+ "audio_samplerate = "+ config.get("audio_samplerate") + " "
						//
						+ "WHERE name = '" + config.get("name") + "'");
			}
			ret = true;
		}
		catch(Exception e) {
			e.printStackTrace();
		} } while(false);
		//
		if(db != null)
			db.close();
		if(helper != null)
			helper.close();
		//
		return ret;
	}
	
	public static boolean profileDelete(Context context, String key) {
		GAConfigHelper helper = null;
		SQLiteDatabase db = null;
		boolean ret = false;
		//
		try {// load profiles
			helper = new GAConfigHelper(context, null, null, 0);
			db = helper.getReadableDatabase();
			db.execSQL("DELETE FROM profile WHERE name = '" + key + "'");
			ret = true;
		}
		catch(Exception e) {
			ret = false;
		}
		if(db != null)		db.close();
		if(helper != null)	helper.close();
		return ret;
	}
	
	public static boolean profileRename(Context context, String key, String newName) {
		GAConfigHelper helper = null;
		SQLiteDatabase db = null;
		Cursor c = null;
		boolean ret = false;
		// name is empty or the same?
		if(newName.trim().equals(""))
			return false;
		if(newName.equals(key))
			return false;
		do { try {
			helper = new GAConfigHelper(context, null, null, 0);
			db = helper.getReadableDatabase();
			// name existed?
			c = db.query("profile", null,
					"name = '" + newName + "'", null, null, null, null);
			if(c.getCount() > 0) {
				break;
			}
			//
			db.execSQL("UPDATE profile SET name = '" + newName + "' WHERE name = '" + key + "'");
			ret = true;
		} catch(Exception e) {} } while(false);
		if(c != null)		c.close();
		if(db != null)		db.close();
		if(helper != null)	helper.close();
		return ret;
	}
	
	public GAConfigHelper(Context context, String name, CursorFactory factory,
			int version) {
		super(context, GAConfigHelper.dbName, factory, GAConfigHelper.dbVersion);
	}

	@Override
	public void onCreate(SQLiteDatabase db) {
		// 
		String create_table_profile =
				"CREATE TABLE IF NOT EXISTS profile ("
				+ "name VARCHAR PRIMARY KEY NOT NULL,"
				// server
				+ "protocol VARCHAR NOT NULL,"
				+ "host VARCHAR NOT NULL,"
				+ "port INT,"
				+ "object VARCHAR NOT NULL,"
				+ "rtpovertcp BOOLEAN,"				
				// controller
				+ "ctrlenable BOOLEAN,"
				+ "ctrlprotocol VARCHAR NOT NULL,"
				+ "ctrlport INT,"
				+ "ctrlrelative BOOLEAN,"
				// decoders
				+ "audio_channels INT,"
				+ "audio_samplerate INT"
				+ ")";
		db.execSQL(create_table_profile);
	}

	@Override
	public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
		if(newVersion > oldVersion) {
			// XXX: this is the initial version
		} else {
			onCreate(db);
		}
	}

}
