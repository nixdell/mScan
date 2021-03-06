package com.bubblebot;


import java.util.Date;
import java.util.Random;

import org.json.JSONObject;

import com.bubblebot.RunProcessor.Mode;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.WebView;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.CompoundButton.OnCheckedChangeListener;

/**
 * This activity runs the form processor and provides user feedback
 * displaying progress dialogs and the alignment results.
 */
public class AfterPhotoTaken extends Activity {
	
	private boolean aligned = false;
    private String photoName;
    private RunProcessor runProcessor;
    
    private Button processButton;
    private LinearLayout content;
    
    private long startTime;//only needed in debugMode

    @Override
	protected void onCreate(Bundle savedInstanceState) {
    	super.onCreate(savedInstanceState);
    	
    	Log.i("mScan", "After Photo taken");
    	
    	setContentView(R.layout.after_photo_taken);
    	
    	content = (LinearLayout) findViewById(R.id.myLinearLayout);
    	
		Bundle extras = getIntent().getExtras();
		if (extras == null) {
			Log.i("mScan","extras == null");
			//This might happen if we use back to reach this activity from the camera activity.
			content.setVisibility(View.VISIBLE);
			return;
		}
		
		photoName = extras.getString("photoName");
		
	    //SharedPreferences settings = getSharedPreferences(getResources().getString(R.string.prefs_name), 0);
		SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
		runProcessor = new RunProcessor(handler, photoName,
				settings.getBoolean("doFormDetection", false),
				settings.getBoolean("calibrate", false));
		
		if( extras.getBoolean("preAligned") ){
			startThread(RunProcessor.Mode.LOAD);
		}
		else{
			startThread(RunProcessor.Mode.LOAD_ALIGN);
		}
		

		//Button handlers:
		Button retake = (Button) findViewById(R.id.retake_button);
		retake.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				Intent intent = new Intent(getApplication(), BubbleCollect2.class);
				startActivity(intent);
			}
		});
		processButton = (Button) findViewById(R.id.process_button);
		processButton.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				startThread(RunProcessor.Mode.PROCESS);
			}
		});
	}
    //Launch a thread that runs runProcessor.
    //Shows a status dialog as well.
	protected void startThread(Mode mode) {
		if(MScanUtils.DebugMode){
			Log.i("mScan","photoName: " + photoName);
			startTime = new Date().getTime();
		}
		showDialog(mode.ordinal());
		runProcessor.setMode(mode);
		
		Thread thread = new Thread( runProcessor );
		thread.setPriority(Thread.MAX_PRIORITY);
		thread.start();
	}

	@Override
	protected Dialog onCreateDialog(int id, Bundle args) {
		
		//final String [] didYouKnow = getResources().getStringArray(R.array.did_you_know);
		
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		
		switch (RunProcessor.Mode.values()[id]) {
		case LOAD:
			builder.setTitle("Loading Image");
			break;
		case LOAD_ALIGN:
			builder.setTitle(getResources().getString(R.string.aligning_form));
			break;
		case PROCESS:
			builder.setTitle(getResources().getString(R.string.processing_form));
			break;
		default:
			return null;
		}
		builder.setCancelable(false);
		//		.setMessage("Did you know... \n " + didYouKnow[(new Random()).nextInt(didYouKnow.length)]);
		
		return builder.create();
	}

    private Handler handler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
            	
            	RunProcessor.Mode mode = RunProcessor.Mode.values()[msg.what];
            	try{
            		dismissDialog(msg.what);
            	}
            	catch(IllegalArgumentException e){
            		Log.i("mScan", "Exception: Dialog with id " + msg.what + " was not previously shown.");
            	}
            	
            	if(MScanUtils.DebugMode){
	    			Log.i("mScan", "Mode: " + mode);
	    			double timeTaken = (double) (new Date().getTime() - startTime) / 1000;
	    			Log.i("mScan", "Time taken:" + String.format("%.2f", timeTaken));
            	}
            	
            	switch (mode) {
        		case LOAD:
        		case LOAD_ALIGN:
            		aligned = (msg.arg1 == 1);
	        		if ( aligned ) {
	        			MScanUtils.displayImageInWebView((WebView)findViewById(R.id.webview),
	        														MScanUtils.getAlignedPhotoPath(photoName));
	        		}
	        		else {
	        			RelativeLayout failureMessage = (RelativeLayout) findViewById(R.id.failureMessage);
	        			failureMessage.setVisibility(View.VISIBLE);
	        		}
	        		content.setVisibility(View.VISIBLE);
	        		processButton.setEnabled(aligned);
        			break;
        		case PROCESS:
            		if ( msg.arg1 == 1 ) {
            			//JSONObject formRoot = JSONUtils.parseFileToJSONObject(MScanUtils.getJsonPath(photoName));
            			//TODO: Attach clinic and other user specified data to JSON output.		
            			//writeJSONObjectToFile(formRoot);
	            		Intent intent = new Intent(getApplication(), DisplayProcessedForm.class);
	                    intent.putExtra("photoName", photoName);
	                    startActivity(intent);
	        			finish(); 
	        			//Not sure this finish is necessary, but it might fix the inexplicable crashes
	        			//we've been having (since they might have something to do with the processor not being freed)
	        			//however, this has the downside of preventing users from going back to the aligned image
	                    //One possible reason for crashing is the user accidentally hitting the search button
	        			//during the dialog.
            		}
        			break;
        		default:
        			return;
        		}
            }
    };
}