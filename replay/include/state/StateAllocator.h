#pragma once
#include "memory/dag_genMemAlloc.h"
#include <memory_resource>
#include <new>
class StateRewinder;


/// ParserState allocator backed by a dedicated mimalloc heap when enabled.
class StateAllocator : public std::pmr::memory_resource {
  struct StateDagAlloc : public IMemAlloc {
#if USE_MI_HEAP == 1
    mi_heap_t *heap = mi_heap_new();

    StateDagAlloc() {
      if (!heap)
        throw std::bad_alloc();
    }
    StateDagAlloc(const StateDagAlloc &) = delete;
    StateDagAlloc &operator=(const StateDagAlloc &) = delete;
    ~StateDagAlloc() { mi_heap_destroy(heap); }
#endif

    void destroy() override {}
    bool isEmpty() override { return false; }
    size_t getSize(void *p) override { return 0; }
    void *alloc(size_t sz) override {
#if USE_MI_HEAP == 1
      return mi_heap_malloc(heap, sz);
#else
      return malloc(sz);
#endif
    }
    void *tryAlloc(size_t sz) override {
      return alloc(sz);
    }
    void *allocAligned(size_t sz, size_t alignment) override {
#if USE_MI_HEAP == 1
      return mi_heap_malloc_aligned(heap, sz, alignment);
#else
      return G_ALLOC.allocAligned(sz, alignment);
#endif
    }
    bool resizeInplace(void *p, size_t sz) override {
#if USE_MI_HEAP == 1
      return mi_expand(p, sz) != nullptr;
#else
      return false;
#endif
    }
    void *realloc(void *p, size_t sz) override {
#if USE_MI_HEAP == 1
      return mi_heap_realloc(heap, p, sz);
#else
      return ::realloc(p, sz);
#endif
    }
    void free(void *p) override {
#if USE_MI_HEAP == 1
      mi_free(p);
#else
      return ::free(p);
#endif
    }
    void freeAligned(void *p) override {
#if USE_MI_HEAP == 1
      mi_free(p);
#else
      G_ALLOC.freeAligned(p);
#endif
    }
  };

  StateDagAlloc dagAlloc;

  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (void *p = dagAlloc.allocAligned(bytes, alignment))
      return p;
    throw std::bad_alloc();
  }

  void do_deallocate(void *p, std::size_t, std::size_t) override {
    dagAlloc.freeAligned(p);
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
#if USE_MI_HEAP == 1
  mi_heap_t *get_heap_ptr() { return dagAlloc.heap; }
#endif
};

/// see _in_destruction_state, represents the allocator of the currently being destroyed state
inline thread_local StateAllocator *_in_destruction_allocator{};
