//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer.cpp
//
// Identification: src/buffer/lru_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_replacer.h"

namespace bustub {

LRUReplacer::LRUReplacer(size_t num_pages) {}

LRUReplacer::~LRUReplacer() = default;

bool LRUReplacer::Victim(frame_id_t *frame_id) {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  if (lru_list_.empty()) {
    return false;
  }
  *frame_id = lru_list_.front();
  auto first = lru_list_.begin();
  lru_map_.erase(*first);
  lru_list_.erase(first);
  return true;
}

void LRUReplacer::Pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  auto find_result = lru_map_.find(frame_id);
  if (find_result != lru_map_.end()) {
    lru_list_.erase(find_result->second);
    lru_map_.erase(find_result);
  }
}

void LRUReplacer::Unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  if (lru_list_.size() >= num_pages_) {
    return;
  }
  auto find_result = lru_map_.find(frame_id);
  if (find_result != lru_map_.end()) {
    return;
  }
  lru_list_.push_back(frame_id);
  lru_map_.emplace(frame_id, std::prev(lru_list_.end(), 1));
}

size_t LRUReplacer::Size() {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  return lru_list_.size();
}
}  // namespace bustub
