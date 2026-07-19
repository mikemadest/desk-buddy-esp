#pragma once

// Face / emotion state constants (compile-time — not ESPHome globals).
// Active: SMILE_ACTIVE, SURPRISED, DIZZY
// Happiness: SMILE, HAPPY(+ANIM), HEART_EYES(+ANIM)
// Time-of-day: WORKING, LUNCH, BORED, FOCUS, SAD, SLEEP
// Idle events (EVENT_BASE + 0..7): SLEEPY/DIZZY/HEART/YAWNING/WORKING/SAD/BORED/THUMBUP maple

enum : int {
  STATE_NONE = -1,
  STATE_IDLE = 0,
  STATE_WORKING = 2,
  STATE_LUNCH = 3,
  STATE_BORED = 4,
  STATE_FOCUS = 5,
  STATE_SAD = 6,
  STATE_SLEEP = 7,
  STATE_SMILE_ACTIVE = 10,
  STATE_SURPRISED = 13,
  STATE_DIZZY = 14,
  STATE_SMILE = 15,
  STATE_HAPPY = 16,
  STATE_HAPPY_ANIM = 17,
  STATE_HEART_EYES = 21,
  STATE_HEART_EYES_ANIM = 22,
  STATE_EVENT_BASE = 29,
  STATE_SLEEPY_MAPLE = 29,
  STATE_DIZZY_MAPLE = 30,
  STATE_HEART_MAPLE = 31,
  STATE_YAWNING_MAPLE = 32,
  STATE_WORKING_MAPLE = 33,
  STATE_SAD_MAPLE = 34,
  STATE_BORED_MAPLE = 35,
  STATE_THUMBUP_MAPPLE = 36,
  // Temperature comfort (selected from room sensor when idle)
  STATE_HOT_MILD = 40,     // >25°C: 2–3 sweat drops
  STATE_HOT_WARM = 41,     // 28–30°C: 3–4 sweat drops
  STATE_HOT_OVER = 42,     // >30°C: X eyes + sweat
  STATE_COLD_CHILLY = 43,  // 18–19°C: small serious eyes, inverse mouth
  STATE_COLD_SAD = 44,     // ~17–18°C: sad eyes + inverse grin
  STATE_COLD_FREEZE = 45,  // <17°C: X eyes + inverse grin
  // Internal presentation-only state used between expression families.
  STATE_TRANSITION_BLINK = 90,
};

// Returns a label for active interaction states; nullptr = keep previous / unknown.
inline const char *buddy_emotion_label(int s) {
  switch (s) {
    case STATE_IDLE:
      return "IDLE";
    case STATE_SMILE_ACTIVE:
    case STATE_SMILE:
      return "SMILE :)";
    case STATE_SURPRISED:
      return "SURPRISED O.O";
    case STATE_DIZZY:
      return "DIZZY @.@";
    case STATE_HAPPY:
    case STATE_HAPPY_ANIM:
      return "HAPPY :D";
    case STATE_HEART_EYES:
    case STATE_HEART_EYES_ANIM:
      return "HEART EYES <3";
    default:
      return nullptr;
  }
}
