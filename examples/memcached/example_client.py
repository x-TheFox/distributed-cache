import socket
import pickle
import sys

class MemcachedClient:
    def __init__(self, host='localhost', port=11211):
        self.server_address = (host, port)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect(self.server_address)

    def set(self, key, value):
        message = f'set {key} 0 900 {len(value)}\r\n{value}\r\n'
        self.socket.sendall(message.encode())
        response = self.socket.recv(1024).decode()
        return response

    def get(self, key):
        message = f'get {key}\r\n'
        self.socket.sendall(message.encode())
        response = self.socket.recv(1024).decode()
        if response.startswith('VALUE'):
            return response.split('\r\n')[1]
        return None

    def delete(self, key):
        message = f'delete {key}\r\n'
        self.socket.sendall(message.encode())
        response = self.socket.recv(1024).decode()
        return response

    def close(self):
        self.socket.close()

if __name__ == "__main__":
    client = MemcachedClient()
    if len(sys.argv) < 3:
        print("Usage: example_client.py <command> <key> [value]")
        sys.exit(1)

    command = sys.argv[1]
    key = sys.argv[2]
    value = sys.argv[3] if len(sys.argv) > 3 else None

    if command == 'set' and value is not None:
        print(client.set(key, value))
    elif command == 'get':
        print(client.get(key))
    elif command == 'delete':
        print(client.delete(key))
    else:
        print("Invalid command or missing value for 'set'.")
    
    client.close()