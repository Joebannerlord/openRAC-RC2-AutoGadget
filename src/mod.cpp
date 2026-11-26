// SPDX-License-Identifier: GPL-3.0-or-later
// Temporary equip and input replay flow based on glawks' Auto Swingshot (MIT).
#include "native.hpp"

#include "common/json.h"
#include "input.hpp"
#include "openrac/games/rac2.hpp"

#include <filesystem>
#include <memory>

namespace {

using namespace gadgets;
using Report = std::uint8_t*;
using Equip = openrac::rac2::Item_Equip;

constexpr char id[] = "rac2-auto-gadgets";
constexpr int equip_timeout = 100;
constexpr int capture_timeout = 300;
constexpr int default_thermanator_delay = 3;
constexpr int game_ticks_per_second = 60;

enum class Phase { idle, equipping, replay, active, capture, canceling, restoring };

struct Player {
    openrac_play_status status;
    const Level* level = nullptr;
    gaddr actor = 0;
    gaddr hand = 0;
    int item = 0;
    int group = 0;
    int body = 0;
    int ready = 0;
    int request = 0;
    int selected = 0;
    bool alive = false;
    bool usable = false;
    bool free_hand = false;

    bool drawn() const { return moby_address(hand) && ready == 2; }
    bool has(const Adapter& adapter, Memory mem) const {
        return drawn() && item == adapter.item && mem.read<gaddr>(held_trigger) == circle
            && mem.read<std::uint16_t>(hand + moby_class) == adapter.hand_class;
    }
};

Player player(openrac::Mod& mod) {
    Player p{mod.play_status()};
    const auto level = p.status.level;
    if (level < 0 || level >= std::size(levels) || mod.loaded_level() != level || !levels[level].equip) {
        return p;
    }
    p.level = &levels[level];
    const auto mem = mod.memory();
    p.actor = mem.read<gaddr>(hero_moby);
    p.body = mem.read<std::uint8_t>(hero_body);
    p.group = mem.read<int>(hero_group);
    const int state = mem.read<int>(hero_state);
    p.alive = moby_address(p.actor) && mem.read<int>(health) > 0 && state != 0x31 && state != 0x61;
    p.usable = p.alive && p.status.phase == OPENRAC_PLAY_PLAYING && p.body == 0
        && mem.read<gaddr>(vehicle) == 0 && mem.read<int>(pad_lock) == 0
        && mem.read<std::uint8_t>(no_update) == 0 && mem.read<std::uint8_t>(no_menu) == 0
        && mem.read<std::uint8_t>(no_item) == 0 && mem.read<std::uint8_t>(item_tucked) == 0;
    p.free_hand = mem.read<std::uint8_t>(wrench_thrown) == 0 && mem.read<std::uint8_t>(melee_active) == 0;
    p.request = mem.read<int>(equip_request);
    p.selected = mem.read<int>(selected_item);
    p.hand = mem.read<gaddr>(held_moby);
    p.item = mem.read<int>(held_item);
    p.ready = mem.read<int>(held_ready);
    return p;
}

bool can_equip(const Player& p) {
    return p.usable && p.free_hand && contextual::is_foot_group(p.group);
}

void circle_report(Report report, bool down) {
    auto& buttons = report[contextual::buttons];
    buttons = static_cast<std::uint8_t>(down ? (buttons & ~circle) : (buttons | circle));
    report[contextual::circle_pressure] = down ? 0xFF : 0;
}

class Controls {
public:
    Controls(openrac::Mod& mod, int thermanator_delay)
        : mod_(mod), thermanator_delay_ticks_(thermanator_delay * game_ticks_per_second) {}

    int stock_equip(openrac::Original<int(int)> original, int item) {
        const int result = original(item);
        // A manual choice, including explicit re-selection of our gadget, owns the hand.
        if (!calling_equip_ && phase_ != Phase::idle && phase_ != Phase::capture && result != 0
            && (item == mod_.memory().read<int>(equip_request) || item == adapter_->item)) {
            release_tractor(player(mod_));
            close();
        }
        return result;
    }

