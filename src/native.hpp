// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "levels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace gadgets {

using openrac::gaddr;
using openrac::Memory;

// Going Commando USA v1.01. Resolve native functions against the active overlay before calling them.
constexpr gaddr pad = 0x138180;
constexpr gaddr hero = 0x189E20;
constexpr gaddr hand_attachment_position = hero + 0xC80;
constexpr gaddr held_moby = hero + 0x1220;
constexpr gaddr held_trigger = hero + 0x1230;
constexpr gaddr melee_active = hero + 0x1239;
constexpr gaddr wrench_thrown = hero + 0x123A;
constexpr gaddr held_age = hero + 0x1240;
constexpr gaddr held_ready = hero + 0x1244;
constexpr gaddr held_item = hero + 0x1248;
constexpr gaddr cached_lock_target = hero + 0x1450;
constexpr gaddr vehicle = hero + 0x1830;
constexpr gaddr hero_moby = hero + 0x2290;
constexpr gaddr hero_state = hero + 0x2294;
constexpr gaddr hero_group = hero + 0x229C;
constexpr gaddr hero_body = hero + 0x22B4;
constexpr gaddr no_menu = hero + 0x22B7;
constexpr gaddr item_in_use = hero + 0x22B8;
constexpr gaddr no_item = hero + 0x22BC;
constexpr gaddr no_update = hero + 0x22BD;
constexpr gaddr item_tucked = hero + 0x22BF;
constexpr gaddr equip_request = hero + 0x22C8;
constexpr gaddr selected_item = hero + 0x22E4;
constexpr gaddr health = hero + 0x24CC;
constexpr gaddr raised_arm = hero + 0x24EC;
constexpr gaddr tractor_latch = hero + 0x5C0;
constexpr gaddr owned_items = 0x1A7AF8;
constexpr gaddr remembered_item = 0x1A7398;
constexpr gaddr moby_position = 0x10;
constexpr gaddr moby_state = 0x20;
constexpr gaddr moby_class_data = 0x24;
constexpr gaddr moby_drawn = 0x31;
constexpr gaddr moby_flags = 0x34;
constexpr gaddr moby_pvars = 0x68;
constexpr gaddr moby_collision = 0x98;
constexpr gaddr moby_class = 0xAA;
constexpr gaddr moby_uid = 0xB2;
constexpr gaddr pad_lock = pad + 0x1CC;
constexpr gaddr pad_held = pad + 0x1A0;
constexpr std::uint32_t circle = 0x20;
constexpr int wrench = 10;

enum class Kind { dynamo, tractor, thermanator, hypnomatic };

struct Adapter {
    Kind kind;
    int item;
    std::uint16_t hand_class;

    bool hold() const { return kind != Kind::dynamo; }
};

constexpr std::array adapters{
    Adapter{Kind::dynamo, 36, 0x825},
    Adapter{Kind::tractor, 46, 0xBC},
    Adapter{Kind::thermanator, 39, 0xFB0},
    Adapter{Kind::hypnomatic, 55, 0x950},
};

constexpr bool game_address(gaddr addr, unsigned size, unsigned alignment = 4) {
    return addr >= 0x100000 && addr <= 0x2000000 - size && addr % alignment == 0;
}

constexpr bool moby_address(gaddr addr) {
    return game_address(addr, 0x100, 16);
}

inline bool owned(Memory mem, int item) {
    return item == wrench || (item > 0 && item < 56 && mem.read<std::uint8_t>(owned_items + item) != 0);
}

inline gaddr pvars(Memory mem, gaddr moby, unsigned size) {
    const auto addr = mem.read<gaddr>(moby + moby_pvars);
    return game_address(addr, size) ? addr : 0;
}

inline std::uint8_t hand_state(Memory mem, gaddr hand) {
    return mem.read<std::uint8_t>(hand + moby_state);
}

struct Target {
    gaddr address = 0;
    std::uint16_t uid = 0;
    std::uint16_t type = 0;
    bool frozen = false;

    explicit operator bool() const { return address != 0; }
    bool operator==(const Target& other) const {
        return address == other.address && uid == other.uid && type == other.type && frozen == other.frozen;
    }
    bool operator!=(const Target& other) const { return !(*this == other); }
};

