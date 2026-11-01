#pragma once
constexpr int GPIO_NUM_15 = 15, GPIO_NUM_2 = 2, GPIO_NUM_13 = 13, GPIO_PULLUP_ONLY = 1;
inline unsigned gpio_pullup_mask = 0;
inline int gpio_set_pull_mode(int pin, int mode) {
    if (mode == GPIO_PULLUP_ONLY) gpio_pullup_mask |= 1u << pin;
    return 0;
}