    void pad_report(int port, std::uint8_t* bytes, int size) {
        if (port != 0 || bytes == nullptr || size != contextual::report_size) {
            return;
        }
        auto* report = bytes;
        const auto input = contextual::sample(report);
        const bool down = input.down(contextual::Circle);
        const bool fresh = down && !circle_down_;
        circle_down_ = down;
        if (!down) {
            swallowed_ = false;
        }
        const auto p = player(mod_);
        const auto mem = mod_.memory();
        validate(p);
        if (phase_ == Phase::capture) {
            return;
        }
        if (input.opening_menu() || p.status.phase == OPENRAC_PLAY_MENU) {
            swallowed_ = false;
            if (phase_ == Phase::equipping || phase_ == Phase::replay || phase_ == Phase::active) {
                cancel(p);
            }
            return;
        }
        if (p.status.phase != OPENRAC_PLAY_PLAYING || !p.alive) {
            return;
        }
        if (input.down(contextual::Square)) {
            if (phase_ != Phase::idle) {
                release_tractor(p);
                clear_own_request(p);
                close();
                swallowed_ = false;
            }
            return;
        }

        const bool r2_fire = input.down(contextual::R2) && p.drawn()
            && contextual::is_weapon(p.item) && mem.read<gaddr>(held_trigger) == circle;
        if (r2_fire) {
            if (phase_ == Phase::equipping) {
                cancel(p);
            }
            return;
        }

        const bool jumping = input.down(contextual::Cross) || p.group == 2 || p.group == 4;
        const bool break_tractor = jumping || input.down(contextual::R2);

        if (phase_ == Phase::canceling && thermanator_used_ && fresh && p.has(*adapter_, mem)) {
            resume_thermanator(p);
        } else if (phase_ == Phase::idle && fresh && can_equip(p) && p.request == 0 && p.drawn()
            && (p.selected == 0 || p.selected == p.item) && owned(mem, p.item)
            && mem.read<std::uint8_t>(item_in_use) == 0) {
            if (begin(p, break_tractor)) {
                circle_report(report, false);
                return;
            }
        }

        if (phase_ == Phase::equipping) {
            if (adapter_->kind == Kind::tractor && break_tractor) {
                cancel(p);
                circle_report(report, false);
            } else {
                pending(p, report);
            }
        } else if (phase_ == Phase::replay && started_at_ == frame_) {
            circle_report(report, true);
        } else if (phase_ == Phase::active && adapter_->hold()) {
            if (adapter_->kind == Kind::tractor) {
                tractor_input(p, report, fresh || break_tractor);
            } else {
                held_input(p, report);
            }
        } else if (swallowed_ && p.usable) {
            circle_report(report, false);
        }
    }

