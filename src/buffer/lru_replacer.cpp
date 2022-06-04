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

// 删除与Replacer跟踪的所有元素相比最近被访问次数最少的对象，将其内容存储在输出参数中并返回True。如果Replacer是空的，则返回False。
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

// 这个方法应该在一个页面被钉在BufferPoolManager的一个框架上之后被调用。它应该从LRUReplacer中移除包含钉住的页面的框架。
void LRUReplacer::Pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  auto find_result = lru_map_.find(frame_id);
  if (find_result != lru_map_.end()) {
    lru_list_.erase(find_result->second);
    lru_map_.erase(find_result);
  }
}

//  当一个页面的pin_count变成0的时候，这个方法应该被调用，这个方法应该把包含未被钉住的页面的框架添加到LRUReplacer中。
void LRUReplacer::Unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock_guard(mutex_);
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
