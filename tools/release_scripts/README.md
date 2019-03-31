# Release scripts

Validating a release requires a lot of work and to that end, a few scripts were written to automate the process as much as possible.

## test_everything.py

This script runs the regression tests on every platform except **iOS**, **Android**, and **Windows ARM64**. It will run every permutation possible.

## extract_stats.py

This script is used to run the `acl_compressor` tool over a large dataset in order to extract statistics.