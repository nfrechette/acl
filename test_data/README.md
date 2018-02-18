# Regression testing

This directory is to contain all the relevant data to perform regression testing. Sadly, the setup is manual for now and as a result the continuous integration builds do not run regression testing yet.

## Setup

Find the latest test data zip file located on Google Drive and save it into this directory. If your data ever becomes stale, the python script will indicate as such and you will need to perform this step again. The zip file contains a number of clips from the [Carnegie-Mellon University](../docs/cmu_performance.md) database that were hand selected. A readme file within the zip file details this.

*  **v1** Test data [link](https://drive.google.com/open?id=1psNO0riJ6RlD5_vsvPh2vPsgEBvLN3Hr) (**Latest**)

## Running the tests

Using **Python 3**, run [make.py](../make.py) with the `-regression_test` command line switch.

## Test configurations

The [configs](./configs) directory contains a number of test configuration files. Each file specifies the compression settings to use and the corresponding expected error threshold. A reference configuration file can be found [here](./reference.config.sjson).

All clips are tested against every test configuration.

## Debugging a test failure

If a regression test fails, the python script will output the clip that failed along with the command line used to reproduce the failure.
