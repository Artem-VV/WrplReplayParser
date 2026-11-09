#pragma once
#include "memory/dag_genMemAlloc.h"
#include <memory_resource>
class StateRewinder;


/// ParserState allocator backed by the system allocator.
class StateAllocator : public std::pmr::memory_resource {
  struct StateDagAlloc : public IMemAlloc {
    void destroy() override {}
    bool isEmpty() override { return false; }
    size_t getSize(void *p) override { return 0; }
    void *alloc(size_t sz) override {
      return malloc(sz);
    }
    void *tryAlloc(size_t sz) override {
      return malloc(sz);
    }
    void *allocAligned(size_t sz, size_t alignment) override {
#if defined(_WIN32)
      return _aligned_malloc(sz, alignment);
#else
      return ::aligned_alloc(alignment, sz);
#endif
    }
    bool resizeInplace(void *p, size_t sz) override {
      return false;
    }
    void *realloc(void *p, size_t sz) override {
      return ::realloc(p, sz);
    }
    void free(void *p) override {
      return ::free(p);
    }
    void freeAligned(void *p) override {
#if defined(_WIN32)
      _aligned_free(p);
#else
      return ::free(p);
#endif
    }
  };

  StateDagAlloc dagAlloc;

  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    return ::malloc(bytes);
  }

  void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) override {
    return ::free(p);
  }

  bool do_is_equal(std::pmr::memory_resource const &other) const noexcept override { return this == &other; }
  friend StateRewinder;

public:
  using DagAllocType = StateDagAlloc *;

  template<class T, class... Args>
  T *_new(Args &&...args) {
    void *mem = do_allocate(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }

  template<class T>
  void _delete(const T *ptr) {
    if (ptr) {
      if (!ptr)
        return;
      std::destroy_at(ptr);
      do_deallocate(static_cast<void *>(const_cast<T *>(ptr)), sizeof(T), alignof(T));
    }
  }
  StateAllocator() = default;
  ~StateAllocator() override = default;

  DagAllocType getMem() { return &dagAlloc; }
};

/// see _in_destruction_state, represents the allocator of the currently being destroyed state
inline thread_local StateAllocator *_in_destruction_allocator{};
