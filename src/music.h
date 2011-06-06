// music.h
//
// Something stupid I felt like writing for no reason!
//
#pragma once

#include "events.h"
#include "player.h"
#include "network/response.h"

#include <list>
#include <memory>

namespace boostcraft { namespace stupidmusic {

enum duration {
    E = 250,
    Q = 500,
    H = 1000,
    W = 2000
};

enum pitches {
                               Fs3,  G3,  Gs3,  A3,  As3,  B3,
    C4,  Cs4, D4, Ds4, E4, F4, Fs4,  G4,  Gs4,  A4,  As4,  B4,
    C5,  Cs5, D5, Ds5, E5, F5, Fs5
};

static void send_note(Player& p, int instrument, int pitch)
{
    network::Response r;
    r << (uint8_t)0x36;
    r << (uint32_t)0 << (uint16_t)1 << (uint32_t)0;
    r << (uint8_t)instrument << (uint8_t)pitch;
    p.deliver(r);
}

static void playSequence(std::weak_ptr<Player> weakp,
        std::list<int> notes, std::list<int> times)
{
    //
    // Atempt to lock weak_ptr before doing anything; this ensures that we'll
    // stop playing music automatically when the target player disconnects.
    //
    auto player = weakp.lock();

    if (player && !notes.empty() && !times.empty())
    {
        int pitch = notes.front();
        int duration = times.front();

        // Play the head note and then schedule another call to this function
        send_note(*player, 0, pitch);
        notes.pop_front();
        times.pop_front();
        schedule(duration, std::bind(&playSequence, weakp, notes, times));
    }
}

}} // namespace boostcraft::stupidmusic

