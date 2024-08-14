#!/bin/bash

# Run this in build or build_debug

cmake --build . --parallel --target production && ./production $1
