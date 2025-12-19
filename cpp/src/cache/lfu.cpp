#include "cache/lfu.h"
#include <algorithm>

LFUEvictor::LFUEvictor(size_t capacity) : capacity_(capacity) {}

void LFUEvictor::put(const std::string& key, const CacheEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(key);
    if (it != nodes_.end()) {
        // update entry and increase frequency
        it->second->entry = entry;
        touch(it->second);
        return;
    }

    if (nodes_.size() >= capacity_) {
        evict_one();
    }

    // insert new node with freq 1
    Node node{key, entry, 1};
    freq_lists_[1].push_front(node);
    nodes_[key] = freq_lists_[1].begin();
    min_freq_ = 1;
}

bool LFUEvictor::get(const std::string& key, CacheEntry& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) return false;
    out = it->second->entry;
    touch(it->second);
    return true;
}

bool LFUEvictor::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) return false;
    size_t freq = it->second->freq;
    auto &lst = freq_lists_[freq];
    lst.erase(it->second);
    if (lst.empty()) freq_lists_.erase(freq);
    nodes_.erase(it);
    // adjust min_freq_
    if (nodes_.empty()) min_freq_ = 0; else if (freq == min_freq_ && freq_lists_.find(freq) == freq_lists_.end()) min_freq_ = freq+1; // conservative
    return true;
}

size_t LFUEvictor::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

void LFUEvictor::touch(std::list<Node>::iterator it) {
    size_t freq = it->freq;
    Node node = *it; // copy
    auto &lst = freq_lists_[freq];
    lst.erase(it);
    if (lst.empty()) freq_lists_.erase(freq);

    node.freq++;
    freq_lists_[node.freq].push_front(node);
    nodes_[node.key] = freq_lists_[node.freq].begin();

    if (freq_lists_.find(min_freq_) == freq_lists_.end()) min_freq_ = node.freq;
}

void LFUEvictor::evict_one() {
    if (nodes_.empty()) return;
    auto it = freq_lists_.find(min_freq_);
    if (it == freq_lists_.end()) {
        // find next min
        size_t m = SIZE_MAX;
        for (auto &p : freq_lists_) m = std::min(m, p.first);
        it = freq_lists_.find(m);
        if (it == freq_lists_.end()) return;
        min_freq_ = it->first;
    }
    auto &lst = it->second;
    auto node_it = --lst.end(); // least recently used within min_freq (back)
    nodes_.erase(node_it->key);
    lst.erase(node_it);
    if (lst.empty()) freq_lists_.erase(it->first);
}