#include "precompiled.hpp"
#include "sea_object.hpp"
#include "packet.h"

using namespace ss;

void sea_object::update(float delta_time) {
    /*if (remain_unloading_time > 0) {
        assert(state == SOS_UNLOADING);
        remain_unloading_time -= delta_time;
        if (remain_unloading_time <= 0) {
            remain_unloading_time = 0;
            state = SOS_LOADING;
        }
    }
    if (remain_loading_time > 0) {
        assert(state == SOS_LOADING);
        remain_loading_time -= delta_time;
        if (remain_loading_time <= 0) {
            remain_loading_time = 0;
            state = SOS_SAILING;
        }
    }*/
}

int sea_object::add_cargo(int amount) {
    if (amount < 0) {
        amount = 0;
    }
    if (amount > MAX_CARGO) {
        amount = MAX_CARGO;
    }
    const auto before = cargo;
    auto after = before + amount;
    if (after > MAX_CARGO) {
        after = MAX_CARGO;
    }
    cargo = after;
    return after - before;
}

int sea_object::remove_cargo(int amount) {
    if (amount < 0) {
        amount = 0;
    }
    if (amount > MAX_CARGO) {
        amount = MAX_CARGO;
    }
    const auto before = cargo;
    auto after = before - amount;
    if (after < 0) {
        after = 0;
    }
    cargo = after;
    return before - after;
}

void sea_object::fill_packet(LWPTTLFULLSTATEOBJECT& p) const {
    p.fx0 = fx;
    p.fy0 = fy;
    p.fx1 = fx + fw;
    p.fy1 = fy + fh;
    p.fvx = fvx;
    p.fvy = fvy;
    p.id = id;
    p.type = type;
    strcpy(p.guid, guid.c_str());
    if (state == SOS_LOADING) {
        strcat(p.guid, "[LOADING]");
    } else if (state == SOS_UNLOADING) {
        strcat(p.guid, "[UNLOADING]");
    }
}
