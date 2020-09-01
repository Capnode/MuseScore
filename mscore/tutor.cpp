//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2002-2011 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include "config.h"
#include "musescore.h"
#include "tutor.h"

#include <stdio.h>
#include <time.h>
#include <assert.h>

Tutor::Tutor() : num_curr_events(0), coeff(-2.0), lit_until_release(false)
{
    // mark all notes as unused
    for (int i = 0; i < 256; i++) {
        notes[i].velo = -1;
    }
}

void Tutor::addKey(int pitch, int velo, int channel, int future) {
    struct timespec prev = { 0, 0 };
    if (velo == 0) {
        clearKey(pitch);
        return;
    }
    pitch &= 255;
    tnote& n = notes[pitch];
    std::lock_guard<std::mutex> lock(mtx);
    if (velo == n.velo && channel == n.channel && future == n.future) {
        return;
    }

    qDebug("addKey(): p=%d, v=%d, c=%d, f=%d", pitch, velo, channel, future);
    if (n.velo != -1 && n.velo != -2) {
        if (future == 0 && n.future > 0) {
            ++num_curr_events;
            if (n.ts.tv_sec != 0 || n.ts.tv_nsec != 0)
            {
                prev = n.ts;
            }
        }
        else if (future > n.future || (future == n.future && velo < n.velo)) {
            return;
        }
    }
    else {
        if (future == 0) {
            ++num_curr_events;
        }
    }
    n = { velo, channel, future, {0, 0} };

    // Future keys pressed less than 100ms ago are automatically cleared
    // Purposedly light up then turn off LED (unless too fast to be visible)
    if (prev.tv_sec != 0 || prev.tv_nsec != 0) {
        struct timespec now;
        timespec_get(&now, TIME_UTC);
        unsigned long diff_us = timespec_sub_us(&now, &prev);
        if (diff_us < 100000) {
            //printf("diff_us: %lu, now: %ld,%ld, prev: %ld,%ld\n", diff_us, now.tv_sec,now.tv_nsec, prev.tv_sec,prev.tv_nsec);
            --num_curr_events;
            n.velo = -1;
        }
        //printf("pitch %d, diff_us: %lu, size: %d\n", pitch, diff_us, num_curr_events);
    }
}

void Tutor::clearKeyNoLock(int pitch, bool mark) {
    qDebug("clearKey(): p=%d", pitch);
    assert(!mtx.try_lock());
    pitch &= 255;
    tnote& n = notes[pitch];
    if (n.velo == -2) {
        // num_curr_events was already decreased in this case
        n.velo = -1;
    }
    else if (n.velo != -1) {
        if (n.future == 0) {
            --num_curr_events;
            n.velo = -1;
        }
        else if (mark) {
            timespec_get(&n.ts, TIME_UTC);
            //printf("Marking time for pitch %d: %d,%d\n", pitch, n.ts.tv_sec,n.ts.tv_nsec);
        }
    }
}

void Tutor::clearKey(int pitch, bool mark) {
    std::lock_guard<std::mutex> lock(mtx);
    clearKeyNoLock(pitch, mark);
}

void Tutor::clearKeys(int channel) {
    do {
        std::lock_guard<std::mutex> lock(mtx);
        if (channel == -1) {
            for (int i = 0; i < (int)(sizeof(notes) / sizeof(notes[0])); ++i) {
                notes[i].velo = -1;
            }
            num_curr_events = 0;
        }
        else {
            for (int i = 0; i < (int)(sizeof(notes) / sizeof(notes[0])); ++i) {
                if (notes[i].velo != -1 && notes[i].channel == channel) {
                    notes[i].velo = -1;
                }
            }
        }
    } while (false);
}

// ms to wait before flushing on keyPressed()
static int FLUSH_TOUT = 5;

// returns: 0 (expected key press), 1 (future key press), -1 (key release or unexpected key press)
int Tutor::keyPressed(int pitch, int velo) {
    if (velo == 0) {
        return -1;
    }

    pitch &= 255;
    tnote& n = notes[pitch];
    int rv = -1;
    do {
        std::lock_guard<std::mutex> lock(mtx);
        if (n.velo == -1 || n.velo == -2) {
            rv = -1;
            break;
        }
        if (n.future == 0) {
            if (lit_until_release) {
                qDebug("Marking key as pressed: pitch=%d", pitch);
                n.velo = -2;
            }
            else {
                qDebug("Clearing key: pitch=%d", pitch);
                n.velo = -1;
            }
            --num_curr_events;
            rv = 0;
            break;
        }
        else if (n.future > 0 && num_curr_events == 0) {
            rv = n.future;
            qDebug("Clearing future event & skipping: pitch=%d", pitch);
            clearKeyNoLock(pitch, true);
            break;
        }
    } while (false);
    return rv;
}

size_t Tutor::size() {
    std::lock_guard<std::mutex> lock(mtx);
    return num_curr_events;
}