inline Target target_at(Memory mem, gaddr addr) {
    if (!moby_address(addr) || mem.read<std::uint8_t>(addr + moby_state) >= 0x80) {
        return {};
    }
    return {addr, mem.read<std::uint16_t>(addr + moby_uid), mem.read<std::uint16_t>(addr + moby_class)};
}

inline int class_kind(Memory mem, gaddr addr) {
    const auto row = mem.read<gaddr>(addr + moby_class_data);
    return game_address(row, 0x48) ? mem.read<std::int16_t>(row + 0x46) : -1;
}

struct MobyRange {
    gaddr first = 0;
    gaddr end = 0;
};

inline MobyRange moby_range(Memory mem, const Level& level) {
    if (!level.moby_base) {
        return {};
    }
    const auto first = mem.read<gaddr>(level.moby_base);
    const auto end = mem.read<gaddr>(level.moby_end);
    if (!moby_address(first) || end <= first || end > 0x2000000 || (end - first) % 0x100 != 0) {
        return {};
    }
    return {first, end};
}

// The collision routine publishes its last hit at +0x18..+0x7f. Preserve that result
// for the caller and leave the visit counter at +0x10 advancing normally.
class PreserveCollision {
public:
    PreserveCollision(Memory mem, gaddr address) : ptr_(mem.base() + address + 0x18) {
        std::memcpy(saved_.data(), ptr_, saved_.size());
    }
    ~PreserveCollision() { std::memcpy(ptr_, saved_.data(), saved_.size()); }

    PreserveCollision(const PreserveCollision&) = delete;
    PreserveCollision& operator=(const PreserveCollision&) = delete;

private:
    std::uint8_t* ptr_;
    std::array<std::uint8_t, 0x68> saved_;
};

template<typename Fn>
Fn active_function(openrac::Mod& mod, const char* symbol, gaddr address) {
    if (!address) {
        return nullptr;
    }
    const auto named = mod.function<Fn>(symbol);
    if (!named) {
        return nullptr;
    }
    const auto live = mod.function_at<Fn>(address);
    return live == named ? live : nullptr;
}

using Vec = std::array<float, 4>;
using Line = int (*)(gaddr, gaddr, int, gaddr, gaddr);

inline Target dynamo_target(openrac::Mod& mod, const Level& level) {
    const auto mem = mod.memory();
    const PreserveCollision collision(mem, level.collision_data);
    const auto search = active_function<gaddr (*)()>(mod, "func_00315C78", level.dynamo_search);
    const auto target = search ? target_at(mem, search()) : Target{};
    return target && (target.type == 0xCB5 || target.type == 0x12EA) && hand_state(mem, target.address) != 2
        ? target : Target{};
}

inline bool oozla_box_in_range(Memory mem, Target target) {
    const auto pv = pvars(mem, target.address, 0x8C);
    if (!pv || hand_state(mem, target.address) != 1) {
        return false;
    }
    const auto box = mem.read<Vec>(target.address + moby_position);
    const auto pos = mem.read<Vec>(hero + 0x80);
    const float reach = mem.read<float>(pv + 0x88);
    return std::isfinite(reach) && std::hypot(box[0] - pos[0], box[1] - pos[1], box[2] - pos[2]) < reach;
}

inline bool oozla_box(Target target) {
    return target.type == 0xACB || target.type == 0xACC;
}

struct TractorSelection {
    Target target;
    bool listed = false;
};