    void frame() {
        ++frame_;
        const auto p = player(mod_);
        const auto mem = mod_.memory();
        validate(p);
        if (phase_ == Phase::idle || phase_ == Phase::capture) {
            return;
        }
        if (p.status.phase != OPENRAC_PLAY_PLAYING) {
            if (phase_ != Phase::restoring) {
                cancel(p);
            }
            return;
        }

        if (phase_ == Phase::replay) {
            if (p.has(*adapter_, mem) && fired(mem, *adapter_, p.hand)) {
                phase_ = Phase::active;
                thermanator_used_ = adapter_->kind == Kind::thermanator;
            } else {
                cancel(p);
            }
        }
        if (phase_ == Phase::active && p.has(*adapter_, mem)) {
            if (adapter_->kind == Kind::dynamo && resting(mem, *adapter_, p.hand)) {
                cancel(p);
            } else if (adapter_->kind == Kind::tractor && mem.read<gaddr>(tractor_latch) != target_.address) {
                cancel(p);
            }
        }
        if (phase_ == Phase::canceling && can_equip(p)) {
            restore(p);
        } else if (phase_ == Phase::restoring) {
            if (p.item == previous_ && p.drawn() && p.request == 0) {
                // An explicit Item_Equip(wrench) writes the native fallback slot as well.
                if (previous_ == wrench && p.selected == wrench && mem.read<int>(remembered_item) == wrench) {
                    mem.write(remembered_item, previous_remembered_);
                }
                close();
            } else if (frame_ - started_at_ >= equip_timeout) {
                clear_own_request(p);
                close();
            }
        }
    }

private:
    bool begin(const Player& p, bool break_tractor) {
        const auto mem = mod_.memory();
        const Adapter* chosen = nullptr;
        Target chosen_target;
        for (const auto& adapter : adapters) {
            if (!owned(mem, adapter.item) || (adapter.kind == Kind::tractor && break_tractor)) {
                continue;
            }
            const auto target = find_target(mod_, *p.level, adapter, p.actor, p.hand, p.has(adapter, mem));
            if (!target) {
                continue;
            }
            if (chosen || adapter.item == p.item) {
                return false;
            }
            chosen = &adapter;
            chosen_target = target;
        }
        if (!chosen || !equip(*p.level, chosen->item)) {
            return false;
        }

        adapter_ = chosen;
        target_ = chosen_target;
        level_ = p.level;
        loads_ = p.status.loads;
        actor_ = p.actor;
        previous_ = p.item;
        previous_selected_ = p.selected;
        previous_remembered_ = mem.read<int>(remembered_item);
        phase_ = Phase::equipping;
        started_at_ = frame_;
        saw_receiver_body_ = false;
        swallowed_ = true;
        return true;
    }

    void pending(const Player& p, Report report) {
        const auto mem = mod_.memory();
        const bool equipped = p.has(*adapter_, mem);
        if (frame_ - started_at_ >= equip_timeout || !can_equip(p) || !owned(mem, adapter_->item)
            || (adapter_->hold() && !circle_down_)) {
            cancel(p);
        } else {
            const auto selection = adapter_->kind == Kind::tractor ? tractor_target(mod_, *level_)
                : TractorSelection{find_target(mod_, *level_, *adapter_, p.actor, p.hand, equipped), true};
            if (selection.target != target_) {
                cancel(p);
            } else if (selection.listed && p.request == 0 && equipped && ready(mem, *adapter_, p.hand, target_)
                       && (mem.read<std::uint32_t>(pad_held) & circle) == 0) {
                phase_ = Phase::replay;
                started_at_ = frame_;
                swallowed_ = circle_down_;
                circle_report(report, true);
                return;
            }
        }
        circle_report(report, false);
    }

    void tractor_input(const Player& p, Report report, bool drop) {
        const auto mem = mod_.memory();
        const auto link = mem.read<gaddr>(tractor_latch);
        if (drop || !p.usable || !p.has(*adapter_, mem) || target_at(mem, link) != target_) {
            cancel(p);
            swallowed_ = circle_down_;
            circle_report(report, false);
            return;
        }
        // Preserve the same stock hold across physical release and leave both sticks alone.
        circle_report(report, true);
    }

    void held_input(const Player& p, Report report) {
        const auto mem = mod_.memory();
        if (!p.has(*adapter_, mem)) {
            cancel(p);
        } else {
            const auto hand_pvars = pvars(mem, p.hand, 0x64);
            const auto current = adapter_->kind == Kind::thermanator
                ? (hand_pvars ? grid_target(mem, mem.read<gaddr>(hand_pvars + 0x54)) : Target{})
                : (hand_pvars ? receiver(mem, mem.read<gaddr>(hand_pvars)) : Target{});
            if (!circle_down_ || current != target_ || !p.usable) {
                cancel(p);
            } else if (adapter_->kind == Kind::hypnomatic && hypno_target(mod_, *level_, p.actor) != target_) {
                cancel(p);
            }
        }
        if (phase_ == Phase::canceling && circle_down_) {
            swallowed_ = true;
            circle_report(report, false);
        }
    }

    void resume_thermanator(const Player& p) {
        swallowed_ = true;
        const auto mem = mod_.memory();
        if (!can_equip(p) || p.request != 0 || (mem.read<std::uint32_t>(pad_held) & circle) != 0) {
            return;
        }
        const auto target = thermanator_target(mod_, *level_, p.hand, true);
        if (target && ready(mem, *adapter_, p.hand, target)) {
            target_ = target;
            phase_ = Phase::replay;
            started_at_ = frame_;
        }
    }

