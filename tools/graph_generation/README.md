# Graph generation

The scripts found in this folder are used to generate various curated CSV files from the raw input CSV data.

Usage always takes the form: `python <script.py> <path/to/input_file.sjson>`

The SJSON input file uses a simple and custom format that describes where the raw data lives and other associated information. You can look at the CMU files in this directory for inspiration.

The `extract_stats.py` script found [here](./release_scripts/extract_stats.py) is used to generate the raw data necessary for most scripts.

## gen_bit_rate_stats.py

This script aggregates the raw data and formats the bit rate usage frequency.

## gen_decomp_stats.py

This script parses the decompression profiling results and outputs various CSV files.

## gen_decomp_delta_stats.py

This script parses the decompression profiling results and outputs various CSV files to track the decompression performance across versions.

## gen_full_error_stats.py

This script reads the very large raw data files and outputs the percentiles. This reduces the amount of data so that we can actually see how it looks without manipulating millions of rows.

Note: This scripts uses a *LOT* of memory. It is preferable to run it with 64-bit python to avoid running out of memory.

## gen_summary_stats.py

This script takes in the summary input data and converts it into various output CSV files that track the metrics we care about: clip durations, compression ratio, compression ratio by the raw size, max error, max error by raw size, and the compression ratio by max error.
