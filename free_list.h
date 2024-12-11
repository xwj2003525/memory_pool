#pragma once

#include <assert.h>
#include <cstddef>
#include <iostream>
#include <iterator>
namespace x {
namespace memory {

template <size_t Groups, size_t BytesInc> class FreeList {
public:
  static_assert(Groups >= 1 && Groups <= 64, "Groups must be between 1 and 64");
  static_assert(BytesInc >= 1 && BytesInc <= 64,
                "BytesInc must be between 1 and 64");

  constexpr size_t min_group_bytes() { return BytesInc; }
  constexpr size_t max_group_bytes() { return Groups * BytesInc; }

  void clear() {
    for (size_t i = 0; i < Groups; i++)
      free_list[i] = nullptr;
  }

  FreeList() { clear(); }

  void insert(void *p, size_t s) {
    assert_in_range(s);
    assert_multiple_size(s);

    auto i = index(s);

    auto *node = (ListNode *)(p);
    node->next = free_list[i];
    free_list[i] = node;
  }

  void *pop(size_t s) {
    assert_in_range(s);

    auto i = index(s);
    auto ret = free_list[i];

    if (ret) {
      free_list[i] = ret->next;
    }

    return ret;
  }

  size_t real_size(size_t s) {
    assert_in_range(s);
    return BytesInc * (index(s) + 1);
  }

protected:
  void assert_multiple_size(size_t s) { assert(s % BytesInc == 0); }

  void assert_in_range(size_t s) { assert(s >= 1 && s <= Groups * BytesInc); }

  size_t constexpr index(size_t s) const {
    if (s == 0)
      return 0;
    return (s - 1) / BytesInc;
  }

  struct ListNode {
    struct ListNode *next = nullptr;
  };

  ListNode *free_list[Groups];
};

}; // namespace memory
}; // namespace x
