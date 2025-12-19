#!/bin/bash

# This script is used to run the cache system locally.

# Build the C++ part of the project
cd cpp
mkdir -p build
cd build
cmake ..
make

# Run the C++ server
./cache_server &

# Java implementation removed; script is C++-only now
cd ../..

# Wait for servers to start
sleep 5

echo "Cache system is running locally."