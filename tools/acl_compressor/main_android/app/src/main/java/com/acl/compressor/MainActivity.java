package com.acl.compressor;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;

public class MainActivity extends Activity {
	static {
		System.loadLibrary("acl_compressor");
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		TextView resultTextView = new TextView(this);

		int result = nativeMain();

		if (result == 0)
			resultTextView.setText("Success!");
		else
			resultTextView.setText("Failed!");

		setContentView(resultTextView);
	}

	public native int nativeMain();
}
