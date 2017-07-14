# Why we don't use 64-bit floats

When the library was first written, the compression algorithms used 64-bit floating point math. The assumption was that accuracy when measuring error was going to be higher and lead to a smaller memory footprint. Eventually, it became clear that maintaining the code with the conversions everywhere required was a lot of work which prompted the question of whether it was really worth it or not. Before and during the convertion of the code to 32-bit floating point math, statistics were pulled and a verdict reached: 32-bit was faster, easier to maintain, and generally lead to higher accuracy (but not always). For this reason, 32-bit is used everywhere. This also opens the door for runtime realtime compression later on.

**TODO: Show stats and graphs**
