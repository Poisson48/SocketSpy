#pragma once
#include "cancore.h"
#include <atomic>
#include <cstddef>

namespace socketspy::core {

// Each atomic counter lives on its own cache line to prevent false sharing
// between the producer (writes head) and consumer (writes tail).
struct alignas(64) CacheLinePadded {
    std::atomic<size_t> value{0};
    // Pad to exactly 64 bytes; the static_assert below enforces this.
    char _pad[64 - sizeof(std::atomic<size_t>)];
};
static_assert(sizeof(CacheLinePadded) == 64,
              "CacheLinePadded must be exactly one cache line");

struct RingBufferImpl {
    CanFrame* storage;   // mmap-backed frame array, capacity elements
    size_t    capacity;  // always a power of 2
    size_t    mask;      // capacity - 1, used for index wrapping

    // head is owned by the producer; tail is owned by the consumer.
    // Keeping them on separate cache lines avoids write-invalidation traffic.
    CacheLinePadded head;
    CacheLinePadded tail;

    // Allocate and initialise a new impl on the heap; storage is mmap-backed.
    static RingBufferImpl* create(size_t capacity);

    // Release the mmap'd storage and delete the impl.
    static void destroy(RingBufferImpl* impl);
};

} // namespace socketspy::core
