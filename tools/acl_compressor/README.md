# acl_compressor in a nutshell

This tool serves two purposes:

*  Test compression and decompression
*  Extract compression statistics

The various command line arguments to the python script and the executable allow the user to control this behavior. Note that this tool isn't meant to be used at part of a pipeline to compress animation clips to later be used at runtime by a game.

## Regression testing

When regression testing, the python script isn't used but the executable is. By passing the argument `-test` to it, decompression will be tested and an assert will trigger should the error exceed the provided threshold by the configuration passed by `-config=<path to sjson config file>`.

## Compression statistics

When generating the [graphs](../../docs/graph_generation.md), a python script is used in order to run the compression over a large dataset and aggregate the results into various CSV files as well as the standard output.

Use `python acl_compressor.py -help` in order to get a description of the supported script arguments.
