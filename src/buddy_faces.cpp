#include "buddy_faces.h"

#include <cstdlib>

using esphome::Color;
using esphome::display::Display;

namespace {

constexpr int EL = 40;
constexpr int ER = 88;
constexpr int EY = 24;
constexpr int MX = 64;
constexpr int MY = 50;
constexpr int EYE_R = 10;

constexpr uint32_t TRANSITION_MS = 280;
constexpr uint32_t BLINK_CLOSED_MS = 200;
constexpr uint32_t DIZZY_TO_X_MS = 450;

enum Family : int {
  FAM_NEUTRAL = 0,
  FAM_CUTE,
  FAM_HEART,
  FAM_SLEEP,
  FAM_SAD,
  FAM_WORKING,
  FAM_SURPRISED,
  FAM_DIZZY,
  FAM_FOCUS,
  FAM_BORED,
  FAM_LUNCH,
};

enum Brow : int {
  BROW_NEUTRAL = 0,
  BROW_SOFT,
  BROW_RAISED,
  BROW_ANGRY,  // \  /  (serious)
  BROW_SAD,    // /  \  (worried)
};

int s_current_family = FAM_NEUTRAL;
bool s_in_transition = false;
uint32_t s_transition_until = 0;

bool s_blink_inited = false;
bool s_blink_closed = false;
uint32_t s_blink_next_ms = 0;

int s_gaze = 0;
uint32_t s_gaze_until = 0;

// Bored attention-seeking: rare intentional wink (both not random blink)
bool s_bored_wink = false;
uint32_t s_bored_wink_until = 0;
uint32_t s_bored_next_wink_ms = 0;

// Gallery face list (for review mode)
constexpr int GALLERY[] = {
    STATE_IDLE,         STATE_SMILE,       STATE_SMILE_ACTIVE, STATE_HAPPY,      STATE_HEART_EYES,
    STATE_SURPRISED,    STATE_DIZZY,       STATE_WORKING,      STATE_FOCUS,      STATE_BORED,
    STATE_LUNCH,        STATE_SAD,         STATE_SAD_MAPLE,    STATE_SLEEP,      STATE_SLEEPY_MAPLE,
    STATE_YAWNING_MAPLE, STATE_DIZZY_MAPLE, STATE_HEART_MAPLE,  STATE_THUMBUP_MAPPLE,
};
constexpr int GALLERY_N = sizeof(GALLERY) / sizeof(GALLERY[0]);

uint32_t rand_range(uint32_t lo, uint32_t hi) { return lo + (uint32_t) (random() % (hi - lo + 1)); }

void blink_update(uint32_t now_ms) {
  if (!s_blink_inited) {
    s_blink_inited = true;
    s_blink_closed = false;
    s_blink_next_ms = now_ms + rand_range(1800, 3500);
    return;
  }
  if (now_ms < s_blink_next_ms) {
    return;
  }
  if (!s_blink_closed) {
    s_blink_closed = true;
    s_blink_next_ms = now_ms + BLINK_CLOSED_MS;
  } else {
    s_blink_closed = false;
    s_blink_next_ms = now_ms + rand_range(1800, 3500);
  }
}

void gaze_update(uint32_t now_ms) {
  if (now_ms < s_gaze_until) {
    return;
  }
  const int r = random() % 100;
  if (r < 45) {
    s_gaze = 0;
  } else if (r < 72) {
    s_gaze = -1;
  } else {
    s_gaze = 1;
  }
  s_gaze_until = now_ms + rand_range(1600, 4200);
}

void bored_wink_update(uint32_t now_ms) {
  if (s_bored_wink) {
    if (now_ms >= s_bored_wink_until) {
      s_bored_wink = false;
      s_bored_next_wink_ms = now_ms + rand_range(6000, 12000);
    }
    return;
  }
  if (s_bored_next_wink_ms == 0) {
    s_bored_next_wink_ms = now_ms + rand_range(5000, 9000);
  }
  if (now_ms >= s_bored_next_wink_ms) {
    s_bored_wink = true;
    s_bored_wink_until = now_ms + 350;  // brief "hey look at me"
  }
}

void eye_closed(Display &it, int cx, int cy, int w = 12) {
  it.filled_rectangle(cx - w, cy - 2, w * 2 + 1, 4, Color(1));
}

void apply_brow(Display &it, int cx, int cy, Brow brow) {
  if (brow == BROW_SOFT) {
    it.filled_rectangle(cx - EYE_R - 1, cy - EYE_R - 1, EYE_R * 2 + 3, 4, Color(0));
  } else if (brow == BROW_RAISED) {
    it.filled_rectangle(cx - 3, cy - EYE_R - 1, 7, 2, Color(0));
  }
}

void apply_brow_angled(Display &it, int cx, int cy, bool nose_on_right, bool angry) {
  // angry (serious): \  /   — outer high, nose side low
  // sad:             /  \   — outer low, nose side high
  for (int dx = -EYE_R; dx <= EYE_R; dx++) {
    const int from_nose = nose_on_right ? (EYE_R - dx) : (EYE_R + dx);
    const int from_outer = nose_on_right ? (EYE_R + dx) : (EYE_R - dx);
    const int t = angry ? from_nose : from_outer;
    const int cut = 2 + (t * 5) / (EYE_R * 2);
    for (int dy = 0; dy < cut; dy++) {
      it.draw_pixel_at(cx + dx, cy - EYE_R - 1 + dy, Color(0));
    }
  }
}

void eye_base(Display &it, int cx, int cy, int gaze, Brow brow, bool blink, bool nose_on_right, int radius = EYE_R) {
  if (blink) {
    eye_closed(it, cx, cy);
    return;
  }
  it.filled_circle(cx, cy, radius, Color(1));
  if (brow == BROW_ANGRY || brow == BROW_SAD) {
    apply_brow_angled(it, cx, cy, nose_on_right, brow == BROW_ANGRY);
  } else {
    apply_brow(it, cx, cy, brow);
  }
  it.filled_circle(cx + gaze * 4, cy + 1, 3, Color(0));
}

void eye_pair_base(Display &it, int gaze, Brow brow, bool blink, int y_off = 0, int radius = EYE_R) {
  eye_base(it, EL, EY + y_off, gaze, brow, blink, true, radius);
  eye_base(it, ER, EY + y_off, gaze, brow, blink, false, radius);
}

// Soft thick happy arcs (not extreme /\ )
void eye_happy_curve(Display &it, int cx, int cy) {
  for (int t = 0; t < 3; t++) {
    it.line(cx - 10, cy + 2 + t, cx - 3, cy - 2 + t, Color(1));
    it.line(cx - 3, cy - 2 + t, cx + 3, cy - 2 + t, Color(1));
    it.line(cx + 3, cy - 2 + t, cx + 10, cy + 2 + t, Color(1));
  }
}

// Flat reading squint — almost closed but not a blink line
void eye_flat_squint(Display &it, int cx, int cy, int gaze) {
  it.filled_rectangle(cx - 11, cy - 3, 23, 7, Color(1));
  it.filled_rectangle(cx - 9, cy - 1, 19, 3, Color(0));
  it.filled_circle(cx + gaze * 3, cy, 2, Color(1));
}

void draw_glasses(Display &it) {
  it.circle(EL, EY, EYE_R + 3, Color(1));
  it.circle(ER, EY, EYE_R + 3, Color(1));
  it.line(EL + EYE_R + 3, EY - 1, ER - EYE_R - 3, EY - 1, Color(1));
  it.line(EL + EYE_R + 3, EY, ER - EYE_R - 3, EY, Color(1));
  // temples
  it.line(EL - EYE_R - 3, EY, EL - EYE_R - 8, EY + 2, Color(1));
  it.line(ER + EYE_R + 3, EY, ER + EYE_R + 8, EY + 2, Color(1));
}

void eye_heart(Display &it, int cx, int cy, int scale) {
  const int r = 3 + scale;
  const int drop = 5 + scale * 2;
  it.filled_circle(cx - (2 + scale), cy - (1 + scale / 2), r, Color(1));
  it.filled_circle(cx + (2 + scale), cy - (1 + scale / 2), r, Color(1));
  it.line(cx - (5 + scale), cy + 1, cx, cy + drop, Color(1));
  it.line(cx + (5 + scale), cy + 1, cx, cy + drop, Color(1));
  if (scale >= 1) {
    it.line(cx - (4 + scale), cy + 2, cx, cy + drop - 1, Color(1));
    it.line(cx + (4 + scale), cy + 2, cx, cy + drop - 1, Color(1));
  }
}

void eye_sleepy(Display &it, int cx, int cy) {
  it.line(cx - 10, cy, cx - 4, cy + 4, Color(1));
  it.line(cx - 4, cy + 4, cx + 4, cy + 4, Color(1));
  it.line(cx + 4, cy + 4, cx + 10, cy, Color(1));
  it.line(cx - 10, cy + 1, cx - 4, cy + 5, Color(1));
  it.line(cx + 4, cy + 5, cx + 10, cy + 1, Color(1));
}

void eye_spiral(Display &it, int cx, int cy) {
  // @@ style — concentric rings + offset dot
  it.circle(cx, cy, 9, Color(1));
  it.circle(cx, cy, 6, Color(1));
  it.circle(cx, cy, 3, Color(1));
  it.filled_circle(cx + 2, cy - 2, 2, Color(1));
}

void eye_dizzy_x(Display &it, int cx, int cy) {
  const int s = 8;
  it.line(cx - s, cy - s, cx + s, cy + s, Color(1));
  it.line(cx - s + 1, cy - s, cx + s + 1, cy + s, Color(1));
  it.line(cx - s, cy + s, cx + s, cy - s, Color(1));
  it.line(cx - s + 1, cy + s, cx + s + 1, cy - s, Color(1));
}

void eye_wink(Display &it, int cx, int cy) {
  for (int t = 0; t < 2; t++) {
    it.line(cx - 9, cy + 2 + t, cx + 9, cy - 2 + t, Color(1));
  }
}

void blush(Display &it) {
  for (int i = 0; i < 3; i++) {
    it.line(EL - 2 + i * 3, EY + 14, EL - 4 + i * 3, EY + 18, Color(1));
    it.line(ER - 2 + i * 3, EY + 14, ER + i * 3, EY + 18, Color(1));
  }
}

void mouth_smile(Display &it, int w = 14) {
  for (int t = 0; t < 2; t++) {
    it.line(MX - w, MY - 2 + t, MX - w / 2, MY + 5 + t, Color(1));
    it.line(MX - w / 2, MY + 5 + t, MX + w / 2, MY + 5 + t, Color(1));
    it.line(MX + w / 2, MY + 5 + t, MX + w, MY - 2 + t, Color(1));
  }
}

void mouth_big(Display &it, int w = 18) {
  for (int t = 0; t < 2; t++) {
    it.line(MX - w, MY - 1 + t, MX + w, MY - 1 + t, Color(1));
    it.line(MX - w, MY - 1 + t, MX - w + 2, MY + 7 + t, Color(1));
    it.line(MX + w, MY - 1 + t, MX + w - 2, MY + 7 + t, Color(1));
    it.line(MX - w + 2, MY + 7 + t, MX, MY + 9 + t, Color(1));
    it.line(MX, MY + 9 + t, MX + w - 2, MY + 7 + t, Color(1));
  }
}

void mouth_cat(Display &it) {
  for (int t = 0; t < 2; t++) {
    it.line(MX - 9, MY + t, MX - 2, MY + 6 + t, Color(1));
    it.line(MX - 2, MY + 6 + t, MX, MY + 3 + t, Color(1));
    it.line(MX, MY + 3 + t, MX + 2, MY + 6 + t, Color(1));
    it.line(MX + 2, MY + 6 + t, MX + 9, MY + t, Color(1));
  }
}

void mouth_o(Display &it, int r = 7) {
  it.circle(MX, MY, r, Color(1));
  it.circle(MX, MY, r - 1, Color(1));
}

void mouth_line(Display &it, int w = 9) { it.filled_rectangle(MX - w, MY - 1, w * 2 + 1, 3, Color(1)); }

void mouth_frown(Display &it, int w = 12) {
  for (int t = 0; t < 2; t++) {
    it.line(MX - w, MY + 4 + t, MX - w / 2, MY - 1 + t, Color(1));
    it.line(MX - w / 2, MY - 1 + t, MX + w / 2, MY - 1 + t, Color(1));
    it.line(MX + w / 2, MY - 1 + t, MX + w, MY + 4 + t, Color(1));
  }
}

void mouth_dizzy(Display &it) {
  for (int t = 0; t < 2; t++) {
    it.line(MX - 12, MY + t, MX - 4, MY + 4 + t, Color(1));
    it.line(MX - 4, MY + 4 + t, MX + 4, MY - 4 + t, Color(1));
    it.line(MX + 4, MY - 4 + t, MX + 12, MY + t, Color(1));
  }
}

void tears_waterfall(Display &it) {
  // Very sad — streams under both eyes
  for (int i = 0; i < 3; i++) {
    it.line(EL + 2 + i, EY + 8, EL + 4 + i, EY + 22 + i * 2, Color(1));
    it.line(ER - 2 - i, EY + 8, ER - 4 - i, EY + 22 + i * 2, Color(1));
  }
  it.filled_circle(EL + 5, EY + 24, 2, Color(1));
  it.filled_circle(ER - 5, EY + 24, 2, Color(1));
}

void zzz(Display &it, uint32_t now_ms) {
  const int phase = (now_ms / 800) % 3;
  const int x = ER + 4;
  const int y = 8 + phase;
  it.line(x, y, x + 5, y, Color(1));
  it.line(x + 5, y, x, y + 5, Color(1));
  it.line(x, y + 5, x + 5, y + 5, Color(1));
}

int expression_family(int face) {
  switch (face) {
    case STATE_HEART_EYES:
    case STATE_HEART_EYES_ANIM:
    case STATE_HEART_MAPLE:
      return FAM_HEART;
    case STATE_SMILE:
    case STATE_SMILE_ACTIVE:
    case STATE_HAPPY:
    case STATE_HAPPY_ANIM:
    case STATE_THUMBUP_MAPPLE:
      return FAM_CUTE;
    case STATE_LUNCH:
      return FAM_LUNCH;
    case STATE_SLEEP:
    case STATE_SLEEPY_MAPLE:
      return FAM_SLEEP;
    case STATE_SAD:
    case STATE_SAD_MAPLE:
      return FAM_SAD;
    case STATE_WORKING:
    case STATE_WORKING_MAPLE:
      return FAM_WORKING;
    case STATE_SURPRISED:
      return FAM_SURPRISED;
    case STATE_DIZZY:
    case STATE_DIZZY_MAPLE:
      return FAM_DIZZY;
    case STATE_FOCUS:
      return FAM_FOCUS;
    case STATE_BORED:
    case STATE_BORED_MAPLE:
      return FAM_BORED;
    default:
      return FAM_NEUTRAL;
  }
}

bool face_uses_living_gaze(int face) {
  switch (face) {
    case STATE_IDLE:
    case STATE_SMILE:
    case STATE_LUNCH:
    case STATE_WORKING:
    case STATE_WORKING_MAPLE:
    case STATE_FOCUS:
    case STATE_BORED:
    case STATE_BORED_MAPLE:
    case STATE_YAWNING_MAPLE:
      return true;
    default:
      return false;  // surprised/sad: center; special eyes: n/a
  }
}

void draw_base_mouth(Display &it, int face, uint32_t now_ms, bool mouth_talking) {
  if (mouth_talking) {
    if ((now_ms / 280) % 2) {
      mouth_o(it, 6);
    } else {
      mouth_line(it, 8);
    }
    return;
  }
  // Surprised: mouth opens/closes on its own (gasping)
  if (face == STATE_SURPRISED) {
    if ((now_ms / 320) % 2) {
      mouth_o(it, 7);
    } else {
      mouth_o(it, 4);
    }
    return;
  }
  switch (face) {
    case STATE_DIZZY:
    case STATE_DIZZY_MAPLE:
      mouth_dizzy(it);
      break;
    case STATE_YAWNING_MAPLE:
      mouth_o(it, 8);
      break;
    case STATE_HEART_EYES:
    case STATE_HEART_EYES_ANIM:
    case STATE_HEART_MAPLE:
      mouth_cat(it);
      break;
    case STATE_HAPPY:
    case STATE_HAPPY_ANIM:
    case STATE_THUMBUP_MAPPLE:
    case STATE_SMILE_ACTIVE:
      mouth_big(it, 16);
      break;
    case STATE_SMILE:
    case STATE_LUNCH:
    case STATE_FOCUS:
      mouth_smile(it, 14);
      break;
    case STATE_SAD:
      mouth_frown(it, 12);
      break;
    case STATE_SAD_MAPLE:
      mouth_frown(it, 14);
      break;
    case STATE_SLEEP:
    case STATE_SLEEPY_MAPLE:
    case STATE_BORED:
    case STATE_BORED_MAPLE:
    case STATE_WORKING:
    case STATE_WORKING_MAPLE:
      mouth_line(it, 9);
      break;
    case STATE_IDLE:
    default:
      mouth_smile(it, 12);
      break;
  }
}

void draw_work_eyes(Display &it, uint32_t now_ms, bool blink, int gaze) {
  // Cycle: neutral → serious slant → flat reading squint
  const int variant = (now_ms / 4500) % 3;
  draw_glasses(it);
  if (blink) {
    eye_closed(it, EL, EY);
    eye_closed(it, ER, EY);
    return;
  }
  if (variant == 0) {
    eye_pair_base(it, gaze, BROW_NEUTRAL, false);
  } else if (variant == 1) {
    eye_pair_base(it, gaze, BROW_ANGRY, false);
  } else {
    eye_flat_squint(it, EL, EY, gaze);
    eye_flat_squint(it, ER, EY, gaze);
  }
}

void draw_base_eyes(Display &it, int face, uint32_t now_ms, bool blink, int gaze, bool dizzy_severe,
                    uint32_t dizzy_started_ms) {
  const int hs_raw = (now_ms / 280) % 4;
  const int hs = (hs_raw == 3) ? 1 : hs_raw;

  switch (face) {
    case STATE_DIZZY:
    case STATE_DIZZY_MAPLE: {
      const bool use_x = dizzy_severe && (now_ms - dizzy_started_ms >= DIZZY_TO_X_MS);
      if (use_x) {
        eye_dizzy_x(it, EL, EY);
        eye_dizzy_x(it, ER, EY);
      } else {
        eye_spiral(it, EL, EY);
        eye_spiral(it, ER, EY);
      }
      break;
    }
    case STATE_HEART_EYES:
    case STATE_HEART_EYES_ANIM:
    case STATE_HEART_MAPLE:
      eye_heart(it, EL, EY - 1, hs);
      eye_heart(it, ER, EY - 1, hs);
      break;
    case STATE_HAPPY:
    case STATE_HAPPY_ANIM:
    case STATE_THUMBUP_MAPPLE:
      if (blink) {
        eye_closed(it, EL, EY);
        eye_closed(it, ER, EY);
      } else {
        eye_happy_curve(it, EL, EY);
        eye_happy_curve(it, ER, EY);
      }
      break;
    case STATE_SMILE_ACTIVE:
      if (blink) {
        eye_closed(it, EL, EY);
        eye_closed(it, ER, EY);
      } else {
        eye_base(it, EL, EY, gaze, BROW_NEUTRAL, false, true);
        eye_wink(it, ER, EY);
      }
      break;
    case STATE_SLEEP:
    case STATE_SLEEPY_MAPLE:
      eye_sleepy(it, EL, EY);
      eye_sleepy(it, ER, EY);
      break;
    case STATE_SURPRISED:
      // Center gaze; bigger eyes
      eye_pair_base(it, 0, BROW_RAISED, blink, 0, EYE_R + 1);
      break;
    case STATE_SAD:
      // Less open + sad brows (outer low, nose high)
      eye_pair_base(it, 0, BROW_SAD, blink, 1, 8);
      break;
    case STATE_SAD_MAPLE:
      // Very sad / crying: closed lids + waterfall
      eye_closed(it, EL, EY + 2);
      eye_closed(it, ER, EY + 2);
      tears_waterfall(it);
      break;
    case STATE_WORKING:
    case STATE_WORKING_MAPLE:
      draw_work_eyes(it, now_ms, blink, gaze);
      break;
    case STATE_BORED:
    case STATE_BORED_MAPLE:
      if (s_bored_wink && !blink) {
        eye_base(it, EL, EY, gaze, BROW_SOFT, false, true);
        eye_wink(it, ER, EY);
      } else {
        eye_pair_base(it, gaze, BROW_SOFT, blink);
      }
      break;
    case STATE_FOCUS:
      eye_pair_base(it, gaze, BROW_ANGRY, blink);
      break;
    case STATE_YAWNING_MAPLE:
      eye_pair_base(it, gaze, BROW_RAISED, blink, 0, EYE_R + 1);
      break;
    case STATE_SMILE:
    case STATE_LUNCH:
    case STATE_IDLE:
    default:
      eye_pair_base(it, face_uses_living_gaze(face) ? gaze : 0, BROW_NEUTRAL, blink);
      break;
  }
}

}  // namespace

