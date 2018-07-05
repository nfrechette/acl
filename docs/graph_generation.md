# Graph generation

In order to track our progress and where we stand against the competition, we generate lots of data and turn it into graphs as seen [here](cmu_performance.md) for CMU, [here](paragon_performance.md) for Paragon, and [here](fight_scene_performance.md) for the Matinee fight scene.

The steps to generate these graphs are detailed here for transparency.

## Generating the compression performance data

The first step is to run the compression algorithms in order to extract the raw performance data. This is done by running the [acl_compressor](../tools/acl_compressor) executable.

We run it on the input data with the associated python script found [here](../tools/acl_compressor/acl_compressor.py). Typical usage looks like this:

`python acl_compressor.py -acl=<path/to/raw/acl/clips> -stats=<path/to/output/stats> -parallel=4 -csv_summary -csv_bit_rate -csv_error`

This script generates various CSV files.

Yet another script automates dumping various permutations and can be found [here](../tools/release_scripts/extract_stats.py).

## Generating the decompression performance data

Much like the compression data, we use a custom tool to generate the decompression CSV files. It is fairly simple with the python script found [here](../tools/acl_decompressor/acl_decompressor.py). Typical usage looks like this:

`python acl_decompressor -stats=<path/to/output/stats> -csv [-ios] [-android]`

The script will generate various CSV files. Note that for *iOS* you must copy the generated SJSON files manually from the device after running the *acl_decompressor* tool manually as well as add the `-ios` switch to the python script, and for *Android* you must add the `-android` switch after running the tool manually.

## Generating the graphs

The generated CSV files in the previous steps can be very large and contain lots of things. In order to make sense of it and to make graph generation easier, we use an array of scripts to parse, extract, and format the data into simpler and cleaner CSV files.

These scripts can be found [here](../tools/graph_generation) along with some instructions on how to run them.
