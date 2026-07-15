#ifndef LRU_LIST_H
#define LRU_LIST_H

#include <string>
#include <atomic>

struct LRUNode {
    std::string key;
    LRUNode* prev = nullptr;
    LRUNode* next = nullptr;
    std::atomic<uint64_t> lastLruUpdate{0};

    explicit LRUNode(const std::string& k) : key(k) {}
};

class LRUList {
public:
    LRUList();
    ~LRUList();

    
    LRUList(const LRUList&) = delete;
    LRUList& operator=(const LRUList&) = delete;

    LRUNode* pushFront(const std::string& key);
    void moveToFront(LRUNode* node);
    LRUNode* popBack();
    void remove(LRUNode* node);

    bool isEmpty() const;

private:
    LRUNode* head_;
    LRUNode* tail_;
};

#endif 
