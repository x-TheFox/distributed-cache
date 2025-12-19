FROM ubuntu:20.04

# Set environment variables
ENV CMAKE_VERSION=3.20.3-0ubuntu1
ENV CC=gcc
ENV CXX=g++

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Create a directory for the application
WORKDIR /app

# Copy the C++ source code
COPY cpp /app/cpp

# Build the application
RUN cd cpp && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make

# Expose the port the app runs on
EXPOSE 8080

# Command to run the application
CMD ["./build/main"]