#include "lru_list.h"

LRUList::LRUList() {
    head_ = new LRUNode(""); 
    tail_ = new LRUNode(""); 
    head_->next = tail_;
    tail_->prev = head_;
}

LRUList::~LRUList() {
    LRUNode* curr = head_;
    while (curr != nullptr) {
        LRUNode* next = curr->next;
        delete curr;
        curr = next;
    }
}

LRUNode* LRUList::pushFront(const std::string& key) {
    LRUNode* newNode = new LRUNode(key);
    newNode->next = head_->next;
    newNode->prev = head_;
    head_->next->prev = newNode;
    head_->next = newNode;
    return newNode;
}

void LRUList::moveToFront(LRUNode* node) {
    if (node == nullptr || head_->next == node) {
        return;
    }
    
    node->prev->next = node->next;
    node->next->prev = node->prev;

    
    node->next = head_->next;
    node->prev = head_;
    head_->next->prev = node;
    head_->next = node;
}

LRUNode* LRUList::popBack() {
    if (isEmpty()) {
        return nullptr;
    }
    LRUNode* last = tail_->prev;
    remove(last);
    return last;
}

void LRUList::remove(LRUNode* node) {
    if (node == nullptr) return;
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

bool LRUList::isEmpty() const {
    return head_->next == tail_;
}
