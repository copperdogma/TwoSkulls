#pragma once
#include <cstdint>
struct sdmmc_card_t { struct { uint32_t capacity, sector_size; } csd; };