    bool equip(const Level& level, int item) {
        const auto function = active_function<Equip::fn>(mod_, "func_002AD860", level.equip);
        if (!function) {
            return false;
        }
        calling_equip_ = true;
        const int result = function(item);
        calling_equip_ = false;
        return result != 0 && mod_.memory().read<int>(equip_request) == item;
    }

    bool same_program(const Player& p) const {
        return level_ && p.status.loads == loads_
            && mod_.loaded_level() == static_cast<int>(level_ - levels);
    }

    void validate(const Player& p) {
        if (phase_ == Phase::idle) {
            return;
        }
        if (!same_program(p)) {
            close();
            swallowed_ = false;
            return;
        }

        // A capture moves the native hero to the receiver and locks the original body.
        // Let the stock transition own both the capture animation and every possession input.
        if (adapter_->kind == Kind::hypnomatic && (phase_ == Phase::replay || phase_ == Phase::active
            || phase_ == Phase::capture || phase_ == Phase::canceling)) {
            const auto mem = mod_.memory();
            const bool capturing = mem.read<int>(level_->capture) != 0;
            if (p.body == 4 || (!saw_receiver_body_ && capturing && p.alive)) {
                if (phase_ != Phase::capture) {
                    started_at_ = frame_;
                    phase_ = Phase::capture;
                    swallowed_ = false;
                }
                saw_receiver_body_ |= p.body == 4;
                if (saw_receiver_body_ || frame_ - started_at_ < capture_timeout) {
                    return;
                }
                close();
                return;
            }
            if (phase_ == Phase::capture) {
                if (!saw_receiver_body_ && frame_ - started_at_ < capture_timeout && p.alive) {
                    return;
                }
                cancel(p);
            }
        }

        if (p.status.phase == OPENRAC_PLAY_LOADING || p.status.phase == OPENRAC_PLAY_FRONT_END
            || (p.level && (!p.alive || p.actor != actor_))) {
            clear_own_request(p);
            close();
            swallowed_ = false;
            return;
        }

        const auto wanted = phase_ == Phase::restoring ? previous_ : adapter_->item;
        const bool native_wrench = saw_receiver_body_ && (p.item == wrench || p.selected == wrench);
        const bool native_return_request = saw_receiver_body_ && phase_ == Phase::canceling && p.request == wrench;
        if (p.level && ((p.request != 0 && p.request != wanted && !native_return_request)
            || (!native_wrench && p.selected != adapter_->item && p.selected != previous_selected_
                && p.selected != previous_)
            || (!native_wrench && p.drawn() && p.item != adapter_->item && p.item != previous_))) {
            close();
        }
    }

    void clear_own_request(const Player& p) {
        if (!same_program(p) || (phase_ != Phase::equipping && phase_ != Phase::restoring)) {
            return;
        }
        const int item = phase_ == Phase::restoring ? previous_ : adapter_->item;
        const auto mem = mod_.memory();
        if (mem.read<int>(equip_request) == item) {
            mem.write(equip_request, 0);
        }
    }

    void cancel(const Player& p) {
        release_tractor(p);
        clear_own_request(p);
        if (phase_ != Phase::canceling) {
            phase_ = Phase::canceling;
            started_at_ = frame_;
        }
    }

    void release_tractor(const Player& p) {
        if (!adapter_ || adapter_->kind != Kind::tractor
            || (phase_ != Phase::replay && phase_ != Phase::active)
            || !same_program(p) || p.actor != actor_) {
            return;
        }
        const auto mem = mod_.memory();
        const auto link = mem.read<gaddr>(tractor_latch);
        if (p.has(*adapter_, mem) && target_at(mem, link) == target_) {
            // The native Tractor trigger clears this exact latch on its second Circle edge.
            // Release directly so Jump and Square can reach the game on this same tick.
            mem.write<gaddr>(tractor_latch, 0);
        }
    }