int buddy_gallery_count() { return GALLERY_N; }

int buddy_gallery_face(int index) {
  if (index < 0) {
    index = 0;
  }
  return GALLERY[index % GALLERY_N];
}

const char *buddy_gallery_label(int index) {
  switch (buddy_gallery_face(index)) {
    case STATE_IDLE:
      return "IDLE";
    case STATE_SMILE:
      return "SMILE";
    case STATE_SMILE_ACTIVE:
      return "WINK";
    case STATE_HAPPY:
      return "HAPPY";
    case STATE_HEART_EYES:
      return "HEART";
    case STATE_SURPRISED:
      return "SURPRISE";
    case STATE_DIZZY:
      return "DIZZY";
    case STATE_WORKING:
      return "WORK";
    case STATE_FOCUS:
      return "FOCUS";
    case STATE_BORED:
      return "BORED";
    case STATE_LUNCH:
      return "LUNCH";
    case STATE_SAD:
      return "SAD";
    case STATE_SAD_MAPLE:
      return "CRY";
    case STATE_SLEEP:
      return "SLEEP";
    case STATE_SLEEPY_MAPLE:
      return "SLEEPY";
    case STATE_YAWNING_MAPLE:
      return "YAWN";
    case STATE_DIZZY_MAPLE:
      return "DIZZY2";
    case STATE_HEART_MAPLE:
      return "HEART2";
    case STATE_THUMBUP_MAPPLE:
      return "THUMB";
    default:
      return "FACE";
  }
}

