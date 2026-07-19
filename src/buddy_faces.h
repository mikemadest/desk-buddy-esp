#pragma once

#include <cstdint>

#include "esphome/components/display/display.h"
#include "buddy_states.h"

inline void buddy_expire_random_event(int &random_event_id, uint32_t end_ms, uint32_t now_ms) {
  if (random_event_id != STATE_NONE && now_ms >= end_ms) {
    random_event_id = STATE_NONE;
  }
}

int buddy_select_face(int current_state, int happiness_level, int random_event_id, uint32_t random_event_end_ms,
                      uint32_t now_ms, int hour);

int buddy_present_face(int target_face, uint32_t now_ms);

// Face gallery (triple-click mode): cycle expressions for review.
int buddy_gallery_count();
int buddy_gallery_face(int index);
const char *buddy_gallery_label(int index);

// dizzy_severe: true → @@ then XX; false → stay @@. dizzy_started_ms for phase timing.
void buddy_draw_face(esphome::display::Display &it, int face, uint32_t now_ms, bool mouth_talking, bool dizzy_severe,
                     uint32_t dizzy_started_ms);
