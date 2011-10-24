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
	public static JSONObject generateSimplifiedJSON(String photoName) throws JSONException, IOException {
		JSONObject outRoot = new JSONObject();
		
		JSONObject bubbleVals = parseFileToJSONObject(MScanUtils.getJsonPath(photoName));
		JSONArray fields = bubbleVals.getJSONArray("fields");
		
		outRoot.put("form_name", bubbleVals.getString("template_path"));
		
		outRoot.put("location", "location name");
		
		outRoot.put("data_capture_date",
				new Date(new File(MScanUtils.getPhotoPath(photoName)).lastModified()).toString());
		
		JSONObject outFields = new JSONObject();
		for(int i = 0; i < fields.length(); i++){
			outFields.put(fields.getJSONObject(i).getString("label"),
						  MScanUtils.sum(getSegmentCounts(fields.getJSONObject(i))));
		}
		outRoot.put("fields", outFields);
		return outRoot;
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
			Integer[] segmentCounts = getSegmentCounts(fields.getJSONObject(i));
			fieldString[i] = fields.getJSONObject(i).getString("label") + " : \n" + MScanUtils.sum(segmentCounts);
		}
		return fieldString;
	}
	public static Integer[] getFieldCounts(JSONObject bubbleVals) throws JSONException {
		JSONArray fields = bubbleVals.getJSONArray("fields");
		Integer[] fieldCounts = new Integer[fields.length()];
		for(int i = 0; i < fields.length(); i++){
			Integer[] segmentCounts = getSegmentCounts(fields.getJSONObject(i));
			fieldCounts[i] = MScanUtils.sum(segmentCounts);
		}
		return fieldCounts;
	}
	public static Integer[] getSegmentCounts(JSONObject field) throws JSONException {

		JSONArray jArray = field.getJSONArray("segments");
		Integer[] bubbleCounts = new Integer[jArray.length()];

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
