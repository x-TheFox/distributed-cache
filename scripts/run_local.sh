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

# Build the Java part of the project
cd ../../java
./gradlew build

# Run the Java server
java -cp build/libs/* com.example.cache.NettyServer &

# Wait for servers to start
sleep 5

echo "Cache system is running locally."