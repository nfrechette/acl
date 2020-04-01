package com.acl.unit_tests;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;

public class MainActivity extends Activity {
	static {
		System.loadLibrary("acl_unit_tests");
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		TextView resultTextView = new TextView(this);

		int numUnitTestCases = getNumUnitTestCases();
		int numFailed = runUnitTests();

		if (numFailed == 0)
			resultTextView.setText("All " + numUnitTestCases + " test cases ran successfully!");
		else
			resultTextView.setText(numFailed + " test cases failed!");

		setContentView(resultTextView);
	}

	public native int getNumUnitTestCases();
	public native int runUnitTests();
}
