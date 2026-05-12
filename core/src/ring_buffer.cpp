#include "ring_buffer.h"
#include <bit>        // std::has_single_bit, std::bit_ceil
#include <stdexcept>  // std::invalid_argument, std::bad_alloc
#include <sys/mman.h>

namespace socketspy::core {

// ---------------------------------------------------------------------------
// RingBufferImpl
// ---------------------------------------------------------------------------

RingBufferImpl* RingBufferImpl::create(size_t capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("RingBuffer capacity must be > 0");
    }

    // Round up to the next power of two so the mask trick is valid.
    if (!std::has_single_bit(capacity)) {
        capacity = std::bit_ceil(capacity);
    }

    const size_t storage_bytes = capacity * sizeof(CanFrame);

    // Anonymous mmap gives us page-aligned, zero-initialised memory and lets
    // the OS reclaim physical pages that were never written (sparse behaviour).
    void* raw = ::mmap(nullptr, storage_bytes,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);
    if (raw == MAP_FAILED) {
        throw std::bad_alloc();
    }

    // The impl struct itself is small; plain new is fine here — it holds no
    // large inline data.
    auto* impl = new RingBufferImpl{};
    impl->storage  = static_cast<CanFrame*>(raw);
    impl->capacity = capacity;
    impl->mask     = capacity - 1;
    // head and tail are value-initialised to 0 by the aggregate constructor
    // in CacheLinePadded, but reset explicitly for clarity.
    impl->head.value.store(0, std::memory_order_relaxed);
    impl->tail.value.store(0, std::memory_order_relaxed);

    return impl;
}

void RingBufferImpl::destroy(RingBufferImpl* impl) {
    if (!impl) return;
    if (impl->storage) {
        ::munmap(impl->storage, impl->capacity * sizeof(CanFrame));
    }
    delete impl;
}

// ---------------------------------------------------------------------------
// RingBuffer — public interface (pimpl forwards to RingBufferImpl)
// ---------------------------------------------------------------------------

RingBuffer::RingBuffer(size_t capacity)
    : impl_(RingBufferImpl::create(capacity)) {}

RingBuffer::~RingBuffer() {
    RingBufferImpl::destroy(static_cast<RingBufferImpl*>(impl_));
}

// push() — called only from the producer thread.
//
// Memory ordering rationale:
//   - tail is loaded with acquire so we observe all consumer stores that
//     happened before the tail advance (prevents the producer from overwriting
//     a slot the consumer has not yet finished reading).
//   - head is stored with release so the consumer's subsequent acquire-load
//     of head sees the completed frame write in storage[].
bool RingBuffer::push(const CanFrame& frame) noexcept {
    auto* impl = static_cast<RingBufferImpl*>(impl_);

    const size_t h = impl->head.value.load(std::memory_order_relaxed);
    const size_t t = impl->tail.value.load(std::memory_order_acquire);

    if ((h - t) == impl->capacity) {
        return false;  // buffer full
    }

    impl->storage[h & impl->mask] = frame;

    // Release: makes the frame write visible to the consumer before the
    // updated head is seen.
    impl->head.value.store(h + 1, std::memory_order_release);
    return true;
}

// pop() — called only from the consumer thread.
//
// Memory ordering rationale:
//   - head is loaded with acquire so we observe the frame written by the
//     producer before it advanced head.
//   - tail is stored with release so a producer's acquire-load of tail sees
//     the slot is free once the consumer has finished reading it.
bool RingBuffer::pop(CanFrame& frame) noexcept {
    auto* impl = static_cast<RingBufferImpl*>(impl_);

    const size_t t = impl->tail.value.load(std::memory_order_relaxed);
    const size_t h = impl->head.value.load(std::memory_order_acquire);

    if (h == t) {
        return false;  // buffer empty
    }

    frame = impl->storage[t & impl->mask];

    // Release: makes the slot reusable by the producer.
    impl->tail.value.store(t + 1, std::memory_order_release);
    return true;
}

// ---------------------------------------------------------------------------
// Approximate stats — suitable for dashboards/logging, not synchronisation.
// ---------------------------------------------------------------------------

size_t RingBuffer::capacity() const noexcept {
    return static_cast<RingBufferImpl*>(impl_)->capacity;
}

size_t RingBuffer::size() const noexcept {
    auto* impl = static_cast<RingBufferImpl*>(impl_);
    const size_t h = impl->head.value.load(std::memory_order_relaxed);
    const size_t t = impl->tail.value.load(std::memory_order_relaxed);
    return h - t;
}

bool RingBuffer::empty() const noexcept {
    auto* impl = static_cast<RingBufferImpl*>(impl_);
    return impl->head.value.load(std::memory_order_relaxed) ==
           impl->tail.value.load(std::memory_order_relaxed);
}

bool RingBuffer::full() const noexcept {
    auto* impl = static_cast<RingBufferImpl*>(impl_);
    const size_t h = impl->head.value.load(std::memory_order_relaxed);
    const size_t t = impl->tail.value.load(std::memory_order_relaxed);
    return (h - t) == impl->capacity;
}

} // namespace socketspy::core
