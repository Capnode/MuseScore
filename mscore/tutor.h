//=============================================================================
//  MusE Score
//  Linux Music Score Editor
//
//  Copyright (C) 2002-2009 Werner Schweer and others
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

#ifndef __TUTOR_H__
#define __TUTOR_H__

#include <thread>
#include <mutex>
#include <string>

typedef struct {
  int velo;     // -1=unused, -2=waiting for note off to clear light
  int channel;
  int future;
  struct timespec ts;	// last press abs time
} tnote;

static inline long timespec_sub_us(struct timespec *now, struct timespec *prev) {
  return (now->tv_sec - prev->tv_sec) * 1000000 + (now->tv_nsec - prev->tv_nsec) / 1000;
}

// Arduino-assisted NeoPixel-based PianoTutor helper class
class Tutor {
      tnote notes[256]; // addressed by pitch, .velo = -1 means unused
      std::mutex mtx;
      int num_curr_events;
      double coeff;
      bool lit_until_release;

      void clearKeyNoLock(int pitch, bool mark = false);

 public:
      Tutor();
      bool getLitUntilRelease() const { return lit_until_release; }
      void setCoeff(double c) { coeff = c; }
      double getCoeff() const { return coeff; }
      void setLitUntilRelease(bool val) { lit_until_release = val; }
      void addKey(int pitch, int velo, int channel, int future = 0);
      void clearKey(int pitch, bool mark = false);
      void clearKeys(int channel = -1);
      size_t size();
      int keyPressed(int pitch, int velo);
};

#endif
