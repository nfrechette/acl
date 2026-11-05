# Regression testing

This directory holds the data for the regression tests. The continuous integration builds run the regression tests on each push to a branch.

## Setup

The [acl-test-data](https://github.com/nfrechette/acl-test-data) repository makes the regression test data. Each version of the data is a zip file in a [release](https://github.com/nfrechette/acl-test-data/releases) of that repository. The zip file contains clips from the [Carnegie-Mellon University](../docs/cmu_performance.md) database that were selected by hand, and scalar track lists. A readme file in the zip file lists the clips.

The regression tests use the version that [make.py](../make.py) specifies (`CURRENT_REGRESSION_TEST_DATA_FILENAME`). When that zip file is not in this directory, the script downloads it from the release and extracts it into `regression_test_data`. When the version changes, the script removes the old extracted data and downloads the new zip file.

To use a zip file that is not published yet, copy it into this directory with the expected name and remove the `regression_test_data` directory.

## Running the tests

Using **Python 3**, run [make.py](../make.py) with the `-regression_test` command line switch.

## Test configurations

The [configs](./configs) directory contains a number of test configuration files. Each file specifies the compression settings to use and the corresponding expected error threshold. A reference configuration file can be found [here](./reference.config.sjson).

All clips are tested against every test configuration.

## Debugging a test failure

If a regression test fails, the python script will output the clip that failed along with the command line used to reproduce the failure.
