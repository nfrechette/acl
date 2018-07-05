# acl_decompressor in a nutshell

This tool serves two purposes:

*  Approximate a real game engine integration in order to reliably measure decompression performance
*  Extract decompression statistics

The various command line arguments to the python script and the executable allow the user to control this behavior.

## Decompression statistics

When generating the [graphs](../../docs/graph_generation.md), a python script is used in order to run the decompression over a small dataset and aggregate the results into CSV files as well as the standard output.

Use `python acl_decompressor.py -help` in order to get a description of the supported script arguments.
