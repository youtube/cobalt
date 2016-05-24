#!/bin/bash

# Ensure we are in the bindings directory.
cd "$(dirname "${BASH_SOURCE[0]}")"

python run_cobalt_bindings_tests.py jsc "$@"
python run_cobalt_bindings_tests.py mozjs "$@"
