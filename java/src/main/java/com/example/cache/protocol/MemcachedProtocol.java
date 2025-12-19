package com.example.cache.protocol;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

public class MemcachedProtocol {

    private static final String SET_COMMAND = "set";
    private static final String GET_COMMAND = "get";
    private static final String DELETE_COMMAND = "delete";

    private Map<String, byte[]> cacheStore;

    public MemcachedProtocol() {
        this.cacheStore = new HashMap<>();
    }

    public String processCommand(String command) {
        String[] parts = command.split(" ");
        String response;

        switch (parts[0].toLowerCase()) {
            case SET_COMMAND:
                response = handleSet(parts);
                break;
            case GET_COMMAND:
                response = handleGet(parts);
                break;
            case DELETE_COMMAND:
                response = handleDelete(parts);
                break;
            default:
                response = "ERROR: Unknown command";
        }

        return response;
    }

    private String handleSet(String[] parts) {
        if (parts.length < 3) {
            return "ERROR: SET command requires key and value";
        }
        String key = parts[1];
        byte[] value = parts[2].getBytes();
        cacheStore.put(key, value);
        return "STORED";
    }

    private String handleGet(String[] parts) {
        if (parts.length < 2) {
            return "ERROR: GET command requires key";
        }
        String key = parts[1];
        byte[] value = cacheStore.get(key);
        if (value == null) {
            return "ERROR: Not found";
        }
        return "VALUE " + key + " " + new String(value);
    }

    private String handleDelete(String[] parts) {
        if (parts.length < 2) {
            return "ERROR: DELETE command requires key";
        }
        String key = parts[1];
        if (cacheStore.remove(key) != null) {
            return "DELETED";
        } else {
            return "ERROR: Not found";
        }
    }
}