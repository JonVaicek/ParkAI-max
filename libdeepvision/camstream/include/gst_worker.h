#ifndef GST_CHILD_WORKER_H
#define GST_CHILD_WORKER_H


#include <atomic>
#include <cstdint>

static constexpr uint32_t SHM_MAGIC = 0x4652414D; // 'FRAM'

constexpr uint64_t EVT_CHILD_STARTED  = 1ull << 0;
constexpr uint64_t EVT_PIPELINE_EXIT  = 1ull << 1;
constexpr uint64_t EVT_MMSH_COMPLETE  = 1ull << 2;



#endif