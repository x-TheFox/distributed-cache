# Protocol Documentation for Distributed Cache System

## Overview

This document outlines the protocols supported by the distributed in-memory cache system. The system is designed to handle multiple communication protocols, allowing clients to interact with the cache using different methods. The primary protocols implemented are Memcached and HTTP.

## Supported Protocols

### 1. Memcached Protocol

The Memcached protocol is a simple text-based protocol used for caching data. It supports basic operations such as `GET`, `SET`, `DELETE`, and `INCREMENT`. The protocol is designed for high performance and low latency, making it suitable for caching scenarios.

#### Commands

- **GET**: Retrieve the value associated with a given key.
- **SET**: Store a value with a specified key.
- **DELETE**: Remove a key-value pair from the cache.
- **INCREMENT**: Increase the value of a numeric key.

#### Example

```
SET mykey 0 900 9
value12345
```

### 2. HTTP Protocol

The HTTP protocol allows clients to interact with the cache system using standard HTTP methods. This protocol is useful for web-based applications and services that require a RESTful interface.

#### Supported Methods

- **GET**: Retrieve the value associated with a given key.
- **POST**: Store a value with a specified key.
- **DELETE**: Remove a key-value pair from the cache.

#### Example

```
POST /cache/mykey HTTP/1.1
Content-Length: 9

value12345
```

## Serialization

Data sent over the network must be serialized to ensure proper transmission. The cache system supports various serialization formats, including JSON and binary formats, depending on the protocol used.

### JSON Serialization

For HTTP requests and responses, JSON is used for data serialization. This allows for easy integration with web applications.

### Binary Serialization

For the Memcached protocol, a more compact binary format is used to minimize the payload size and improve performance.

## Conclusion

This document provides an overview of the protocols supported by the distributed in-memory cache system. By adhering to these protocols, clients can efficiently interact with the cache, ensuring high performance and scalability.