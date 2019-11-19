package com.acl.compressor;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;

public class MainActivity extends Activity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		TextView resultTextView = new TextView(this);

		System.loadLibrary("acl_compressor");

		int result = nativeMain();

		if (result == 0)
			resultTextView.setText("Success!");
		else
			resultTextView.setText("Failed!");

		setContentView(resultTextView);
	}

	public native int nativeMain();
}
