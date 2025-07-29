#ifndef MPMC_QUEUE_H
#define MPMC_QUEUE_H

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <thread>
#include <type_traits>

namespace glimpse {

const std::uint16_t CACHE_LINE_SIZE = 64;

template<typename T>
class MPMCQueue {
public:
    MPMCQueue(std::uint32_t size) : 
            _size(size),
            _mask(size - 1),
            _read_cursor(0),
            _write_cursor(0),
            _buffer(static_cast<Slot*>(std::malloc(sizeof(Slot) * size))) {
        
        // assert size is power of 2
        assert(size > 0 && (size & (size - 1)) == 0 && "size must be power of 2");

        if(!_buffer) throw std::bad_alloc();

        for (std::uint32_t i = 0; i < size; ++i) {
            _buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~MPMCQueue() {
        if(!std::is_trivially_destructible<T>::value) {
            std::uint32_t consumer_cursor = _read_cursor;
            std::uint32_t producer_cursor = _write_cursor;
            while(consumer_cursor != producer_cursor) {
                _buffer[consumer_cursor].~Slot();
                if(++consumer_cursor == _size) {
                    consumer_cursor = 0;
                }
            }
        }
    }

    /*
        checks if slot sequence number is ready
        if it is then try and claim it
        if claim succeeds iterate producer cursor
        write to slot 
        return true

        for now if fails just spin retry on new producer index
    */
    template <class... Args>
    void write(Args&&... data_args) {
        while(true) {
            std::uint32_t write_idx = 
                _write_cursor.load(std::memory_order_acquire);
            // & works as an efficient version of % since the buffer size is power of 2 
            Slot& slot = _buffer[write_idx & _mask];
            std::int32_t diff = slot.sequence.load(std::memory_order_acquire) - write_idx;

            if(diff == 0) {
                if(_write_cursor.compare_exchange_weak(
                        write_idx, 
                        write_idx + 1, 
                        std::memory_order_acquire,
                        std::memory_order_relaxed)) {
                    new (&slot.data) T(std::forward<Args>(data_args)...);  
                    slot.sequence.store(write_idx + 1, std::memory_order_release);
                    return;
                }
            } else {
                // slot isnt consumed yet
                std::this_thread::yield();
            }
        }
    }

    T read() {
        while(true) {
            std::uint32_t read_idx = 
                _read_cursor.load(std::memory_order_acquire);
            Slot& slot = _buffer[read_idx & _mask];
            std::int32_t diff = slot.sequence.load(std::memory_order_acquire) - read_idx;

            if(diff == 1) {
                if(_read_cursor.compare_exchange_weak(
                            read_idx, 
                            read_idx + 1, 
                            std::memory_order_acquire,
                            std::memory_order_relaxed)) {
                        T slot_data = std::move(slot.data);
                        
                        slot.sequence.store(read_idx + _size, std::memory_order_release);

                        return slot_data;
                    }
            } else {
                // slot isnt produced to yet
                std::this_thread::yield();
            }
        }
    }

private:
    struct alignas(CACHE_LINE_SIZE) Slot {
        T data;
        /*
            sequence number scenarios:
            sequence = index               : slot is ready for writing
            sequence = index + 1           : slot is ready for reading
            sequence = index + buffer size : slot has been read
        */
        std::atomic<uint32_t> sequence;
    };

    Slot* _buffer;
    std::uint32_t _size;
    std::uint32_t _mask;
    // assumes cache line is 64 bytes but this varies depending on a computer's hardware :p
    alignas(CACHE_LINE_SIZE) std::atomic<std::uint32_t> _read_cursor; 
    alignas(CACHE_LINE_SIZE) std::atomic<std::uint32_t> _write_cursor;
};

}

#endif 