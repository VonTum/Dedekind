#pragma once

#include <cstdint>

void* aligned_malloc(std::size_t size, std::size_t align);
void aligned_free(void* ptr);
