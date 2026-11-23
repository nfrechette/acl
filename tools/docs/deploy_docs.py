#!/usr/bin/env python3

# Deploys the documentation site to the gh-pages branch with mike.
# mike keeps one copy of the site for each version:
#   - The develop branch deploys to the 'develop' version.
#   - A release tag vX.Y.Z deploys to the 'X.Y' version. A patch release replaces the site of its X.Y version.
#     The newest X.Y version also has the 'latest' alias.
# The default version of the site is 'latest'. Before the first release with documentation, it is 'develop'.
#
# Run tools/docs/generate_api_docs.py before this script.
# Usage: python3 tools/docs/deploy_docs.py --ref refs/heads/develop [--push]

import argparse
import json
import re
import subprocess
import sys

def parse_argv():
	parser = argparse.ArgumentParser(description='Deploys the documentation site to the gh-pages branch with mike.')
	parser.add_argument('--ref', required=True, help='The git ref to deploy, for example refs/heads/develop or refs/tags/v2.2.0')
	parser.add_argument('--push', action='store_true', help='Push the gh-pages branch to the remote')
	return parser.parse_args()

def version_key(version):
	return tuple(int(value) for value in version.split('.'))

def run_mike(args, mike_args):
	command = ['mike'] + mike_args
	if args.push and mike_args[0] in ('deploy', 'set-default'):
		command.append('--push')
	print(' '.join(command))
	subprocess.check_call(command)

def main():
	args = parse_argv()

	if args.ref == 'refs/heads/develop':
		version = 'develop'
	else:
		match = re.match(r'^refs/tags/v(\d+)\.(\d+)\.(\d+)$', args.ref)
		if not match:
			print('Cannot deploy the documentation of ref: {}'.format(args.ref))
			sys.exit(1)
		version = '{}.{}'.format(match.group(1), match.group(2))

	# The gh-pages branch does not exist before the first deploy, and mike then lists no version
	deployed = json.loads(subprocess.check_output(['mike', 'list', '--json']).decode('utf-8') or '[]')
	releases = [entry['version'] for entry in deployed if re.match(r'^\d+\.\d+$', entry['version'])]

	if version == 'develop':
		run_mike(args, ['deploy', 'develop'])
	elif not releases or version_key(version) >= max(version_key(release) for release in releases):
		# A patch release of an older X.Y version does not move the 'latest' alias
		run_mike(args, ['deploy', '--update-aliases', version, 'latest'])
	else:
		run_mike(args, ['deploy', version])

	has_latest = version != 'develop' or any('latest' in entry['aliases'] for entry in deployed)
	run_mike(args, ['set-default', 'latest' if has_latest else 'develop'])

if __name__ == '__main__':
	main()
