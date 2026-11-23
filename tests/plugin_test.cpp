// SPDX-License-Identifier: GPL-3.0-or-later
#include "native.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

using namespace gadgets;
using Buttons = std::uint8_t;
using Report = std::array<Buttons, 18>;

constexpr Buttons fire_button = 0x20;
constexpr Buttons r2_button = 0x02;
constexpr Buttons triangle_button = 0x10;
constexpr Buttons jump_button = 0x40;
constexpr Buttons square_button = 0x80;
constexpr gaddr actor = 0x300000, hand = 0x301000, object = 0x302000;
constexpr gaddr obstruction = object + 0x100;
constexpr gaddr class_row = 0x303000, object_pvars = 0x304000, hand_pvars = 0x305000, guest_stack = 0x380000;

std::vector<std::uint8_t> ram(0x400000);
Memory mem(ram.data());
openrac_play_status status{};
int overlay = 1;
int native_equip_calls = 0;
int shots = 0;
int gadget_starts = 0;
int native_searches = 0;
int exact_muzzle_calls = 0;
gaddr candidate = 0;
bool blocked = false;
bool listed_for_tractor = true;
bool wrong_address = false;
std::string failure;
std::string fixture;
int (*equip_hook)(int) = nullptr;
std::vector<std::pair<openrac_pad_fn, void*>> pad_hooks;
std::vector<std::pair<openrac_frame_fn, void*>> frame_hooks;

