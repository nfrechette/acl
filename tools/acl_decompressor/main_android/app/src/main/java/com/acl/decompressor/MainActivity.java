package com.acl.decompressor;

import android.app.Activity;
import android.content.res.AssetManager;
import android.widget.TextView;
import android.os.Bundle;

public class MainActivity extends Activity {
	static {
		System.loadLibrary("acl_decompressor");
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		TextView resultTextView = new TextView(this);
		String outputDirectory = getExternalFilesDir(null).getAbsolutePath();

		int result = nativeMain(getAssets(), outputDirectory);

		if (result == 0)
			resultTextView.setText("Success!");
		else
			resultTextView.setText("Failed!");

		setContentView(resultTextView);
	}

	public native int nativeMain(AssetManager assetManager, String outputDirectory);
}
