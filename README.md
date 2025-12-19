# Distributed In-Memory Cache System

## Overview
This project implements a distributed in-memory cache system with multi-protocol support, designed for high concurrency and efficient memory management. The system is built using both C++ and Java, allowing for flexibility in deployment and usage.

## Features
- **Multi-Protocol Support**: The cache system supports both Memcached and HTTP protocols for client communication.
- **Concurrency**: Designed to handle multiple requests simultaneously, ensuring high performance in multi-threaded environments.
- **Memory Management**: Utilizes custom memory allocators and an LRU eviction policy to optimize memory usage and performance.
- **Networking**: Implements both TCP and UDP servers for handling incoming requests.

## Project Structure
The project is organized into two main directories: `cpp` and `java`, each containing the respective implementations and tests.

### C++ Implementation
- **Source Code**: Located in the `cpp/src` directory.
- **Headers**: Interface definitions are in the `cpp/include` directory.
- **Tests**: Unit tests can be found in the `cpp/tests` directory.
- **Benchmarks**: Performance benchmarks are located in the `cpp/benchmarks` directory.
- **Documentation**: Design documentation is available in the `cpp/docs` directory.

### Java Implementation
- **Source Code**: Located in the `java/src/main/java/com/example/cache` directory.
- **Tests**: Unit tests are in the `java/src/test/java/com/example/cache` directory.
- **Documentation**: Design documentation is available in the `java/docs` directory.

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

### Java
1. Navigate to the `java` directory.
2. Build the project using Gradle:
   ```
   ./gradlew build
   ```
3. Run the server:
   ```
   java -cp build/libs/* com.example.cache.NettyServer
   ```

## Documentation
- **Architecture**: Detailed architecture diagrams and explanations can be found in `docs/architecture.md`.
- **Protocols**: Information on the protocols used in the cache system is available in `docs/protocol.md`.

## Contributing
Contributions are welcome! Please refer to the `CONTRIBUTING.md` file for guidelines.

## License
This project is licensed under the MIT License. See the `LICENSE` file for more details.