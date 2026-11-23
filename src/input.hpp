// SPDX-License-Identifier: GPL-3.0-or-later
// Controller report helpers adapted from RAC2 Contextual Wrench and Fire.
#pragma once

#include <cstdint>

namespace contextual {

enum Button : std::uint8_t { R2 = 2, Triangle = 16, Circle = 32, Cross = 64, Square = 128 };
constexpr std::uint8_t start = 8;
constexpr int report_size = 18;
constexpr int buttons = 1;
constexpr int circle_pressure = 11;

struct PadSample {
    std::uint8_t physical;
    bool start_down;

    bool down(Button button) const { return (physical & button) == 0; }
    bool opening_menu() const { return start_down || down(Triangle); }
};

inline PadSample sample(const std::uint8_t* report) {
    return {report[buttons], (report[0] & start) == 0};
}

// Upgrades keep the base item ID in the drawn hand slot.
constexpr bool is_weapon(int item) {
    return (item >= 22 && item <= 32) || (item >= 41 && item <= 45)
        || item == 12 || item == 14 || item == 16 || item == 17 || item == 18 || item == 37
        || item == 53 || item == 9;
}

constexpr bool is_foot_group(int group) {
    return group == 0 || group == 1 || group == 2 || group == 4 || group == 0xC;
}

} // namespace contextual
