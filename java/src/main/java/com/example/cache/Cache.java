package com.example.cache;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantLock;

public class Cache<K, V> {
    private final ConcurrentHashMap<K, V> cache;
    private final ReentrantLock lock;

    public Cache() {
        this.cache = new ConcurrentHashMap<>();
        this.lock = new ReentrantLock();
    }

    public void put(K key, V value) {
        lock.lock();
        try {
            cache.put(key, value);
        } finally {
            lock.unlock();
        }
    }

    public V get(K key) {
        return cache.get(key);
    }

    public void remove(K key) {
        lock.lock();
        try {
            cache.remove(key);
        } finally {
            lock.unlock();
        }
    }

    public void clear() {
        lock.lock();
        try {
            cache.clear();
        } finally {
            lock.unlock();
        }
    }

    public boolean containsKey(K key) {
        return cache.containsKey(key);
    }

    public int size() {
        return cache.size();
    }
}