int buddy_select_face(int current_state, int happiness_level, int random_event_id, uint32_t random_event_end_ms,
                      uint32_t now_ms, int hour) {
  if (current_state != STATE_IDLE) {
    return current_state;
  }
  if (random_event_id != STATE_NONE && now_ms < random_event_end_ms) {
    return STATE_EVENT_BASE + random_event_id;
  }
  if (happiness_level >= 3) {
    return STATE_HEART_EYES;
  }
  if (happiness_level == 2) {
    return STATE_HAPPY;
  }
  if (happiness_level == 1) {
    return STATE_SMILE;
  }
  if (hour >= 7 && hour < 9) {
    return STATE_SLEEPY_MAPLE;
  }
  if (hour >= 9 && hour < 12) {
    return STATE_WORKING;
  }
  if (hour >= 12 && hour < 13) {
    return STATE_LUNCH;
  }
  if (hour >= 13 && hour < 18) {
    return ((now_ms / 12000) % 2) ? STATE_FOCUS : STATE_BORED;
  }
  if (hour >= 18 && hour < 22) {
    return STATE_SAD;
  }
  if (hour >= 22 || hour < 7) {
    return STATE_SLEEP;
  }
  return STATE_IDLE;
}

int buddy_present_face(int target_face, uint32_t now_ms) {
  const int target_fam = expression_family(target_face);

  if (s_in_transition) {
    if (now_ms >= s_transition_until) {
      s_in_transition = false;
      s_current_family = target_fam;
      return target_face;
    }
    return STATE_IDLE;
  }

  if (target_fam != s_current_family) {
    s_in_transition = true;
    s_transition_until = now_ms + TRANSITION_MS;
    return STATE_IDLE;
  }

  return target_face;
}