inline TractorSelection tractor_target(openrac::Mod& mod, const Level& level) {
    const auto mem = mod.memory();
    if (mem.read<gaddr>(tractor_latch) != 0) {
        return {};
    }
    const PreserveCollision collision(mem, level.collision_data);
    using Search = gaddr (*)(gaddr, int, int, float, float, float, float);
    const auto search = active_function<Search>(mod, "func_L00_002AE888", level.tractor_search);
    constexpr float yaw_cone = 0x1.0c1526p-1f;
    constexpr float pitch_cone = 0x1.657188p-1f;
    if (!search) {
        return {};
    }
    const float yaw = mem.read<float>(hero + 0x98);
    const auto target = target_at(mem, search(hero, 0x15, 0, 20.0f, yaw, yaw_cone, pitch_cone));
    const openrac::StackFrame frame(mod, 0x20);
    const auto visible = [&](gaddr addr) {
        auto from = mem.read<Vec>(addr + moby_position);
        from[2] += 0.5f;
        mem.write(frame.address(), from);
        const auto line = active_function<Line>(mod, "func_002761F8", level.collision);
        return line && line(frame.address(), hero + 0xE0, 2, addr, 0) == 0;
    };
    if (target && class_kind(mem, target.address) == 0x15
        && (!level.tractor_score || !oozla_box(target) || oozla_box_in_range(mem, target))) {
        return visible(target.address) ? TractorSelection{target, true} : TractorSelection{};
    }

    // The columns update eligibility every 16 ticks, and the native target list is built
    // from that flag. Preview an in-range box with the same aim scorer and LOS, then wait
    // for the game to list it before replay. Keep the stock flags and distance intact.
    if (!level.tractor_score || mem.read<std::uint8_t>(hero + 0x22C3) != 0) {
        return {};
    }
    using Score = float (*)(gaddr, float, float, gaddr, float, float);
    const auto range = moby_range(mem, level);
    const auto rejected = frame.address() + 0x10;
    TractorSelection best{};
    float best_score = 1e8f;
    for (auto addr = range.first; addr < range.end; addr += 0x100) {
        const auto candidate = target_at(mem, addr);
        if (!candidate || !oozla_box(candidate) || class_kind(mem, addr) != 0x15
            || mem.read<std::uint8_t>(addr + moby_drawn) == 0
            || (mem.read<std::uint16_t>(addr + moby_flags) & 2) || !oozla_box_in_range(mem, candidate)) {
            continue;
        }
        const auto score = active_function<Score>(mod, "func_L00_002AE388", level.tractor_score);
        if (!score) {
            return {};
        }
        const float value = score(addr, yaw, 20.0f, rejected, yaw_cone, pitch_cone);
        if (mem.read<int>(rejected) == 0 && value < best_score && visible(addr)) {
            best = {candidate, false};
            best_score = value;
        }
    }
    return best;
}

inline Target grid_target(Memory mem, gaddr addr) {
    auto target = target_at(mem, addr);
    const auto pv = target ? pvars(mem, addr, 0x18) : 0;
    if (!pv || class_kind(mem, addr) != 0x12 || mem.read<std::int16_t>(pv + 0x16) != 0) {
        return {};
    }
    target.frozen = mem.read<std::uint8_t>(pv + 0x14) != 0;
    return target;
}

inline Target thermanator_target(openrac::Mod& mod, const Level& level, gaddr hand, bool equipped) {
    const auto mem = mod.memory();
    const PreserveCollision collision(mem, level.collision_data);
    const openrac::StackFrame frame(mod, 0x50);
    const auto muzzle = frame.address(), velocity = muzzle + 0x10;
    const auto hit_pos = muzzle + 0x20, normal = muzzle + 0x30, hit_moby = muzzle + 0x40;
    // Attachment 0's world translation is the preview origin. Once the gadget is in hand,
    // use the stock joint-0 query to get its actual muzzle for the final recheck.
    if (equipped) {
        const auto joint = active_function<void (*)(gaddr, int, gaddr)>(mod, "func_0029FCC0", level.joint);
        if (!joint) {
            return {};
        }
        joint(hand, 0, muzzle);
    } else {
        mem.write(muzzle, mem.read<Vec>(hand_attachment_position));
    }
    const auto reach = active_function<float (*)(float)>(mod, "func_003004B0", level.reach);
    if (!reach) {
        return {};
    }
    const float range = reach(4.5f) + (mem.read<int>(hero_state) == 0x1D ? 6.5f : 0.0f);
    constexpr float least = 0x1.4f8b58p-17f;
    constexpr float gravity = 0x1.907f7p-9f;
    using Arc = void (*)(gaddr, gaddr, gaddr, float, float, float, float, gaddr);
    const auto arc = active_function<Arc>(mod, "func_002FDF40", level.arc);
    if (!arc) {
        return {};
    }
    arc(velocity, muzzle, 0, least, range, 0.25f, gravity, 0);
    using Hit = int (*)(gaddr, gaddr, gaddr, gaddr, gaddr, gaddr, gaddr, float);
    const auto hit = active_function<Hit>(mod, "func_002FE5A0", level.arc_hit);
    if (!hit) {
        return {};
    }
    mem.write<gaddr>(hit_moby, 0);
    hit(muzzle, velocity, hit_pos, normal, hand, hit_moby, 0, gravity);
    return grid_target(mem, mem.read<gaddr>(hit_moby));
}

