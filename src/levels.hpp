// SPDX-License-Identifier: GPL-3.0-or-later
// Overlay addresses verified against Going Commando USA v1.01 and OpenRAC 1.1.0.
#pragma once

#include "openrac/mod.hpp"

namespace gadgets {

struct Level {
    openrac::gaddr equip;
    openrac::gaddr dynamo_search;
    openrac::gaddr tractor_search;
    openrac::gaddr collision;
    openrac::gaddr collision_data;
    openrac::gaddr arc;
    openrac::gaddr arc_hit;
    openrac::gaddr joint;
    openrac::gaddr reach;
    openrac::gaddr hypno_search;
    openrac::gaddr camera;
    openrac::gaddr camera_forward;
    openrac::gaddr capture;
    openrac::gaddr moby_base;
    openrac::gaddr moby_end;
    openrac::gaddr tractor_score;
};

constexpr Level levels[31] = {
    {}, // 0
    {0x3174B0, 0x3ABBF8, 0x2AF340, 0x2D3C80, 0x1BF900, 0, 0, 0, 0, 0, 0, 0, 0, 0x1B255C, 0x1B2564, 0x2AEE40}, // 1
    {0x3258D8, 0, 0x2B1B88, 0x2E19C8, 0x1C0940, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 2
    {0x322908, 0x3B88E0, 0, 0x2DF038, 0x1BFDC0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 3
    {0x343580, 0, 0, 0x2FED10, 0x1BFAC0, 0x3A2C08, 0x3A3268, 0x332928, 0x3A5BD8, 0, 0, 0, 0, 0, 0, 0}, // 4
    {}, // 5
    {0x362478, 0x3F9638, 0, 0x31DB98, 0x1BFC80, 0x3C05C8, 0x3C0C28, 0x3514C8, 0x3C3598, 0, 0, 0, 0, 0, 0, 0}, // 6
    {0x317788, 0, 0x2AD6A8, 0x2D4510, 0x1BF640, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 7
    {0x330370, 0, 0, 0x2EA300, 0x1BFF40, 0x38D820, 0x38DE80, 0x31E680, 0x3907F0, 0, 0, 0, 0, 0, 0, 0}, // 8
    {0x31DAA8, 0x3B4B38, 0, 0x2DA5C0, 0x1BFE40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 9
    {}, // 10
    {0x333D68, 0x3C92E8, 0, 0x2F0318, 0x1C0D00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 11
    {0x3250E8, 0, 0x2B40C0, 0x2E1BC8, 0x1C0240, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 12
    {}, // 13
    {}, // 14
    {}, // 15
    {0x31AB68, 0x3AFE88, 0, 0x2D7790, 0x1BFF40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 16
    {0x3234E0, 0x3B8950, 0, 0x2DF570, 0x1C0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 17
    {0x33C3E8, 0, 0, 0x2F8D60, 0x1BFC00, 0x39B318, 0x39B978, 0x32C5E0, 0x39E2E8, 0x3DB440, 0x1B97C0, 0x1B99F0, 0x1AFBB0, 0x1B281C, 0x1B2824, 0}, // 18
    {0x31F1B8, 0, 0, 0x2DBF58, 0x1BFE80, 0, 0, 0, 0, 0x3BF4E0, 0x1B9A40, 0x1B9C70, 0x1B0040, 0x1B2A1C, 0x1B2A24, 0}, // 19
    {0x3367B8, 0x3D24C8, 0x2C1618, 0x2F24B8, 0x1C0400, 0x395018, 0x395678, 0x326528, 0x397FE8, 0x3DC4F0, 0x1B9FC0, 0x1BA1F0, 0x1AFF00, 0x1B2F9C, 0x1B2FA4, 0}, // 20
    {}, // 21
    {}, // 22
    {}, // 23
    {}, // 24
    {}, // 25
    {}, // 26
    {}, // 27
    {}, // 28
    {}, // 29
    {}, // 30
};

} // namespace gadgets
