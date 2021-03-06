package com.bubblebot;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Date;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;

public class JSONUtils {
	//Prevent instantiations
	private JSONUtils(){}
	
/*
	public static void simplifyOutput(String photoName, String outPath) {
		try {
			BufferedWriter out = new BufferedWriter(new FileWriter(outPath));
			out.write(generateSimplifiedJSON(photoName).toString());
			out.close();
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			Log.i("mScan", "JSON excetion in JSONUtils.");
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			Log.i("mScan", "IO excetion in JSONUtils.");
		}
	}
	
	public static JSONObject putFieldValue(JSONObject formRoot, String fieldLabel, String value) throws JSONException{
		int segIdx = 0;
		JSONArray fields = formRoot.getJSONArray("fields");
		int fieldsLength = fields.length();
		for(int i = 0; i < fieldsLength; i++){
			JSONObject field = fields.getJSONObject(i);
			if(field.getString("label").equals(value)){
				JSONArray segments = field.getJSONArray("segments");
				JSONObject segment = segments.getJSONObject(segIdx);
				segment.put("value", value);
				segments.put(segIdx, segment);
				field.put("segments", segments);
				fields.put(i, field);
				formRoot.put("fields", fields);
				return formRoot;
			}
		}
		segment.put("value", value);
		segments.put(segIdx, segment);
		field.put("segments", segments);
		fields.put(i, field);
		formRoot.put("fields", fields);
		return formRoot;
	}*/
	public static JSONObject generateSimplifiedJSON(String photoName) throws JSONException, IOException {
		JSONObject outRoot = new JSONObject();
		JSONObject bubbleVals = parseFileToJSONObject(MScanUtils.getJsonPath(photoName));
		JSONArray fields = bubbleVals.getJSONArray("fields");
		
		outRoot.put("form_name", bubbleVals.getString("template_path"));
		
		outRoot.put("location", "location_name");
		
		outRoot.put("data_capture_date",
				new Date(new File(MScanUtils.getPhotoPath(photoName)).lastModified()).toString());
		
		JSONObject outFields = new JSONObject();
		for(int i = 0; i < fields.length(); i++){
			outFields.put(fields.getJSONObject(i).getString("label"),
						  MScanUtils.sum(getSegmentValues(fields.getJSONObject(i))));
		}
		outRoot.put("fields", outFields);
		return outRoot;
	}
	public static String generateSimplifiedJSONString(String photoName) {
		try {
			String outString = generateSimplifiedJSON(photoName).toString();
			//Here I use a regular expression to add backslashes before characters that need to be escaped,
			//to be uploaded to the fusion table:
			outString = outString.replaceAll("[\\[\\]\\(\\)\\\\]", "\\\\$0");
			return outString;
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			Log.i("mScan", "JSON excetion in JSONUtils.");
			return "";
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			Log.i("mScan", "IO excetion in JSONUtils.");
			return "";
		}
	}
	public static void writeJSONObjectToFile(JSONObject obj, String outPath) throws JSONException, IOException {
		BufferedWriter out = new BufferedWriter(new FileWriter(outPath));
		out.write(obj.toString(4));
		out.close();
	}
	public static JSONObject parseFileToJSONObject(String bvFilename) throws JSONException, IOException {
		File jsonFile = new File(bvFilename);

		//Read text from file
		StringBuilder text = new StringBuilder();
		BufferedReader br = new BufferedReader(new FileReader(jsonFile));
		String line;

		while((line = br.readLine()) != null) 
		{
			text.append(line);
			text.append('\n');
		}

		return new JSONObject(text.toString());
	}
	public static JSONArray parseFileToJSONArray(String bvFilename) throws JSONException, IOException {
		File jsonFile = new File(bvFilename);

		//Read text from file
		StringBuilder text = new StringBuilder();
		BufferedReader br = new BufferedReader(new FileReader(jsonFile));
		String line;

		while ((line = br.readLine()) != null) 
		{
			text.append(line);
			text.append('\n');
		}

		return new JSONArray(text.toString());
	}
	public static JSONObject[] JSONArray2Array(JSONArray jsonArray) throws JSONException {
		JSONObject[] output = new JSONObject[jsonArray.length()];
		for(int i = 0; i < jsonArray.length(); i++){
			output[i] = jsonArray.getJSONObject(i);
		}
		return output;
	}
	public static String[] getFields(JSONObject bubbleVals) throws JSONException {
		JSONArray fields = bubbleVals.getJSONArray("fields");
		String[] fieldString = new String[fields.length()];
		for(int i = 0; i < fields.length(); i++){
			Number[] segmentCounts = getSegmentValues(fields.getJSONObject(i));
			fieldString[i] = fields.getJSONObject(i).getString("label") + " : \n" + MScanUtils.sum(segmentCounts);
		}
		return fieldString;
	}
	public static Number[] getFieldCounts(JSONObject bubbleVals) throws JSONException {
		JSONArray fields = bubbleVals.getJSONArray("fields");
		Number[] fieldCounts = new Number[fields.length()];
		for(int i = 0; i < fields.length(); i++){
			Number[] segmentCounts = getSegmentValues(fields.getJSONObject(i));
			fieldCounts[i] = MScanUtils.sum(segmentCounts);
		}
		return fieldCounts;
	}
	/**
	 * Assumes segments have integer values and returns them in an array.
	 * @param field
	 * @return
	 * @throws JSONException
	 */
	public static Number[] getSegmentValues(JSONObject field) throws JSONException {

		JSONArray jArray = field.getJSONArray("segments");
		Number[] bubbleCounts = new Number[jArray.length()];

		for (int i = 0; i < jArray.length(); i++) {
			int numFilled = 0;
			try{
				JSONArray bubbleValArray = jArray.getJSONObject(i).getJSONArray("bubbles");
				for (int j = 0; j < bubbleValArray.length(); j++) {
					if( bubbleValArray.getJSONObject(j).getInt("value") > 0 ){
						numFilled++;
					}
				}
			}catch(JSONException e){
				//Log.i("mScan", "getSegmentCounts Exception: " + e.toString());
			}
			
			bubbleCounts[i] = numFilled;
		}
		return bubbleCounts;
	}
}