inline Target receiver(Memory mem, gaddr addr) {
    const auto target = target_at(mem, addr);
    if (!target || target.type != 0xEE1 || mem.read<std::uint8_t>(addr + moby_drawn) == 0) {
        return {};
    }
    const auto pv = pvars(mem, addr, 0x3E8);
    return pv && (mem.read<std::uint8_t>(pv + 0x3B1) & 0x80) && mem.read<float>(pv + 0x20) > 0.0f
        ? target : Target{};
}

inline bool hypno_grounded(Memory mem) {
    return mem.read<int>(hero + 0x340) != 0 && mem.read<std::int16_t>(hero + 0x34E) == 0
        && mem.read<std::int16_t>(hero + 0x1E0) == 0 && mem.read<std::int16_t>(raised_arm) == 0;
}

// The stock Hypnomatic brackets its searches with the same nine class exclusions.
// Use private scratch so the stock hand updater can keep its own cached collider list.
class HypnoCollision {
public:
    HypnoCollision(Memory mem, const Level& level) : mem_(mem) {
        const auto range = moby_range(mem, level);
        for (auto addr = range.first; addr < range.end && count_ < saved_.size(); addr += 0x100) {
            const auto klass = mem_.read<std::uint16_t>(addr + moby_class);
            if (std::find(classes_.begin(), classes_.end(), klass) == classes_.end()) {
                continue;
            }
            const auto coll = addr + moby_collision;
            const auto pointer = mem_.read<gaddr>(coll);
            if (pointer) {
                saved_[count_++] = {coll, pointer};
                mem_.write<gaddr>(coll, 0);
            }
        }
    }
    ~HypnoCollision() {
        for (std::size_t i = 0; i < count_; ++i) {
            mem_.write(saved_[i].first, saved_[i].second);
        }
    }

    HypnoCollision(const HypnoCollision&) = delete;
    HypnoCollision& operator=(const HypnoCollision&) = delete;

private:
    static constexpr std::array classes_{0x1146, 0x1185, 0x119B, 0x11A4, 0x1204, 0x11FC, 0x120C, 0x1135, 0x1148};
    Memory mem_;
    std::array<std::pair<gaddr, gaddr>, 64> saved_{};
    std::size_t count_ = 0;
};

class FreshHypnoAim {
public:
    explicit FreshHypnoAim(Memory mem) : mem_(mem), saved_(mem.read<gaddr>(cached_lock_target)) {
        mem_.write<gaddr>(cached_lock_target, 0);
    }
    ~FreshHypnoAim() {
        mem_.write(cached_lock_target, saved_);
    }

    FreshHypnoAim(const FreshHypnoAim&) = delete;
    FreshHypnoAim& operator=(const FreshHypnoAim&) = delete;

private:
    Memory mem_;
    gaddr saved_;
};

