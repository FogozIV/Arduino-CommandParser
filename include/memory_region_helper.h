//
// Created by fogoz on 10/05/2025.
//

#ifndef MEMORY_REGION_HELPER_H
#define MEMORY_REGION_HELPER_H
#include <cstdint>

inline bool isDMAMEM(const void* ptr) {
    return (uint32_t)ptr >= 0x20200000 && (uint32_t)ptr < 0x20280000;
}

inline bool isRAM1(const void* ptr) {
    return (uint32_t)ptr >= 0x20000000 && (uint32_t)ptr < 0x20080000;
}
#endif //MEMORY_REGION_HELPER_H
