# Distributed In-Memory Cache System

[![CI](https://github.com/x-TheFox/distributed-cache/actions/workflows/ci.yml/badge.svg)](https://github.com/x-TheFox/distributed-cache/actions/workflows/ci.yml) [![codecov](https://codecov.io/gh/x-TheFox/distributed-cache/branch/main/graph/badge.svg)](https://codecov.io/github/x-TheFox/distributed-cache)

## Overview
This project implements a distributed in-memory cache system with multi-protocol support, designed for high concurrency and efficient memory management. The system is built using C++, focusing on performance and low-level control for production-grade caching.

## Features
- **Multi-Protocol Support**: The cache system supports both Memcached and HTTP protocols for client communication.
- **Concurrency**: Designed to handle multiple requests simultaneously, ensuring high performance in multi-threaded environments.
- **Memory Management**: Utilizes custom memory allocators and an LRU eviction policy to optimize memory usage and performance.
- **Networking**: Implements both TCP and UDP servers for handling incoming requests.

## Project Structure
The project is organized under the `cpp` directory, which contains the implementation, tests, benchmarks, and related documentation.

### C++ Implementation
- **Source Code**: Located in the `cpp/src` directory.
- **Headers**: Interface definitions are in the `cpp/include` directory.
- **Tests**: Unit tests can be found in the `cpp/tests` directory.
- **Benchmarks**: Performance benchmarks are located in the `cpp/benchmarks` directory.
- **Documentation**: Design documentation is available in the `cpp/docs` directory.

**Note:** The Java implementation has been removed; this repository is now maintained as a C++-only project.

## Setup Instructions
### C++
1. Navigate to the `cpp` directory.
2. Build the project using CMake:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```
3. Run the server:
   ```
   ./cache_server
   ```



## Documentation
- **Architecture**: Detailed architecture diagrams and explanations can be found in `docs/architecture.md`.
- **Protocols**: Information on the protocols used in the cache system is available in `docs/protocol.md`.

## CI & Quality
- The CI pipeline runs on pushes and pull requests with a Linux/macOS matrix and includes:
  - Build and unit tests (GoogleTest)
  - Static analysis (cppcheck)
  - Formatting checks (clang-format)
  - Sanitizers (ASan/UBSan)
  - Coverage collection and upload to Codecov
- Test results (JUnit XML) and coverage artifacts are attached to workflow runs; Codecov also posts test/failure insights to PRs.

## Contributing
Contributions are welcome! Please refer to the `CONTRIBUTING.md` file for guidelines.

## License
This project is licensed under the MIT License. See the `LICENSE` file for more details.
