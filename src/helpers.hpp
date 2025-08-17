#pragma once

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>

#define fatal(FMT, ...) do { \
    fprintf(stderr, "[FATAL: %s:%d:%s]: " FMT "\n", \
        __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    __builtin_debugtrap(); \
    exit(EXIT_FAILURE); \
} while (0)

#define check_vk_result(ERR) do { \
    if (ERR != VK_SUCCESS) { \
        fprintf(stderr, "[vulkan:%s:%d:%s] Error: VkResult = %d\n", __FILE__, __LINE__, __func__, ERR); \
        if (ERR < 0) abort(); \
    } \
} while (0)