inline Target hypno_target(openrac::Mod& mod, const Level& level, gaddr actor) {
    const auto mem = mod.memory();
    if (!hypno_grounded(mem) || !moby_address(actor) || mem.read<int>(level.capture) != 0) {
        return {};
    }
    const auto state = mem.read<int>(hero_state);
    const bool looking = state == 0x1D;
    const bool first_person = looking && (mem.read<std::uint16_t>(actor + moby_flags) & 1);
    constexpr float angle_normal = 0x1.0c1526p-2f;
    constexpr float angle_first = 0x1.657188p-4f;
    constexpr float cone_first = 0x1.aceea2p-4f;
    constexpr float near_pitch = 0x1.921fb8p-1f;
    constexpr float near_yaw = 0x1.38c356p-1f;
    const openrac::StackFrame frame(mod, 0x30);
    const auto from = frame.address(), direction = from + 0x10, end = from + 0x20;
    auto origin = mem.read<Vec>(first_person ? level.camera : hero + 0x80);
    if (!first_person) {
        const auto up = mem.read<Vec>(actor + 0xE0);
        for (int i = 0; i < 3; ++i) {
            origin[i] += 0.75f * up[i];
        }
    }
    auto forward = mem.read<Vec>(level.camera_forward);
    if (looking) {
        forward[2] = 0.0f;
    }
    const float length = std::hypot(forward[0], forward[1], forward[2]);
    if (!std::isfinite(length) || length < 0.001f) {
        return {};
    }
    auto dest = origin;
    for (int i = 0; i < 3; ++i) {
        forward[i] /= length;
        dest[i] += 35.0f * forward[i];
    }
    mem.write(from, origin);
    mem.write(direction, forward);
    mem.write(end, dest);
    const PreserveCollision collision(mem, level.collision_data);
    const HypnoCollision excluded(mem, level);
    using Search = gaddr (*)(gaddr, gaddr, gaddr, float, float, float, float, float);
    const auto search = active_function<Search>(mod, "func_L00_003B8E58", level.hypno_search);
    if (!search) {
        return {};
    }
    Target target;
    {
        // The native helper can return a cached Lock-On target before querying the aim cone.
        const FreshHypnoAim fresh_aim(mem);
        target = receiver(mem, search(from, direction, 0,
            first_person ? angle_first : angle_normal, first_person ? cone_first : angle_normal,
            35.0f, near_pitch, near_yaw));
    }
    if (!target) {
        const auto line = active_function<Line>(mod, "func_002761F8", level.collision);
        if (line && line(from, end, 0, actor, 0) != 0) {
            target = receiver(mem, mem.read<gaddr>(level.collision_data + 0x18));
        }
    }
    if (target) {
        const auto pos = mem.read<Vec>(target.address + moby_position);
        const float distance = std::hypot(pos[0] - origin[0], pos[1] - origin[1]);
        const float reach = mem.read<float>(pvars(mem, target.address, 0x3E8) + 0x3E4);
        if (!std::isfinite(distance) || distance > 35.0f || (reach != 0.0f && !(distance < reach))) {
            return {};
        }
    }
    return target;
}

inline Target find_target(openrac::Mod& mod, const Level& level, Adapter adapter, gaddr actor,
                          gaddr hand, bool equipped) {
    switch (adapter.kind) {
        case Kind::dynamo: return level.dynamo_search ? dynamo_target(mod, level) : Target{};
        case Kind::tractor: return level.tractor_search ? tractor_target(mod, level).target : Target{};
        case Kind::thermanator: return level.arc ? thermanator_target(mod, level, hand, equipped) : Target{};
        case Kind::hypnomatic: return level.hypno_search ? hypno_target(mod, level, actor) : Target{};
    }
    return {};
}

inline bool ready(Memory mem, Adapter adapter, gaddr hand, Target target) {
    switch (adapter.kind) {
        case Kind::dynamo:
            return hand_state(mem, hand) == 1;
        case Kind::tractor:
            return hand_state(mem, hand) == 1;
        case Kind::thermanator: {
            const auto pv = pvars(mem, hand, 0x64);
            return hand_state(mem, hand) == 2 && pv && mem.read<int>(pv + 0x40) != 0
                && grid_target(mem, mem.read<gaddr>(pv + 0x54)) == target;
        }
        case Kind::hypnomatic: {
            const auto pv = pvars(mem, hand, 0x50);
            return pv && mem.read<int>(held_age) >= 16
                && receiver(mem, mem.read<gaddr>(pv)) == target && hypno_grounded(mem);
        }
    }
    return false;
}

inline bool fired(Memory mem, Adapter adapter, gaddr hand) {
    const auto state = hand_state(mem, hand);
    switch (adapter.kind) {
        case Kind::dynamo: return state >= 2 && state <= 4;
        case Kind::tractor: return mem.read<gaddr>(tractor_latch) != 0;
        case Kind::thermanator: return state == 3;
        case Kind::hypnomatic: {
            const auto pv = pvars(mem, hand, 0x50);
            return pv && mem.read<int>(pv + 0x48) != 0;
        }
    }
    return false;
}

inline bool resting(Memory mem, Adapter adapter, gaddr hand) {
    const auto state = hand_state(mem, hand);
    switch (adapter.kind) {
        case Kind::dynamo: return state <= 1;
        case Kind::tractor: return state <= 1 && mem.read<gaddr>(tractor_latch) == 0;
        case Kind::thermanator: return state <= 2;
        case Kind::hypnomatic: return mem.read<std::uint8_t>(item_in_use) == 0;
    }
    return false;
}

} // namespace gadgets
