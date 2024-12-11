#pragma once

#include "free_list.h"
#include <atomic>
#include <cstddef>
#include <set>
#include <unordered_map>

namespace x {
namespace memory {

class pool {
public:
  static void *alloc(size_t);
  static void dealloc(void *);
  static void dealloc_all();
  static size_t real_alloc_size(size_t s);
  static size_t real_alloc_size(void *);

protected:
  static void lock();
  static void unlock();
  static void *memory_pool_alloc(size_t);
  static void *malloc_alloc(size_t);
  static void *chunk_alloc(size_t);

  static std::unordered_map<void *, size_t> block_size_info;
  static std::set<void *> malloc_info;
  static std::atomic<bool> lock_;
  static char *chunk;
  static size_t chunk_size;
  static FreeList<24, 8> free_list;
};
}; // namespace memory
}; // namespace x
xxx@xxx:~/projects/memory_poll$ cat memory_pool.cpp

#include "memory_pool.h"
#include "free_list.h"
#include <assert.h>
#include <cstddef>
#include <cstdlib>
#include <set>

namespace x {
namespace memory {

std::unordered_map<void *, size_t> pool::block_size_info;
std::set<void *> pool::malloc_info;
std::atomic<bool> pool::lock_ = ATOMIC_VAR_INIT(true);
char *pool::chunk = nullptr;
size_t pool::chunk_size = 0;
FreeList<24, 8> pool::free_list;

inline void pool::lock() {
  while (!lock_.exchange(false, std::memory_order_acquire)) {
  }
}

inline void pool::unlock() { lock_.store(true, std::memory_order_release); }

// 小于256 则 为 8的倍数 ， 大于256则是s
size_t pool::real_alloc_size(size_t s) {
  if (s <= free_list.max_group_bytes()) {
    return free_list.real_size(s);
  }
  return s;
}

// 不在池中则为0
size_t pool::real_alloc_size(void *p) {
  lock();
  auto it = block_size_info.find(p);
  auto size = (it == block_size_info.end()) ? 0 : it->second;
  unlock();
  return size;
}

// 小于256则池分配，大于则malloc分配
void *pool::alloc(size_t s) {
  void *ret = nullptr;

  if (s <= free_list.max_group_bytes()) {
    ret = memory_pool_alloc(s);
  } else {
    ret = malloc_alloc(s);
  }

  return ret;
}

// 从链表中取一个，没有则池分配一个
void *pool::memory_pool_alloc(size_t s) {
  lock();
  auto *ret = free_list.pop(s);

  if (ret == nullptr) {
    ret = chunk_alloc(s);
  }
  block_size_info[ret] = real_alloc_size(s);
  unlock();
  return ret;
}

// malloc分配
void *pool::malloc_alloc(size_t s) {
  auto *ret = ::malloc(s);

  lock();
  malloc_info.insert(ret);
  block_size_info[ret] = real_alloc_size(s);
  unlock();
  return ret;
}

void *pool::chunk_alloc(size_t s) {
  auto one_node_bytes = free_list.real_size(s);
  void *ret = nullptr;

  // 至少一个 则 给用户
  if (chunk_size >= one_node_bytes) {
    ret = chunk;
    chunk += one_node_bytes;
    chunk_size -= one_node_bytes;

    int i = 0;
    // 剩余至多 5个从池放入链表中
    while (chunk_size >= one_node_bytes && i < 5) {
      free_list.insert(chunk, one_node_bytes);
      chunk += one_node_bytes;
      chunk_size -= one_node_bytes;
      i++;
    }
  } else {
    // 没有一个则malloc分配给池
    // 把池剩余部分放入链表

    // 剩余大于min，则向下取整 最近的链表
    if (chunk_size > free_list.min_group_bytes()) {
      auto rest =
          free_list.real_size(chunk_size - free_list.min_group_bytes() + 1);
      free_list.insert(chunk, rest);
    } else if (chunk_size == free_list.min_group_bytes()) {
      // 剩余等于min，则直接放入
      free_list.insert(chunk, chunk_size);
    }

    // 分配新内存 ， 产生不大于min的内存碎片
    chunk = (char *)::malloc(one_node_bytes * 11);
    malloc_info.insert(chunk);
    ret = chunk;
    chunk += one_node_bytes;
    chunk_size = 10 * one_node_bytes;
  }

  return ret;
}

// 释放所有malloc的内存，清空内存池
void pool::dealloc_all() {
  lock();
  block_size_info.clear();
  chunk = nullptr;
  chunk_size = 0;
  free_list.clear();

  for (auto &i : malloc_info) {
    free(i);
  }

  malloc_info.clear();

  unlock();
}

void pool::dealloc(void *p) {

  lock();
  auto it = block_size_info.find(p);
  if (it != block_size_info.end()) {
    unlock();
    return;
  }

  if (it->second <= free_list.max_group_bytes()) {
    free_list.insert(p, it->second);
    block_size_info.erase(it);

    unlock();
  } else {
    malloc_info.erase(p);
    unlock();
    free(p);
  }
}

}; // namespace memory
}; // namespace x
