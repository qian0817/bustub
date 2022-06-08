//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager_instance.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager_instance.h"

#include "common/macros.h"

namespace bustub {

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager,
                                                     LogManager *log_manager)
    : BufferPoolManagerInstance(pool_size, 1, 0, disk_manager, log_manager) {}

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, uint32_t num_instances, uint32_t instance_index,
                                                     DiskManager *disk_manager, LogManager *log_manager)
    : pool_size_(pool_size),
      num_instances_(num_instances),
      instance_index_(instance_index),
      next_page_id_(instance_index),
      disk_manager_(disk_manager),
      log_manager_(log_manager) {
  BUSTUB_ASSERT(num_instances > 0, "If BPI is not part of a pool, then the pool size should just be 1");
  BUSTUB_ASSERT(
      instance_index < num_instances,
      "BPI index cannot be greater than the number of BPIs in the pool. In non-parallel case, index should just be 1.");
  // We allocate a consecutive memory space for the buffer pool.
  pages_ = new Page[pool_size_];
  replacer_ = new LRUReplacer(pool_size);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManagerInstance::~BufferPoolManagerInstance() {
  delete[] pages_;
  delete replacer_;
}

bool BufferPoolManagerInstance::FlushPgImp(page_id_t page_id) {
  std::lock_guard<std::mutex> guard(latch_);
  // Make sure you call DiskManager::WritePage!
  auto find_result = page_table_.find(page_id);
  if (find_result == page_table_.end()) {
    return false;
  }
  auto page = &pages_[find_result->second];

  disk_manager_->WritePage(page_id, page->GetData());
  page->is_dirty_ = false;
  return true;
}

void BufferPoolManagerInstance::FlushAllPgsImp() {
  std::lock_guard<std::mutex> guard(latch_);
  // You can do it!
  for (size_t i = 0; i < pool_size_; ++i) {
    auto page = &pages_[i];
    if (page->page_id_ != INVALID_PAGE_ID && page->is_dirty_) {
      disk_manager_->WritePage(page->page_id_, page->GetData());
      page->is_dirty_ = false;
    }
  }
}

Page *BufferPoolManagerInstance::NewPgImp(page_id_t *page_id) {
  std::lock_guard<std::mutex> guard(latch_);
  // 0.   Make sure you call AllocatePage!
  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  // 4.   Set the page ID output parameter. Return a pointer to P.
  auto all_pinned = true;
  for (size_t i = 0; i < pool_size_; ++i) {
    auto current_page = &pages_[i];
    if (current_page->GetPinCount() == 0) {
      all_pinned = false;
      break;
    }
  }
  if (all_pinned) {
    return nullptr;
  }
  if (!free_list_.empty()) {
    auto allocate_page_id = AllocatePage();
    auto frame_id = free_list_.back();
    free_list_.pop_back();
    auto page = &pages_[frame_id];
    page->page_id_ = allocate_page_id;
    page->pin_count_ = 1;
    replacer_->Pin(frame_id);
    page_table_.emplace(allocate_page_id, frame_id);
    *page_id = allocate_page_id;
    return page;
  }
  frame_id_t frame_id;
  auto victim_result = replacer_->Victim(&frame_id);
  if (!victim_result) {
    return nullptr;
  }
  auto page = &pages_[frame_id];
  if (page->is_dirty_) {
    disk_manager_->WritePage(page->GetPageId(), page->GetData());
    page->is_dirty_ = false;
  }
  page_table_.erase(page->GetPageId());
  auto allocate_page_id = AllocatePage();
  replacer_->Pin(frame_id);
  page->pin_count_ = 1;
  page->ResetMemory();
  page->page_id_ = allocate_page_id;
  page_table_[allocate_page_id] = frame_id;
  *page_id = allocate_page_id;
  return page;
}

Page *BufferPoolManagerInstance::FetchPgImp(page_id_t page_id) {
  std::lock_guard<std::mutex> guard(latch_);
  // 1.     Search the page table for the requested page (P).
  // 1.1    If P exists, pin it and return it immediately.
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  // 2.     If R is dirty, write it back to the disk.
  // 3.     Delete R from the page table and insert P.
  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.
  auto find_result = page_table_.find(page_id);
  if (find_result != page_table_.end()) {
    auto frame_id = find_result->second;
    auto page = &pages_[frame_id];
    if (page->GetPinCount() == 0) {
      replacer_->Pin(frame_id);
    }
    page->pin_count_++;
    return page;
  }
  if (!free_list_.empty()) {
    auto frame_id = free_list_.back();
    free_list_.pop_back();
    auto page = &pages_[frame_id];
    page->page_id_ = page_id;
    page->pin_count_ = 1;
    replacer_->Pin(frame_id);
    page_table_.emplace(page_id, frame_id);
    return page;
  }
  frame_id_t frame_id;
  auto victim_result = replacer_->Victim(&frame_id);
  if (!victim_result) {
    return nullptr;
  }
  auto page = &pages_[frame_id];
  if (page->is_dirty_) {
    disk_manager_->WritePage(page->GetPageId(), page->GetData());
    page->is_dirty_ = false;
  }
  page_table_.erase(page->GetPageId());
  replacer_->Pin(frame_id);
  page->pin_count_ = 1;
  page->ResetMemory();
  page->page_id_ = page_id;
  page_table_[page_id] = frame_id;
  disk_manager_->ReadPage(page_id, page->GetData());
  return page;
}

bool BufferPoolManagerInstance::DeletePgImp(page_id_t page_id) {
  std::lock_guard<std::mutex> guard(latch_);
  // 0.   Make sure you call DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  auto find_result = page_table_.find(page_id);
  if (find_result == page_table_.end()) {
    return true;
  }
  auto frame_id = find_result->second;
  auto page = &pages_[frame_id];
  if (page->pin_count_ != 0) {
    return false;
  }
  page->ResetMemory();
  page->pin_count_ = 0;
  page->is_dirty_ = false;
  page->page_id_ = INVALID_PAGE_ID;
  page_table_.erase(find_result);
  DeallocatePage(page_id);
  return true;
}

bool BufferPoolManagerInstance::UnpinPgImp(page_id_t page_id, bool is_dirty) {
  std::lock_guard<std::mutex> guard(latch_);
  auto find_result = page_table_.find(page_id);
  if (find_result == page_table_.end()) {
    return true;
  }
  auto frame_id = find_result->second;
  auto page = &pages_[frame_id];
  page->is_dirty_ = is_dirty;
  page->pin_count_--;
  if (page->GetPinCount() == 0) {
    replacer_->Unpin(frame_id);
  }
  return page->GetPinCount() > 0;
}

page_id_t BufferPoolManagerInstance::AllocatePage() {
  const page_id_t next_page_id = next_page_id_;
  next_page_id_ += num_instances_;
  ValidatePageId(next_page_id);
  return next_page_id;
}

void BufferPoolManagerInstance::ValidatePageId(const page_id_t page_id) const {
  assert(page_id % num_instances_ == instance_index_);  // allocated pages mod back to this BPI
}

}  // namespace bustub