    void restore(const Player& p) {
        const auto mem = mod_.memory();
        if (p.request != 0) {
            return;
        }
        if (!p.drawn()) {
            // After a failed hand spawn, the native selection can still name our gadget.
            if (p.hand == 0 && p.selected == adapter_->item && frame_ - started_at_ >= 8
                && owned(mem, previous_) && equip(*level_, previous_)) {
                phase_ = Phase::restoring;
                started_at_ = frame_;
            } else if (frame_ - started_at_ >= equip_timeout) {
                close();
            }
            return;
        }
        if (p.item == previous_ || !owned(mem, previous_)
            || (p.item != adapter_->item && !(saw_receiver_body_ && p.item == wrench))) {
            close();
            return;
        }
        const bool resting_hand = p.item != adapter_->item || resting(mem, *adapter_, p.hand);
        const bool release_timeout = adapter_->kind == Kind::tractor && frame_ - started_at_ >= 8;
        const bool capture_grace = adapter_->kind == Kind::hypnomatic && frame_ - started_at_ < 3;
        const bool meter_grace = thermanator_used_ && frame_ - started_at_ < thermanator_delay_ticks_;
        if ((!resting_hand && !release_timeout) || capture_grace || meter_grace) {
            return;
        }
        if (equip(*level_, previous_)) {
            phase_ = Phase::restoring;
            started_at_ = frame_;
        } else {
            close();
        }
    }

    void close() {
        phase_ = Phase::idle;
        adapter_ = nullptr;
        level_ = nullptr;
        thermanator_used_ = false;
    }

    openrac::Mod& mod_;
    const std::uint32_t thermanator_delay_ticks_;
    Phase phase_ = Phase::idle;
    const Adapter* adapter_ = nullptr;
    const Level* level_ = nullptr;
    Target target_;
    gaddr actor_ = 0;
    std::uint32_t loads_ = 0;
    std::uint32_t frame_ = 0;
    std::uint32_t started_at_ = 0;
    int previous_ = 0;
    int previous_selected_ = 0;
    int previous_remembered_ = 0;
    bool circle_down_ = true;
    bool swallowed_ = false;
    bool calling_equip_ = false;
    bool saw_receiver_body_ = false;
    bool thermanator_used_ = false;
};

bool is_first_enabled(const std::string& folder) {
    if (folder.empty()) {
        return false;
    }
    const auto path = std::filesystem::path(folder) / "plugins.json";
    openrac::json::Value list;
    std::string error;
    if (!openrac::json::parse_file(path.string(), list, error) || list["format"].number() != 1) {
        return false;
    }
    for (const auto& entry : list["mods"].items()) {
        if (entry["enabled"].is_null() || entry["enabled"].boolean()) {
            return entry["id"].string() == id;
        }
    }
    return false;
}

void load(openrac::Mod& mod) {
    if (mod.game() != "rac2" || mod.version() != "ntsc" || mod.openrac_version() != "1.1.0"
        || !mod.has_play()) {
        throw openrac::Error("Auto Gadgets requires Going Commando USA v1.01 on OpenRAC 1.1.0");
    }
    if (!is_first_enabled(mod.plugins_folder())) {
        throw openrac::Error("Move Auto Gadgets to the first enabled position in the RAC2 Mods list");
    }
    const auto delay = mod.setting<std::int64_t>("thermanator_return_delay");
    const auto seconds = delay >= 2 && delay <= 5 ? static_cast<int>(delay) : default_thermanator_delay;
    auto controls = std::make_shared<Controls>(mod, seconds);
    mod.hook<Equip>([controls](auto original, int item) { return controls->stock_equip(original, item); });
    mod.on_pad([controls](int port, std::uint8_t* bytes, int size) { controls->pad_report(port, bytes, size); });
    mod.on_frame([controls] { controls->frame(); });
    mod.info("Auto Gadgets reads Circle before other mods for Dynamo, Tractor Beam, Thermanator, and Hypnomatic");
}

} // namespace

OPENRAC_MOD_CPP("rac2-auto-gadgets", "0.1.5", load)
