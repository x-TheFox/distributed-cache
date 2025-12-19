import requests

class HttpClient:
    def __init__(self, base_url):
        self.base_url = base_url

    def get(self, key):
        response = requests.get(f"{self.base_url}/get/{key}")
        if response.status_code == 200:
            return response.json().get('value')
        else:
            return None

    def set(self, key, value):
        response = requests.post(f"{self.base_url}/set", json={'key': key, 'value': value})
        return response.status_code == 200

    def delete(self, key):
        response = requests.delete(f"{self.base_url}/delete/{key}")
        return response.status_code == 200

if __name__ == "__main__":
    client = HttpClient("http://localhost:8080")
    
    # Example usage
    client.set("example_key", "example_value")
    print(client.get("example_key"))
    client.delete("example_key")