void buddy_draw_face(Display &it, int face, uint32_t now_ms, bool mouth_talking, bool dizzy_severe,
                     uint32_t dizzy_started_ms) {
  blink_update(now_ms);
  if (face_uses_living_gaze(face) || face == STATE_SMILE_ACTIVE) {
    gaze_update(now_ms);
  }
  if (face == STATE_BORED || face == STATE_BORED_MAPLE) {
    bored_wink_update(now_ms);
  }

  draw_base_eyes(it, face, now_ms, s_blink_closed, s_gaze, dizzy_severe, dizzy_started_ms);

  if (face == STATE_HEART_EYES || face == STATE_HEART_EYES_ANIM || face == STATE_HEART_MAPLE || face == STATE_HAPPY ||
      face == STATE_HAPPY_ANIM || face == STATE_THUMBUP_MAPPLE || face == STATE_SMILE_ACTIVE || face == STATE_SMILE ||
      face == STATE_LUNCH) {
    blush(it);
  }

  if (face == STATE_SLEEP || face == STATE_SLEEPY_MAPLE) {
    it.line(MX + 6, MY + 2, MX + 6, MY + 8, Color(1));
    it.filled_circle(MX + 6, MY + 9, 1, Color(1));
    zzz(it, now_ms);
  }

  draw_base_mouth(it, face, now_ms, mouth_talking);
}
