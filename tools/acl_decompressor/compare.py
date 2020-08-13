import os
import subprocess
import sys
import time

if __name__ == "__main__":
	current_dir = os.getcwd()
	google_benchmark_dir = os.path.join(current_dir, '..', '..', 'external', 'benchmark', 'tools')
	google_benchmark_compare_script = os.path.join(google_benchmark_dir, 'compare.py')

	compare_cmd = ['python', google_benchmark_compare_script, 'benchmarks'] + sys.argv[1:]

	subprocess.Popen(compare_cmd, stdout=sys.stdout, stderr=sys.stderr)
