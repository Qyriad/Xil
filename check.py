#!/usr/bin/env python3

# Hacky script to run clang -fsyntax-only on source files using compile_commands.json.

import json
import os
import shlex
import subprocess

# This script is meant to be run from the build directory.
# For convenience, cd to that if we need to.
if os.path.basename(os.getcwd()) != 'build':
    os.chdir('build')

commands_json = json.load(open('compile_commands.json', 'r'))
commands = []

for entry in commands_json:
    # Don't warn for libraries.
    replaced = entry['command'].replace('-I/nix/store', '-isystem /nix/store')
    # Then split it...
    split_cmd = shlex.split(replaced)
    # ..And replace gcc with clang.
    final = ['clang++', '-fsyntax-only', '-Werror', '-Wpedantic', *split_cmd[1:]]
    commands.append(final)

for command in commands:
    print(" ".join([shlex.quote(arg) for arg in command]))
    subprocess.check_call(command)
