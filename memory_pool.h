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
