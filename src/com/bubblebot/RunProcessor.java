package com.bubblebot;

import android.os.Handler;
import android.os.Message;
import android.util.Log;

import com.bubblebot.jni.MarkupForm;
import com.bubblebot.jni.Processor;
/*
 * RunProcessor is used to run the image processing algorithms on a separate thread.
 */
public class RunProcessor implements Runnable {
	
	public enum Mode {
	    LOAD, LOAD_ALIGN, PROCESS
	}
	
	private boolean doFormDetection;
	private boolean undistort;
	
	private Mode mode;
	private Processor mProcessor;
	private Handler handler;
	private String photoName;

	public RunProcessor(Handler handler, String photoName, boolean doFormDetection, boolean undistort) {
		this.handler = handler;
		this.photoName = photoName;
		this.doFormDetection = doFormDetection;
		this.undistort = undistort;
		mProcessor = new Processor(MScanUtils.appFolder);
	}
	/**
	 * This method sets the mode the processor is to run in.
	 * @param mode
	 */
	public void setMode(Mode mode) {
		this.mode = mode;
	}
	//@Override
	public void run() {
		
		Message msg = new Message();
		msg.arg1 = 0;//I'm using arg1 as a success indicator. A value of 1 means success.
		msg.what = mode.ordinal();

		if(mode == Mode.PROCESS) {
			if( mProcessor.processForm( MScanUtils.getJsonPath(photoName)) ) {
				
				MarkupForm.markupForm(  MScanUtils.getJsonPath(photoName),
										MScanUtils.getAlignedPhotoPath(photoName),
										MScanUtils.getMarkedupPhotoPath(photoName));
				msg.arg1 = 1;
			}
		}
		else if(mode == Mode.LOAD) {
			if( mProcessor.loadFormImage(MScanUtils.getAlignedPhotoPath(photoName), false) ) {
				if(mProcessor.setTemplate( MScanUtils.appFolder + "form_templates/SIS-A01.json" )) {
					msg.arg1 = 1;
				}
			}
		}
		else if(mode == Mode.LOAD_ALIGN) {
			
			if(mProcessor.loadFormImage(MScanUtils.getPhotoPath(photoName), undistort)) {
				Log.i("mScan","Loading: " + photoName);
				
				String[] templatePaths = { "form_templates/SIS-A01", "form_templates/checkbox_form", "form_templates/checkbox_form_2" };
				//"form_templates/UW_course_eval_A_front" };
				//String[] templatePaths = { "form_templates/checkbox_form" };
				int formIdx = 0;
				
				if(doFormDetection) {
					for(int i = 0; i < templatePaths.length; i++){ 
						//Log.i("mScan", "loadingFD: " + MScanUtils.appFolder + templatePaths[i]);
						mProcessor.loadFeatureData(MScanUtils.appFolder + templatePaths[i]);
					}
					//Log.i("mScan", templatePaths.length + " Templates Loaded");
					formIdx = mProcessor.detectForm();
				}
				else {
					mProcessor.loadFeatureData(MScanUtils.appFolder + templatePaths[0]);
				}
				
				if(formIdx >= 0) {
					if(mProcessor.setTemplate(MScanUtils.appFolder + templatePaths[formIdx])) {
						Log.i("mScan","template loaded");
						if( mProcessor.alignForm(MScanUtils.getAlignedPhotoPath(photoName), formIdx) ) {
							msg.arg1 = 1;//indicates success
							Log.i("mScan","aligned");
						}
					}
					else {
						Log.i("mScan","Faled to set template.");
					}
				}
				else {
					Log.i("mScan","Faled to detect form.");
				}
			}
			else {
				Log.i("mScan","Faled to load image: " + photoName);
			}
		}
		handler.sendMessage(msg);
	
	}
}
