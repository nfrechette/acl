package com.acl;

import android.app.Activity;
import android.content.res.AssetManager;
import android.widget.TextView;
import android.os.Bundle;

public class MainActivity extends Activity {
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		TextView resultTextView = new TextView(this);

		System.loadLibrary("acl_regression_tester_android");

		int result = nativeMain(getAssets());

		if (result == 0)
			resultTextView.setText("Success!");
		else if (result > 0)
			resultTextView.setText("Some regression tests failed: " + result);
		else
			resultTextView.setText("Failed with error: " + result);

		setContentView(resultTextView);
	}

	public native int nativeMain(AssetManager assetManager);
}