void check(bool ok, const char* why) {
    if (!ok) {
        std::cerr << why << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int real_equip(int item) {
    ++native_equip_calls;
    mem.write(equip_request, item);
    return 1;
}

int equip_entry(int item) {
    return equip_hook ? equip_hook(item) : real_equip(item);
}

gaddr find_dynamo() {
    ++native_searches;
    return blocked ? 0 : candidate;
}

gaddr find_tractor(gaddr record, int type, int line_check, float reach, float yaw, float pitch, float roll) {
    check(record == hero && type == 0x15 && line_check == 0 && reach == 20.0f && yaw == 0.25f
        && pitch > 0.52f && roll > 0.69f, "Tractor Beam mixed arguments");
    ++native_searches;
    return listed_for_tractor ? candidate : 0;
}

float score_tractor(gaddr target, float yaw, float range, gaddr rejected, float pitch, float roll) {
    check(target == object && yaw == 0.25f && range == 20.0f && pitch > 0.52f && roll > 0.69f,
        "Oozla preview uses the same native aim scorer");
    mem.write(rejected, 0);
    return 7.0f;
}

int line(gaddr from, gaddr to, int flags, gaddr ignore, gaddr impacts) {
    check(game_address(from, 16, 16) && game_address(to, 16, 16) && impacts == 0,
        "native collision receives guest vector addresses");
    if (flags == 2) {
        check(to == hero + 0xE0 && ignore == object, "Tractor Beam uses the native LOS endpoints");
    }
    mem.write<gaddr>(levels[overlay].collision_data + 0x18, 0);
    return blocked;
}

void joint(gaddr who, int index, gaddr output) {
    check(who == hand && index == 0, "equipped Thermanator uses its own muzzle");
    ++exact_muzzle_calls;
    mem.write<Vec>(output, {1.0f, 0.0f, 1.0f, 1.0f});
}

float reach(float value) { return value; }

void arc(gaddr output, gaddr origin, gaddr target, float least, float range, float speed, float gravity,
         gaddr target_moby) {
    const bool aiming = mem.read<int>(hero_state) == 0x1D;
    check(target == 0 && target_moby == 0 && least > 0.0f && range == (aiming ? 11.0f : 4.5f) && speed == 0.25f
        && gravity > 0.0f && game_address(origin, 16, 16), "Thermanator arc mixed arguments");
    const Vec expected = mem.read<int>(held_item) == 39
        ? Vec{1.0f, 0.0f, 1.0f, 1.0f} : Vec{1.0f, 0.5f, 1.25f, 1.0f};
    check(mem.read<Vec>(origin) == expected, "Thermanator preview must use the live hand attachment");
    mem.write<Vec>(output, {0.25f, 0.0f, 0.0f, 0.0f});
}

int arc_hit(gaddr muzzle, gaddr velocity, gaddr end, gaddr normal, gaddr ignore, gaddr hit, gaddr extra,
            float gravity) {
    check(game_address(muzzle, 16, 16) && game_address(velocity, 16, 16)
        && game_address(end, 16, 16) && game_address(normal, 16, 16)
        && ignore == hand && extra == 0 && gravity > 0.0f, "Thermanator collision mixed arguments");
    ++native_searches;
    mem.write(hit, blocked ? 0 : candidate);
    return blocked ? 0 : 1;
}

gaddr find_receiver(gaddr from, gaddr direction, gaddr prior, float a, float b, float range,
                    float pitch, float yaw) {
    check(game_address(from, 16, 16) && game_address(direction, 16, 16)
        && prior == 0 && a > 0.25f && b > 0.25f && range == 35.0f
        && pitch > 0.78f && yaw > 0.60f, "Hypnomatic native search mixed arguments");
    ++native_searches;
    if (const auto cached = mem.read<gaddr>(cached_lock_target)) {
        return cached;
    }
    return blocked || mem.read<Vec>(direction)[0] < 0.5f
        || mem.read<gaddr>(obstruction + moby_collision) != 0 ? 0 : candidate;
}

void native_throw() {}

void* symbol(const char* name) {
    if (!std::strcmp(name, "func_002AD860")) return reinterpret_cast<void*>(&equip_entry);
    if (!std::strcmp(name, "func_00315C78")) return reinterpret_cast<void*>(&find_dynamo);
    if (!std::strcmp(name, "func_L00_002AE888")) return reinterpret_cast<void*>(&find_tractor);
    if (!std::strcmp(name, "func_L00_002AE388")) return reinterpret_cast<void*>(&score_tractor);
    if (!std::strcmp(name, "func_002761F8")) return reinterpret_cast<void*>(&line);
    if (!std::strcmp(name, "func_0029FCC0")) return reinterpret_cast<void*>(&joint);
    if (!std::strcmp(name, "func_003004B0")) return reinterpret_cast<void*>(&reach);
    if (!std::strcmp(name, "func_002FDF40")) return reinterpret_cast<void*>(&arc);
    if (!std::strcmp(name, "func_002FE5A0")) return reinterpret_cast<void*>(&arc_hit);
    if (!std::strcmp(name, "func_L00_003B8E58")) return reinterpret_cast<void*>(&find_receiver);
    if (!std::strcmp(name, "func_L00_002BA508")) return reinterpret_cast<void*>(&native_throw);
    return nullptr;
}

void* address(gaddr code) {
    const auto& l = levels[overlay];
    if (wrong_address && code == l.dynamo_search) return reinterpret_cast<void*>(&native_throw);
    if (code == l.equip) return symbol("func_002AD860");
    if (code == l.dynamo_search) return symbol("func_00315C78");
    if (code == l.tractor_search) return symbol("func_L00_002AE888");
    if (code == l.tractor_score) return symbol("func_L00_002AE388");
    if (code == l.collision) return symbol("func_002761F8");
    if (code == l.joint) return symbol("func_0029FCC0");
    if (code == l.reach) return symbol("func_003004B0");
    if (code == l.arc) return symbol("func_002FDF40");
    if (code == l.arc_hit) return symbol("func_002FE5A0");
    if (code == l.hypno_search) return symbol("func_L00_003B8E58");
    return symbol("func_L00_002BA508");
}

void draw(int item, int state = 0) {
    std::uint16_t type = item == wrench ? 0x47 : 0x900;
    for (auto adapter : adapters) {
        if (adapter.item == item) {
            type = adapter.hand_class;
        }
    }
    mem.write(held_moby, hand);
    mem.write(held_item, item);
    mem.write(held_ready, 2);
    mem.write(held_trigger, item == wrench ? 0x80u : circle);
    mem.write(hand + moby_class, type);
    mem.write(hand + moby_state, static_cast<std::uint8_t>(state));
    mem.write(hand + moby_pvars, hand_pvars);
}

void finish_equip(int state = 0) {
    const auto item = mem.read<int>(equip_request);
    check(item != 0, "no native request to finish");
    mem.write(equip_request, 0);
    mem.write(selected_item, item);
    mem.write(remembered_item, item);
    draw(item, state);
}

void target_for(Kind kind) {
    candidate = object;
    mem.write(object + moby_uid, std::uint16_t{17});
    mem.write(object + moby_pvars, object_pvars);
    mem.write(object + moby_class_data, class_row);
    mem.write<Vec>(object + moby_position, {5.0f, 0.0f, 0.5f, 1.0f});
    if (kind == Kind::dynamo) {
        mem.write(object + moby_class, std::uint16_t{0xCB5});
    } else if (kind == Kind::tractor) {
        mem.write(object + moby_class, std::uint16_t{0xACB});
        mem.write(object + moby_state, std::uint8_t{1});
        mem.write(object + moby_drawn, std::uint8_t{1});
        mem.write(object_pvars + 0x88, 12.0f);
        mem.write(class_row + 0x46, std::int16_t{0x15});
    } else if (kind == Kind::thermanator) {
        mem.write(object + moby_class, std::uint16_t{0x960});
        mem.write(class_row + 0x46, std::int16_t{0x12});
    } else {
        mem.write(object + moby_class, std::uint16_t{0xEE1});
        mem.write(object + moby_drawn, std::uint8_t{1});
        mem.write(object_pvars + 0x3B1, std::uint8_t{0x80});
        mem.write(object_pvars + 0x20, 1.0f);
        mem.write<Vec>(levels[overlay].camera_forward, {1.0f, 0.0f, 0.0f, 0.0f});
    }
}

void simulate_stock(const Report& report) {
    const bool down = (report[1] & fire_button) == 0;
    const bool was_down = (mem.read<std::uint32_t>(pad_held) & circle) != 0;
    const bool pressed = down && !was_down;
    mem.write(pad_held, down ? circle : 0u);
    if (status.phase != OPENRAC_PLAY_PLAYING || mem.read<int>(held_ready) != 2
        || mem.read<std::uint8_t>(hero_body) != 0 || mem.read<int>(health) == 0) {
        return;
    }
    const auto item = mem.read<int>(held_item);
    if (item == 30 && pressed) {
        ++shots;
    } else if (item == 36 && pressed && hand_state(mem, hand) == 1 && candidate && !blocked) {
        ++gadget_starts;
        mem.write(hand + moby_state, std::uint8_t{2});
    } else if (item == 46) {
        if (pressed && mem.read<gaddr>(tractor_latch) != 0) {
            mem.write<gaddr>(tractor_latch, 0);
        } else if (pressed && candidate && !blocked) {
            ++gadget_starts;
            mem.write(tractor_latch, object);
        }
        mem.write(hand + moby_state, static_cast<std::uint8_t>(down && mem.read<gaddr>(tractor_latch) != 0 ? 3 : 1));
    } else if (item == 39) {
        const bool firing = down && candidate && !blocked && mem.read<std::int16_t>(object_pvars + 0x16) == 0;
        if (firing && hand_state(mem, hand) == 2) {
            ++gadget_starts;
        }
        mem.write(hand + moby_state, static_cast<std::uint8_t>(firing ? 3 : 2));
    } else if (item == 55) {
        if (down && !was_down) {
            ++gadget_starts;
        }
        mem.write(hand_pvars + 0x48, down ? 1 : 0);
    }
}

Report step(Buttons held = 0, bool start = false) {
    Report r{};
    r[0] = start ? 0xF7 : 0xFF;
    r[1] = static_cast<Buttons>(~held);
    r[2] = 0xAD;
    r[3] = 0x45;
    r[11] = (held & fire_button) ? 0xFF : 0;
    r[17] = (held & r2_button) ? 0xFF : 0;
    for (auto [callback, user] : pad_hooks) {
        callback(user, 0, r.data(), static_cast<int>(r.size()));
    }
    simulate_stock(r);
    for (auto [callback, user] : frame_hooks) {
        callback(user);
    }
    return r;
}

void reset(int level, int prior, Adapter adapter) {
    std::fill(ram.begin(), ram.end(), 0);
    status.phase = OPENRAC_PLAY_PLAYING;
    status.level = overlay = level;
    ++status.loads;
    native_equip_calls = shots = gadget_starts = native_searches = exact_muzzle_calls = 0;
    blocked = wrong_address = false;
    listed_for_tractor = true;
    mem.write(hero_moby, actor);
    mem.write(health, 4);
    mem.write(remembered_item, 30);
    mem.write(owned_items + 30, std::uint8_t{1});
    mem.write(owned_items + adapter.item, std::uint8_t{1});
    mem.write(hero + 0x98, 0.25f);
    mem.write(hero + 0x340, 1);
    if (levels[level].moby_base) {
        mem.write(levels[level].moby_base, object);
        mem.write(levels[level].moby_end, obstruction + 0x100);
    }
    draw(prior);
    target_for(adapter.kind);
    step();
}

void dynamo_and_wrench() {
    reset(1, 30, adapters[0]);
    check(step(r2_button)[11] == 255 && shots == 1 && native_equip_calls == 0 && native_searches == 0,
        "accepted wrench DLL synthesizes Circle after Auto Gadgets samples R2");
    step();
    check(step(r2_button | fire_button)[11] == 255 && shots == 2 && native_equip_calls == 0,
        "simultaneous physical R2 and Circle fire the weapon");
    step(fire_button);
    check(native_equip_calls == 0, "holding Circle from the R2 chord cannot re-arm the gadget");
    step();
    blocked = true;
    check(step(fire_button)[11] == 255 && shots == 3 && native_equip_calls == 0, "no target preserves Circle");
    step();
    blocked = false;
    wrong_address = true;
    check(step(fire_button)[11] == 255 && native_equip_calls == 0, "unexpected native mapping passes stock Circle");
    step();
    wrong_address = false;

    check(step(fire_button)[11] == 0 && mem.read<int>(equip_request) == 36, "Dynamo claims fresh Circle");
    step();
    check(gadget_starts == 0, "tap waits for the queued native equip");
    finish_equip(1);
    check(step()[11] == 255 && gadget_starts == 1, "Dynamo replays one buffered tap");
    step();
    mem.write(hand + moby_state, std::uint8_t{1});
    step();
    check(mem.read<int>(equip_request) == 30 && gadget_starts == 1, "Dynamo restores only after returning ready");
    finish_equip();
    step();

    step(fire_button);
    finish_equip(1);
    step(fire_button);
    mem.write(hand + moby_state, std::uint8_t{1});
    step(fire_button);
    finish_equip();
    const int before = shots;
    check(step(fire_button)[11] == 0 && shots == before && gadget_starts == 2,
        "held Circle cannot repeat Dynamo or fire the restored weapon");
    step();
    reset(1, wrench, adapters[0]);
    step(fire_button);
    finish_equip(1);
    step();
    mem.write(hand + moby_state, std::uint8_t{1});
    step();
    check(mem.read<int>(equip_request) == wrench, "restore the exact previous wrench selection");
    finish_equip();
    step();
    check(mem.read<int>(remembered_item) == 30, "wrench restores the previous fallback weapon");
}

void cancellations() {
    reset(1, 30, adapters[0]);
    step(fire_button);
    blocked = true;
    step(fire_button);
    check(mem.read<int>(equip_request) == 0 && gadget_starts == 0, "target loss clears our queued equip");
    blocked = false;
    step(fire_button);
    check(native_equip_calls == 1, "stale held Circle never requeues");
    step();
    step(fire_button);
    finish_equip(1);
    ++mem.ref<std::uint16_t>(object + moby_uid);
    step();
    check(mem.read<int>(equip_request) == 30 && gadget_starts == 0, "replacement target does not receive old shot");

    reset(1, 30, adapters[0]);
    step(fire_button);
    const auto menu = step(triangle_button);
    check((menu[1] & triangle_button) == 0 && mem.read<int>(equip_request) == 0, "Triangle is stock");
    step();
    step(fire_button);
    equip_entry(32);
    step(fire_button);
    check(mem.read<int>(equip_request) == 32, "manual selection wins");
    reset(1, 30, adapters[0]);
    step(fire_button);
    mem.write(health, 0);
    step(fire_button);
    check(mem.read<int>(equip_request) == 0 && gadget_starts == 0, "death clears the old request");

    reset(1, 30, adapters[0]);
    step(fire_button);
    status.phase = OPENRAC_PLAY_LOADING;
    status.level = 2;
    step(fire_button);
    check(mem.read<int>(equip_request) == 0, "old overlay request cleared during level exit");
    reset(3, 30, adapters[0]);
    candidate = 0;
    step(fire_button);
    check(native_equip_calls == 0, "Endako without a Dynamo target preserves stock Circle");
}

void tractor() {
    reset(1, 30, adapters[1]);
    blocked = true;
    check(step(fire_button)[11] == 255 && !native_equip_calls, "Tractor respects native LOS");
    step();
    blocked = false;
    listed_for_tractor = false;
    mem.write<Vec>(object + moby_position, {12.1f, 0.0f, 0.0f, 1.0f});
    check(step(fire_button)[11] == 255 && !native_equip_calls, "Oozla preview respects native box reach");
    step();
    mem.write<Vec>(object + moby_position, {11.9f, 0.0f, 0.0f, 1.0f});
    step(fire_button);
    check(mem.read<int>(equip_request) == 46, "preview queues equip before the native list refreshes");
    finish_equip(1);
    mem.write(hand_pvars + 0x2C, 16);
    check(step(fire_button)[11] == 0 && gadget_starts == 0, "Tractor waits for native listing");
    listed_for_tractor = true;
    check(step(fire_button)[11] == 255 && mem.read<gaddr>(tractor_latch) == object,
        "Tractor picks the target without waiting for the visual unfold");
    check(step(fire_button)[11] == 255 && gadget_starts == 1, "Tractor preserves the native hold");
    const auto camera = step();
    check(camera[11] == 255 && camera[2] == 0xAD && camera[3] == 0x45
        && mem.read<gaddr>(tractor_latch) == object, "Tractor holds with Circle released and right stick intact");
    check(step(fire_button)[11] == 0 && mem.read<gaddr>(tractor_latch) == 0
        && mem.read<int>(equip_request) == 30, "second Circle releases and restores the weapon");

    for (const Buttons drop : {jump_button, square_button, r2_button}) {
        reset(1, 30, adapters[1]);
        step(fire_button);
        finish_equip(1);
        step(fire_button);
        step();
        const auto report = step(drop);
        check((report[1] & drop) == 0 && report[11] == 0 && mem.read<gaddr>(tractor_latch) == 0,
            "Jump, Square and R2 cancel without consuming their physical report");
        if (drop == r2_button) {
            check(mem.read<int>(equip_request) == 30, "R2 makes the prior weapon available to fire");
            finish_equip();
            check(step(r2_button)[11] == 255 && shots == 1, "held R2 fires after restoration");
        }
    }
}

void thermanator(int return_delay_seconds) {
    reset(4, 30, adapters[2]);
    mem.write<Vec>(hero + 0xC80, {1.0f, 0.5f, 1.25f, 1.0f});
    mem.write<Vec>(hero + 0x1200, {90.0f, 91.0f, 92.0f, 1.0f});
    mem.write<std::uint32_t>(levels[overlay].collision_data + 0x18, 0x1234);
    blocked = true;
    check(step(fire_button)[11] == 255 && native_equip_calls == 0, "blocked water keeps stock Circle");
    step();
    blocked = false;
    step(fire_button);
    check(mem.read<int>(equip_request) == 39, "native arc picks the water grid");
    finish_equip(2);
    mem.write(hand_pvars + 0x40, 1);
    mem.write(hand_pvars + 0x54, object);
    step(fire_button);
    check(gadget_starts == 1 && exact_muzzle_calls == 1, "Thermanator rechecks from the equipped muzzle");
    step(fire_button);
    mem.write(object_pvars + 0x14, std::uint8_t{1});
    mem.write(object_pvars + 0x16, std::int16_t{60});
    check(step(fire_button)[11] == 0 && mem.read<int>(equip_request) == 0,
        "freeze transition releases before it can thaw a surface on the same press");
    mem.write(object_pvars + 0x16, std::int16_t{0});
    mem.write(hero_state, 0x1D);
    check(step(fire_button)[11] == 0 && gadget_starts == 1,
        "holding the same press after the freeze does not start thawing");
    step();
    blocked = true;
    check(step(fire_button)[11] == 0 && gadget_starts == 1,
        "a fresh press during the meter delay still requires a clear arc");
    step();
    blocked = false;
    step(fire_button);
    check(gadget_starts == 2 && native_equip_calls == 1,
        "fresh hold during the delay targets ice and resumes without another equip");

    const int delay_ticks = return_delay_seconds * 60;
    for (int tick = 1; tick < delay_ticks; ++tick) {
        step();
        check(mem.read<int>(equip_request) == 0 && hand_state(mem, hand) == 2,
            "release stops firing and keeps the Thermanator equipped throughout the meter delay");
    }
    step();
    check(mem.read<int>(equip_request) == 30, "Thermanator restores exactly when the configured delay elapses");
    finish_equip();
    step();

    step(fire_button);
    finish_equip(2);
    step(fire_button);
    step();
    check(equip_entry(wrench) != 0, "manual selection works during the Thermanator delay");
    finish_equip();
    for (int tick = 0; tick < delay_ticks; ++tick) {
        step();
    }
    check(mem.read<int>(held_item) == wrench && mem.read<int>(equip_request) == 0,
        "the old return timer cannot replace manually selected equipment");
}

void hypnomatic() {
    reset(18, 30, adapters[3]);
    mem.write(object_pvars + 0x3B1, std::uint8_t{0});
    check(step(fire_button)[11] == 255 && native_equip_calls == 0, "ordinary robot is not a receiver");
    step();
    target_for(Kind::hypnomatic);
    mem.write(cached_lock_target, object);
    mem.write<Vec>(levels[overlay].camera_forward, {-1.0f, 0.0f, 0.0f, 0.0f});
    check(step(fire_button)[11] == 255 && native_equip_calls == 0,
        "cached Hypnomatic target outside the aim cone preserves stock Circle");
    step();
    target_for(Kind::hypnomatic);
    blocked = true;
    check(step(fire_button)[11] == 255 && native_equip_calls == 0,
        "cached Hypnomatic target behind a wall preserves stock Circle");
    step();
    blocked = false;
    mem.write(obstruction + moby_class, std::uint16_t{0x1234});
    mem.write(obstruction + moby_collision, class_row);
    check(step(fire_button)[11] == 255 && native_equip_calls == 0, "ordinary Hypnomatic collider blocks aiming");
    step();
    mem.write(obstruction + moby_class, std::uint16_t{0x11FC});
    step(fire_button);
    check(mem.read<int>(equip_request) == 55 && mem.read<gaddr>(obstruction + moby_collision) == class_row,
        "Hypnomatic uses the stock through-collider rule and restores collision after the search");
    check(mem.read<gaddr>(cached_lock_target) == object, "Hypnomatic leaves the native Lock-On target intact");
    finish_equip();
    mem.write(held_age, 16);
    check(step(fire_button)[11] == 0, "Hypnomatic waits for the native lock");
    mem.write(hand_pvars, object);
    check(step(fire_button)[11] == 255 && gadget_starts == 1, "Hypnomatic replays Circle after receiver lock");
    check(step(fire_button)[11] == 255, "in-view receiver remains eligible before capture");
    mem.write(levels[overlay].capture, 1);
    step();
    check(mem.read<int>(equip_request) == 0, "capture transition retains the Hypnomatic after release");
    mem.write(hero_body, std::uint8_t{4});
    mem.write(hero_moby, object);
    mem.write(health, 0);
    const auto controls = step(square_button | fire_button);
    check((controls[1] & (square_button | fire_button)) == 0 && mem.read<int>(equip_request) == 0,
        "receiver body keeps stock Circle and Square even during receiver death");
    mem.write(hero_body, std::uint8_t{0});
    mem.write(hero_moby, actor);
    mem.write(health, 4);
    mem.write(levels[overlay].capture, 0);
    for (int tick = 0; tick < 4; ++tick) step();
    check(mem.read<int>(equip_request) == 30, "natural return to Ratchet restores the previous item");
}

void hypnomatic_loses_aim() {
    reset(18, 30, adapters[3]);
    mem.write(cached_lock_target, object);
    step(fire_button);
    finish_equip();
    mem.write(held_age, 16);
    mem.write(hand_pvars, object);
    check(step(fire_button)[11] == 255 && gadget_starts == 1, "Hypnomatic begins reaching for the receiver");
    blocked = true;
    check(step(fire_button)[11] == 0, "a wall entering the sightline releases Circle before capture");
    step(fire_button);
    step(fire_button);
    check(mem.read<int>(equip_request) == 30 && mem.read<gaddr>(cached_lock_target) == object,
        "losing sight of the receiver restores the prior item and leaves the native lock intact");
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    check(argc == 3 || argc == 4, "expected paths to both packaged DLLs and optional return delay");
    static const int configured_delay = argc == 4 ? _wtoi(argv[3]) : 0;
    const auto auto_dll = LoadLibraryW(argv[1]);
    const auto wrench_dll = LoadLibraryW(argv[2]);
    check(auto_dll && wrench_dll, "loading the two packaged DLLs");
    const auto auto_load = reinterpret_cast<openrac_mod_load_fn>(GetProcAddress(auto_dll, OPENRAC_MOD_LOAD));
    const auto wrench_load = reinterpret_cast<openrac_mod_load_fn>(GetProcAddress(wrench_dll, OPENRAC_MOD_LOAD));
    const auto query = reinterpret_cast<openrac_mod_query_fn>(GetProcAddress(auto_dll, OPENRAC_MOD_QUERY));
    check(auto_load && wrench_load && query, "DLL exports");
    openrac_mod_info info{};
    info.size = sizeof info;
    query(&info);
    check(info.api_major == 1 && info.api_minor == 14 && !std::strcmp(info.id, "rac2-auto-gadgets")
        && !std::strcmp(info.version, "0.1.4"), "Auto Gadgets metadata matches manifest");

    const auto scenario = std::filesystem::path(argv[2]).stem().string() + "-" + std::to_string(configured_delay);
    fixture = (std::filesystem::current_path() / "input-order-fixture" / scenario).string();
    std::filesystem::create_directories(fixture);
    const auto list = std::filesystem::path(fixture) / "plugins.json";
    auto write_list = [&](const char* first, const char* second) {
        std::ofstream(list) << "{\"format\":1,\"mods\":[{\"id\":\"" << first
            << "\",\"enabled\":true},{\"id\":\"" << second << "\",\"enabled\":true}]}";
    };
    status.size = sizeof status;
    openrac_api api{};
    api.size = sizeof api;
    api.api_major = 1;
    api.api_minor = 20;
    api.game = "rac2";
    api.version = "ntsc";
    api.openrac_version = "1.1.0";
    api.platform = "windows-x86_64";
    api.memory = ram.data();
    api.fail = [](openrac_mod*, const char* message) { failure = message; };
    api.log = [](openrac_mod*, int, const char*) {};
    api.setting_int = [](openrac_mod*, const char* key) -> std::int64_t {
        check(!std::strcmp(key, "thermanator_return_delay"), "Thermanator delay reads the declared option");
        return configured_delay;
    };
    api.on_pad = [](openrac_mod*, openrac_pad_fn fn, void* user) { pad_hooks.emplace_back(fn, user); return 0; };
    api.on_frame = [](openrac_mod*, openrac_frame_fn fn, void* user) { frame_hooks.emplace_back(fn, user); return 0; };
    api.function = symbol;
    api.function_at = address;
    api.play_status = [](openrac_play_status* out) { *out = status; return 0; };
    api.loaded_level = [] { return overlay; };
    api.stack_push = [](std::uint32_t, gaddr* old) { *old = guest_stack; return guest_stack; };
    api.stack_pop = [](gaddr) {};
    api.hero_set_joints = [](openrac_mod*, const openrac_joint_offset*, int) { return 0; };
    api.plugins_folder = [](char* out, std::uint32_t size) {
        if (fixture.size() >= size) return 0;
        std::memcpy(out, fixture.c_str(), fixture.size() + 1);
        return static_cast<int>(fixture.size());
    };
    api.hook = [](openrac_mod*, const char* id, const char* signature, void* fn, void** original) {
        check(!std::strcmp(id, "rac2.item.equip") && !std::strcmp(signature, "i32(i32)"), "Item_Equip SDK binding");
        equip_hook = reinterpret_cast<int (*)(int)>(fn);
        *original = reinterpret_cast<void*>(&real_equip);
        return static_cast<int>(OPENRAC_HOOK_OK);
    };
    auto* auto_mod = reinterpret_cast<openrac_mod*>(1);
    auto* wrench_mod = reinterpret_cast<openrac_mod*>(2);

    write_list("rac2-contextual-wrench", "rac2-auto-gadgets");
    check(auto_load(&api, auto_mod) != 0 && !failure.empty() && pad_hooks.empty(),
        "reversed order refuses before registering any input callback");
    write_list("rac2-auto-gadgets", "rac2-contextual-wrench");
    api.version = "ntsc2";
    check(auto_load(&api, auto_mod) != 0 && pad_hooks.empty(), "other disc revision refuses");
    api.version = "ntsc";
    failure.clear();
    check(auto_load(&api, auto_mod) == 0 && wrench_load(&api, wrench_mod) == 0
        && failure.empty() && pad_hooks.size() == 2 && frame_hooks.size() == 2, "both DLLs load in order");

    dynamo_and_wrench();
    cancellations();
    tractor();
    thermanator(configured_delay >= 2 && configured_delay <= 5 ? configured_delay : 3);
    hypnomatic();
    hypnomatic_loses_aim();
    std::cout << "Dynamo, Tractor Beam, Thermanator, Hypnomatic, and real wrench DLL ordering passed.\n";
}
