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
constexpr uint32_t WINK_CLOSED_MS = 400;
constexpr uint32_t SLEEPY_BLINK_OPEN_LO = 700;
constexpr uint32_t SLEEPY_BLINK_OPEN_HI = 1600;
constexpr uint32_t SLEEPY_BLINK_CLOSED_LO = 400;
constexpr uint32_t SLEEPY_BLINK_CLOSED_HI = 800;

// Eye openness: how much of the globe the lids leave visible (cartoon, not photo-real).
enum Openness : int {
  OPEN_SQUINT = 0,  // reading / tired — slit of sclera + iris
  OPEN_SOFT = 1,    // bored / sleepy-ish
  OPEN_NORMAL = 2,  // default: lids clip top & bottom, iris peeking under upper lid
  OPEN_WIDE = 3,    // almost full (still a hint of lid)
  OPEN_ROUND = 4,   // surprise only — full globe, lids "gone"
};

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
  FAM_HOT,
  FAM_COLD,
};

// Sleepy (not deep sleep) cycle phases
enum SleepyPhase : int {
  SLEEPY_DROWSY = 0,
  SLEEPY_YAWN = 1,
  SLEEPY_ASLEEP = 2,
};

// Brow mood (white line above the globe):
//   / \ family: tiny+arched = normal/happy; strong+straighter = sad
//   \ / family: medium = focus; strong = angry
//   IDLE: living arch that gently tips / \ then back (subtle)
enum Brow : int {
  BROW_IDLE = 0,
  BROW_HAPPY,
  BROW_SOFT,
  BROW_SAD,
  BROW_FOCUS,
  BROW_ANGRY,
  BROW_RAISED,
};

int s_current_family = FAM_NEUTRAL;
bool s_in_transition = false;
uint32_t s_transition_until = 0;

bool s_blink_inited = false;
bool s_blink_closed = false;
uint32_t s_blink_next_ms = 0;

int s_gaze = 0;
uint32_t s_gaze_until = 0;

bool s_bored_wink = false;
uint32_t s_bored_wink_until = 0;
uint32_t s_bored_next_wink_ms = 0;

// Per-face animation clock (wink / surprise mouth / gallery loops)
int s_anim_face = STATE_NONE;
uint32_t s_anim_face_ms = 0;

constexpr int GALLERY[] = {
    STATE_IDLE,          STATE_SMILE,        STATE_SMILE_ACTIVE, STATE_HAPPY,       STATE_HEART_EYES,
    STATE_SURPRISED,     STATE_DIZZY,        STATE_WORKING,      STATE_FOCUS,       STATE_BORED,
    STATE_LUNCH,         STATE_SAD,          STATE_SAD_MAPLE,    STATE_SLEEP,       STATE_SLEEPY_MAPLE,
    STATE_YAWNING_MAPLE, STATE_HOT_MILD,     STATE_HOT_WARM,     STATE_HOT_OVER,    STATE_COLD_CHILLY,
    STATE_COLD_SAD,      STATE_COLD_FREEZE,  STATE_HEART_MAPLE,  STATE_THUMBUP_MAPPLE,
};
constexpr int GALLERY_N = sizeof(GALLERY) / sizeof(GALLERY[0]);

// 16-way unit vectors * 16 for smoother spiral rotation (no float)
constexpr int8_t COS16[] = {16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6, 0, 6, 11, 15};
constexpr int8_t SIN16[] = {0, 6, 11, 15, 16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6};

uint32_t rand_range(uint32_t lo, uint32_t hi) { return lo + (uint32_t) (random() % (hi - lo + 1)); }

void face_anim_sync(int face, uint32_t now_ms) {
  if (face != s_anim_face) {
    s_anim_face = face;
    s_anim_face_ms = now_ms;
  }
}

uint32_t face_age(uint32_t now_ms) { return now_ms - s_anim_face_ms; }

void blink_update(uint32_t now_ms, bool sleepy_mode) {
  if (!s_blink_inited) {
    s_blink_inited = true;
    s_blink_closed = false;
    s_blink_next_ms = now_ms + (sleepy_mode ? rand_range(SLEEPY_BLINK_OPEN_LO, SLEEPY_BLINK_OPEN_HI)
                                             : rand_range(1800, 3500));
    return;
  }
  if (now_ms < s_blink_next_ms) {
    return;
  }
  if (!s_blink_closed) {
    s_blink_closed = true;
    s_blink_next_ms =
        now_ms + (sleepy_mode ? rand_range(SLEEPY_BLINK_CLOSED_LO, SLEEPY_BLINK_CLOSED_HI) : BLINK_CLOSED_MS);
  } else {
    s_blink_closed = false;
    s_blink_next_ms = now_ms + (sleepy_mode ? rand_range(SLEEPY_BLINK_OPEN_LO, SLEEPY_BLINK_OPEN_HI)
                                            : rand_range(1800, 3500));
  }
}

SleepyPhase sleepy_phase(uint32_t now_ms) {
  // drowsy → yawn → brief sleep face → drowsy (loop)
  constexpr uint32_t LOOP = 6500;
  const uint32_t a = face_age(now_ms) % LOOP;
  if (a < 1800) {
    return SLEEPY_DROWSY;
  }
  if (a < 3200) {
    return SLEEPY_YAWN;
  }
  if (a < 4800) {
    return SLEEPY_ASLEEP;
  }
  return SLEEPY_DROWSY;
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
    s_bored_wink_until = now_ms + WINK_CLOSED_MS;
  }
}

void eye_closed(Display &it, int cx, int cy, int w = 12) {
  it.filled_rectangle(cx - w, cy - 2, w * 2 + 1, 4, Color(1));
}

// White 2px brow: outer → mid (arch) → nose. tip>0 with dir+/ \ or dir-\ /
void draw_eyebrow(Display &it, int cx, int cy, bool nose_on_right, Brow brow, int radius, uint32_t now_ms) {
  int y_base = cy - radius;
  const int x_outer = nose_on_right ? (cx - radius + 1) : (cx + radius - 1);
  const int x_nose = nose_on_right ? (cx + radius + 1) : (cx - radius - 1);
  const int x_mid = (x_outer + x_nose) / 2;

  // dir: +1 = / \ , -1 = \ / , 0 = level
  int dir = 0;
  int tip = 0;   // angle strength in px
  int arch = 2;  // mid raised (round-ish). 0 = straighter (strong emotion)

  switch (brow) {
    case BROW_IDLE: {
      // Subtle living brow: arch, gently tips / \ then back (not too strong)
      const uint32_t w = (now_ms / 55) % 140;
      int t = (w < 70) ? (int) (w / 35) : (int) ((140 - w) / 35);  // 0..2
      if (t > 2) {
        t = 2;
      }
      dir = 1;
      tip = t;
      arch = 2;
      break;
    }
    case BROW_HAPPY:
      dir = 1;
      tip = 1;
      arch = 2;
      break;
    case BROW_SOFT:
      dir = 1;
      tip = 1;
      arch = 1;
      y_base += 1;
      break;
    case BROW_SAD:
      dir = 1;
      tip = 3;
      arch = 0;  // straighter, stronger sad slant
      break;
    case BROW_FOCUS:
      dir = -1;
      tip = 2;
      arch = 1;
      break;
    case BROW_ANGRY:
      dir = -1;
      tip = 3;
      arch = 0;
      break;
    case BROW_RAISED:
      dir = 0;
      tip = 0;
      arch = 2;
      y_base -= 3;
      break;
  }

  int y_outer = y_base;
  int y_nose = y_base;
  if (dir > 0) {
    y_outer = y_base + (tip + 1) / 2;
    y_nose = y_base - tip;
  } else if (dir < 0) {
    y_outer = y_base - tip;
    y_nose = y_base + (tip + 1) / 2;
  }
  const int y_mid = ((y_outer + y_nose) / 2) - arch;

  for (int t = 0; t < 2; t++) {
    it.line(x_outer, y_outer + t, x_mid, y_mid + t, Color(1));
    it.line(x_mid, y_mid + t, x_nose, y_nose + t, Color(1));
  }
}

// Lid cut: erase top of sclera. Normal/smile: rounder top (less cut in center).
void apply_upper_lid(Display &it, int cx, int cy, int radius, Brow brow, bool nose_on_right, Openness open) {
  if (open == OPEN_ROUND) {
    return;
  }
  int base_cut = 2;
  if (open == OPEN_WIDE) {
    base_cut = 1;
  } else if (open == OPEN_SOFT) {
    base_cut = 4;
  } else if (open == OPEN_SQUINT) {
    base_cut = 6;
  }

  for (int dx = -radius; dx <= radius; dx++) {
    int cut = base_cut;
    if (open == OPEN_NORMAL || open == OPEN_WIDE) {
      // Rounder top of the eye: more lid at the sides, less in the middle
      cut = base_cut + (abs(dx) * 2) / radius;
    }
    if (brow == BROW_ANGRY || brow == BROW_FOCUS || brow == BROW_SAD) {
      const int from_nose = nose_on_right ? (radius - dx) : (radius + dx);
      const int from_outer = nose_on_right ? (radius + dx) : (radius - dx);
      const int t = (brow == BROW_SAD) ? from_nose : from_outer;
      cut = base_cut + (t * 3) / (radius * 2);
    }
    for (int dy = 0; dy < cut; dy++) {
      it.draw_pixel_at(cx + dx, cy - radius - 1 + dy, Color(0));
    }
  }
}

void apply_lower_lid(Display &it, int cx, int cy, int radius, Openness open) {
  if (open == OPEN_ROUND || open == OPEN_WIDE) {
    return;
  }
  int cut = 2;
  if (open == OPEN_SOFT) {
    cut = 3;
  } else if (open == OPEN_SQUINT) {
    cut = 5;
  }
  for (int dx = -radius + 1; dx <= radius - 1; dx++) {
    for (int dy = 0; dy < cut; dy++) {
      it.draw_pixel_at(cx + dx, cy + radius - dy, Color(0));
    }
  }
}

// Globe in socket: white sclera, lids clip, black iris only (never fill the white).
// iris_r is per-expression (subtle): small=surprise, mid=idle, big=happy/work/focus.
void eye_globe(Display &it, int cx, int cy, int gaze, Brow brow, bool blink, bool nose_on_right, Openness open,
               uint32_t now_ms, int radius = EYE_R, int iris_r = 3) {
  if (blink) {
    eye_closed(it, cx, cy, radius + 2);
    draw_eyebrow(it, cx, cy, nose_on_right, brow, radius, now_ms);
    return;
  }

  it.filled_circle(cx, cy, radius, Color(1));
  apply_upper_lid(it, cx, cy, radius, brow, nose_on_right, open);
  apply_lower_lid(it, cx, cy, radius, open);

  int r = iris_r;
  if (open == OPEN_SQUINT && r > 2) {
    r = 2;
  }
  const int ix = cx + gaze * 3;
  const int iy = cy + ((open == OPEN_ROUND) ? 0 : 1);
  it.filled_circle(ix, iy, r, Color(0));

  draw_eyebrow(it, cx, cy, nose_on_right, brow, radius, now_ms);
}

void eye_pair(Display &it, int gaze, Brow brow, bool blink, Openness open, uint32_t now_ms, int y_off = 0,
              int radius = EYE_R, int iris_r = 3) {
  eye_globe(it, EL, EY + y_off, gaze, brow, blink, true, open, now_ms, radius, iris_r);
  eye_globe(it, ER, EY + y_off, gaze, brow, blink, false, open, now_ms, radius, iris_r);
}

void eye_happy_curve(Display &it, int cx, int cy) {
  // Soft upside-down U — round peak, not a flat top
  for (int t = 0; t < 3; t++) {
    it.line(cx - 10, cy + 3 + t, cx - 5, cy - 1 + t, Color(1));
    it.line(cx - 5, cy - 1 + t, cx, cy - 3 + t, Color(1));
    it.line(cx, cy - 3 + t, cx + 5, cy - 1 + t, Color(1));
    it.line(cx + 5, cy - 1 + t, cx + 10, cy + 3 + t, Color(1));
  }
}

// Reading squint: narrow white band + black iris (sclera stays white)
void eye_flat_squint(Display &it, int cx, int cy, int gaze, bool nose_on_right, uint32_t now_ms) {
  eye_globe(it, cx, cy, gaze, BROW_SOFT, false, nose_on_right, OPEN_SQUINT, now_ms, EYE_R, 2);
}

// Rounded-rectangle outline (square frames, soft corners) — outline only, never filled
void rounded_rect_outline(Display &it, int cx, int cy, int hw, int hh, int rad) {
  const int x0 = cx - hw;
  const int x1 = cx + hw;
  const int y0 = cy - hh;
  const int y1 = cy + hh;
  // edges
  it.line(x0 + rad, y0, x1 - rad, y0, Color(1));
  it.line(x0 + rad, y1, x1 - rad, y1, Color(1));
  it.line(x0, y0 + rad, x0, y1 - rad, Color(1));
  it.line(x1, y0 + rad, x1, y1 - rad, Color(1));
  it.line(x0 + rad, y0 + 1, x1 - rad, y0 + 1, Color(1));
  it.line(x0 + rad, y1 - 1, x1 - rad, y1 - 1, Color(1));
  it.line(x0 + 1, y0 + rad, x0 + 1, y1 - rad, Color(1));
  it.line(x1 - 1, y0 + rad, x1 - 1, y1 - rad, Color(1));
  // Rounded corners
  for (int i = 0; i <= rad; i++) {
    for (int j = 0; j <= rad; j++) {
      if (i * i + j * j >= (rad - 1) * (rad - 1) && i * i + j * j <= rad * rad + rad) {
        it.draw_pixel_at(x0 + rad - i, y0 + rad - j, Color(1));
        it.draw_pixel_at(x1 - rad + i, y0 + rad - j, Color(1));
        it.draw_pixel_at(x0 + rad - i, y1 - rad + j, Color(1));
        it.draw_pixel_at(x1 - rad + i, y1 - rad + j, Color(1));
      }
    }
  }
}

void draw_glasses(Display &it) {
  // Square-ish lenses with round corners; larger than the eye so frames aren't "filled"
  constexpr int HW = 14;
  constexpr int HH = 12;
  constexpr int RAD = 4;
  rounded_rect_outline(it, EL, EY, HW, HH, RAD);
  rounded_rect_outline(it, ER, EY, HW, HH, RAD);
  // bridge
  it.line(EL + HW, EY - 1, ER - HW, EY - 1, Color(1));
  it.line(EL + HW, EY, ER - HW, EY, Color(1));
  // temples
  it.line(EL - HW, EY, EL - HW - 6, EY + 2, Color(1));
  it.line(ER + HW, EY, ER + HW + 6, EY + 2, Color(1));
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

// Pure cartoon dizzy: white spiral only (no eyeball), bigger, slower
void eye_spiral_rotating(Display &it, int cx, int cy, uint32_t now_ms) {
  constexpr int R = 12;
  const int phase = (now_ms / 140) % 16;
  int prev_x = cx;
  int prev_y = cy;
  for (int i = 0; i < 40; i++) {
    const int a = (phase + i) % 16;
    const int r = 2 + (i * (R - 2)) / 39;
    const int x = cx + (COS16[a] * r) / 16;
    const int y = cy + (SIN16[a] * r) / 16;
    for (int t = -1; t <= 1; t++) {
      it.line(prev_x, prev_y + t, x, y + t, Color(1));
    }
    prev_x = x;
    prev_y = y;
  }
}

void eye_x(Display &it, int cx, int cy, int s = 8) {
  it.line(cx - s, cy - s, cx + s, cy + s, Color(1));
  it.line(cx - s + 1, cy - s, cx + s + 1, cy + s, Color(1));
  it.line(cx - s, cy + s, cx + s, cy - s, Color(1));
  it.line(cx - s + 1, cy + s, cx + s + 1, cy - s, Color(1));
}

// Small sweat drops (teardrop) — count 2..4
void draw_sweat(Display &it, int count) {
  struct Drop {
    int x, y;
  };
  const Drop drops[] = {
      {EL - 14, EY + 2}, {ER + 12, EY + 4}, {EL - 10, EY + 14}, {ER + 8, EY + 16},
  };
  const int n = (count < 2) ? 2 : ((count > 4) ? 4 : count);
  for (int i = 0; i < n; i++) {
    const int x = drops[i].x;
    const int y = drops[i].y;
    it.draw_pixel_at(x, y, Color(1));
    it.line(x, y + 1, x, y + 4, Color(1));
    it.draw_pixel_at(x - 1, y + 3, Color(1));
    it.draw_pixel_at(x + 1, y + 3, Color(1));
    it.filled_circle(x, y + 5, 1, Color(1));
  }
}

void eye_wink_lid(Display &it, int cx, int cy) {
  for (int t = 0; t < 2; t++) {
    it.line(cx - 9, cy + 1 + t, cx + 9, cy - 1 + t, Color(1));
  }
}

void blush(Display &it) {
  for (int i = 0; i < 3; i++) {
    it.line(EL - 2 + i * 3, EY + 14, EL - 4 + i * 3, EY + 18, Color(1));
    it.line(ER - 2 + i * 3, EY + 14, ER + i * 3, EY + 18, Color(1));
  }
}

// Wide, thick, round smile (smooth arc — not angular 3-segment)
void mouth_smile(Display &it, int w = 18, int h = 7) {
  for (int t = 0; t < 3; t++) {
    int prev_x = MX - w;
    int prev_y = MY - 1 + t;
    for (int i = 1; i <= 14; i++) {
      const int x = MX - w + (2 * w * i) / 14;
      const int xn = x - MX;
      const int y = MY - 1 + t + (h * (w * w - xn * xn)) / (w * w);
      it.line(prev_x, prev_y, x, y, Color(1));
      prev_x = x;
      prev_y = y;
    }
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

// Yawn open amount 0..100 (in → hold 2s → out). Loops in gallery.
int yawn_progress(uint32_t now_ms) {
  constexpr uint32_t IN_MS = 700;
  constexpr uint32_t HOLD_MS = 2000;
  constexpr uint32_t OUT_MS = 700;
  constexpr uint32_t GAP_MS = 500;
  constexpr uint32_t LOOP = IN_MS + HOLD_MS + OUT_MS + GAP_MS;
  const uint32_t a = face_age(now_ms) % LOOP;
  if (a < IN_MS) {
    return (int) ((100 * a) / IN_MS);
  }
  if (a < IN_MS + HOLD_MS) {
    return 100;
  }
  if (a < IN_MS + HOLD_MS + OUT_MS) {
    return 100 - (int) ((100 * (a - IN_MS - HOLD_MS)) / OUT_MS);
  }
  return 0;
}

// Wide yawn oval scaled by progress 0..100
void mouth_yawn(Display &it, int progress) {
  if (progress <= 8) {
    mouth_line(it, 9);
    return;
  }
  if (progress < 35) {
    mouth_o(it, 3 + (progress * 4) / 35);
    return;
  }
  const int RX = 6 + (progress * 7) / 100;   // up to ~13
  const int RY = 5 + (progress * 7) / 100;   // up to ~12
  for (int t = 0; t < 2; t++) {
    const int rx = RX - t;
    const int ry = RY - t;
    if (rx < 2 || ry < 2) {
      continue;
    }
    for (int x = -rx; x <= rx; x++) {
      const int xx = rx * rx - x * x;
      int y = 0;
      while ((y + 1) * (y + 1) * rx * rx <= xx * ry * ry) {
        y++;
      }
      it.draw_pixel_at(MX + x, MY - y, Color(1));
      it.draw_pixel_at(MX + x, MY + y, Color(1));
    }
    for (int y = -ry; y <= ry; y++) {
      const int yy = ry * ry - y * y;
      int x = 0;
      while ((x + 1) * (x + 1) * ry * ry <= yy * rx * rx) {
        x++;
      }
      it.draw_pixel_at(MX - x, MY + y, Color(1));
      it.draw_pixel_at(MX + x, MY + y, Color(1));
    }
  }
}

void tears_waterfall(Display &it) {
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
    case STATE_HOT_MILD:
    case STATE_HOT_WARM:
    case STATE_HOT_OVER:
      return FAM_HOT;
    case STATE_COLD_CHILLY:
    case STATE_COLD_SAD:
    case STATE_COLD_FREEZE:
      return FAM_COLD;
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
      return true;
    default:
      return false;
  }
}

// Surprise: line → open O → hold 1s → close. Loops in gallery.
void mouth_surprised_anim(Display &it, uint32_t now_ms) {
  constexpr uint32_t OPEN_MS = 400;
  constexpr uint32_t HOLD_MS = 1000;
  constexpr uint32_t CLOSE_MS = 350;
  constexpr uint32_t LOOP_MS = OPEN_MS + HOLD_MS + CLOSE_MS + 400;
  uint32_t age = face_age(now_ms) % LOOP_MS;

  if (age < OPEN_MS) {
    const int r = 2 + (int) ((6 * age) / OPEN_MS);
    if (r < 3) {
      mouth_line(it, 8);
    } else {
      mouth_o(it, r);
    }
  } else if (age < OPEN_MS + HOLD_MS) {
    mouth_o(it, 8);
  } else if (age < OPEN_MS + HOLD_MS + CLOSE_MS) {
    const uint32_t t = age - OPEN_MS - HOLD_MS;
    const int r = 8 - (int) ((6 * t) / CLOSE_MS);
    if (r < 3) {
      mouth_line(it, 8);
    } else {
      mouth_o(it, r);
    }
  } else {
    mouth_line(it, 8);
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
  if (face == STATE_SURPRISED) {
    mouth_surprised_anim(it, now_ms);
    return;
  }
  switch (face) {
    case STATE_DIZZY:
    case STATE_DIZZY_MAPLE:
      mouth_dizzy(it);
      break;
    case STATE_YAWNING_MAPLE:
      mouth_yawn(it, yawn_progress(now_ms));
      break;
    case STATE_HEART_EYES:
    case STATE_HEART_EYES_ANIM:
    case STATE_HEART_MAPLE:
      mouth_cat(it);
      break;
    case STATE_HAPPY:
    case STATE_HAPPY_ANIM:
    case STATE_THUMBUP_MAPPLE:
      mouth_big(it, 16);
      break;
    case STATE_SMILE_ACTIVE:
      mouth_big(it, 14);
      break;
    case STATE_SMILE:
    case STATE_LUNCH:
      mouth_smile(it, 18, 7);
      break;
    case STATE_SAD:
      mouth_frown(it, 12);
      break;
    case STATE_SAD_MAPLE:
      mouth_frown(it, 14);
      break;
    case STATE_COLD_CHILLY:
      mouth_frown(it, 11);
      break;
    case STATE_COLD_SAD:
    case STATE_COLD_FREEZE:
      mouth_frown(it, 14);  // inverse-curve "cold grin"
      break;
    case STATE_SLEEPY_MAPLE: {
      const SleepyPhase sp = sleepy_phase(now_ms);
      if (sp == SLEEPY_YAWN) {
        // Map drowsy-cycle yawn window onto 0..100 yawn mouth
        constexpr uint32_t LOOP = 6500;
        const uint32_t a = face_age(now_ms) % LOOP;
        const uint32_t local = a - 1800;
        int p = 0;
        if (local < 400) {
          p = (int) ((100 * local) / 400);
        } else if (local < 1000) {
          p = 100;
        } else {
          p = 100 - (int) ((100 * (local - 1000)) / 400);
        }
        mouth_yawn(it, p);
      } else if (sp == SLEEPY_ASLEEP) {
        mouth_line(it, 7);
      } else {
        mouth_line(it, 9);
      }
      break;
    }
    case STATE_HOT_MILD:
    case STATE_HOT_WARM:
    case STATE_HOT_OVER:
    case STATE_FOCUS:
    case STATE_SLEEP:
    case STATE_BORED:
    case STATE_BORED_MAPLE:
    case STATE_WORKING:
    case STATE_WORKING_MAPLE:
    case STATE_IDLE:
    default:
      mouth_line(it, 9);
      break;
  }
}

void draw_work_eyes(Display &it, uint32_t now_ms, bool blink, int gaze) {
  // Cycle: soft happy brow → focus \ / → reading squint (iris a bit bigger when interested)
  const int variant = (now_ms / 4500) % 3;
  if (blink) {
    eye_closed(it, EL, EY);
    eye_closed(it, ER, EY);
  } else if (variant == 0) {
    eye_pair(it, gaze, BROW_HAPPY, false, OPEN_NORMAL, now_ms, 0, EYE_R, 4);
  } else if (variant == 1) {
    eye_pair(it, gaze, BROW_FOCUS, false, OPEN_NORMAL, now_ms, 0, EYE_R, 4);
  } else {
    eye_flat_squint(it, EL, EY, gaze, true, now_ms);
    eye_flat_squint(it, ER, EY, gaze, false, now_ms);
  }
  draw_glasses(it);  // outline on top so frames read clearly
}

// Wink: both open → right closed 400ms → both open (loops every ~2s in gallery)
bool wink_right_closed(uint32_t now_ms) {
  constexpr uint32_t PRE_MS = 280;
  constexpr uint32_t LOOP_MS = PRE_MS + WINK_CLOSED_MS + 900;
  const uint32_t age = face_age(now_ms) % LOOP_MS;
  return age >= PRE_MS && age < PRE_MS + WINK_CLOSED_MS;
}

void draw_base_eyes(Display &it, int face, uint32_t now_ms, bool blink, int gaze, bool /*dizzy_severe*/,
                    uint32_t /*dizzy_started_ms*/) {
  const int hs_raw = (now_ms / 280) % 4;
  const int hs = (hs_raw == 3) ? 1 : hs_raw;

  switch (face) {
    case STATE_DIZZY:
    case STATE_DIZZY_MAPLE:
      eye_spiral_rotating(it, EL, EY, now_ms);
      eye_spiral_rotating(it, ER, EY, now_ms);
      break;
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
      } else if (wink_right_closed(now_ms)) {
        eye_globe(it, EL, EY, gaze, BROW_HAPPY, false, true, OPEN_NORMAL, now_ms, EYE_R, 4);
        eye_wink_lid(it, ER, EY);
        draw_eyebrow(it, ER, EY, false, BROW_HAPPY, EYE_R, now_ms);
      } else {
        eye_pair(it, gaze, BROW_HAPPY, false, OPEN_NORMAL, now_ms, 0, EYE_R, 4);
      }
      break;
    case STATE_SLEEP:
      eye_sleepy(it, EL, EY);
      eye_sleepy(it, ER, EY);
      break;
    case STATE_SLEEPY_MAPLE: {
      const SleepyPhase sp = sleepy_phase(now_ms);
      if (sp == SLEEPY_ASLEEP) {
        eye_sleepy(it, EL, EY);
        eye_sleepy(it, ER, EY);
      } else if (sp == SLEEPY_YAWN) {
        eye_closed(it, EL, EY);
        eye_closed(it, ER, EY);
      } else {
        // Drowsy: less open than idle, mild / \ brows; long blinks from blink_update
        eye_pair(it, gaze, BROW_SOFT, blink, OPEN_SOFT, now_ms, 0, 9, 3);
      }
      break;
    }
    case STATE_SURPRISED:
      // Smaller iris — more white, startled look
      eye_pair(it, 0, BROW_RAISED, blink, OPEN_ROUND, now_ms, 0, EYE_R + 1, 2);
      break;
    case STATE_SAD:
      eye_pair(it, 0, BROW_SAD, blink, OPEN_SOFT, now_ms, 1, 8, 3);
      break;
    case STATE_SAD_MAPLE:
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
        eye_globe(it, EL, EY, gaze, BROW_SOFT, false, true, OPEN_SOFT, now_ms, EYE_R, 3);
        eye_wink_lid(it, ER, EY);
        draw_eyebrow(it, ER, EY, false, BROW_SOFT, EYE_R, now_ms);
      } else {
        eye_pair(it, gaze, BROW_SOFT, blink, OPEN_SOFT, now_ms, 0, EYE_R, 3);
      }
      break;
    case STATE_FOCUS:
      eye_pair(it, gaze, BROW_FOCUS, blink, OPEN_NORMAL, now_ms, 0, EYE_R, 4);
      break;
    case STATE_YAWNING_MAPLE: {
      const int yp = yawn_progress(now_ms);
      if (yp >= 75) {
        eye_closed(it, EL, EY);
        eye_closed(it, ER, EY);
      } else if (yp >= 45) {
        eye_pair(it, gaze, BROW_SOFT, false, OPEN_SQUINT, now_ms, 0, EYE_R, 2);
      } else if (yp >= 20) {
        eye_pair(it, gaze, BROW_SOFT, false, OPEN_SOFT, now_ms, 0, EYE_R, 3);
      } else {
        eye_pair(it, gaze, BROW_IDLE, false, OPEN_NORMAL, now_ms, 0, EYE_R, 3);
      }
      break;
    }
    case STATE_SMILE:
      eye_pair(it, gaze, BROW_HAPPY, blink, OPEN_NORMAL, now_ms, 0, EYE_R, 4);
      break;
    case STATE_LUNCH:
      eye_pair(it, gaze, BROW_HAPPY, blink, OPEN_NORMAL, now_ms, 0, EYE_R, 4);
      break;
    case STATE_HOT_MILD:
      eye_pair(it, gaze, BROW_SOFT, blink, OPEN_NORMAL, now_ms, 0, EYE_R, 3);
      draw_sweat(it, 2 + (int) ((now_ms / 2000) % 2));  // 2–3
      break;
    case STATE_HOT_WARM:
      eye_pair(it, gaze, BROW_SOFT, blink, OPEN_SOFT, now_ms, 0, EYE_R, 3);
      draw_sweat(it, 3 + (int) ((now_ms / 2500) % 2));  // 3–4
      break;
    case STATE_HOT_OVER:
      eye_x(it, EL, EY);
      eye_x(it, ER, EY);
      draw_sweat(it, 2 + (int) ((now_ms / 1800) % 2));  // 2–3
      break;
    case STATE_COLD_CHILLY:
      // Smaller serious eyes, inverse mouth (drawn in mouth)
      eye_pair(it, 0, BROW_FOCUS, blink, OPEN_SOFT, now_ms, 0, 8, 3);
      break;
    case STATE_COLD_SAD:
      eye_pair(it, 0, BROW_SAD, blink, OPEN_SOFT, now_ms, 1, 8, 3);
      break;
    case STATE_COLD_FREEZE:
      eye_x(it, EL, EY - 1, 7);
      eye_x(it, ER, EY - 1, 7);
      break;
    case STATE_IDLE:
    default:
      eye_pair(it, face_uses_living_gaze(face) ? gaze : 0, BROW_IDLE, blink, OPEN_NORMAL, now_ms, 0, EYE_R, 3);
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
    case STATE_HOT_MILD:
      return "HOT";
    case STATE_HOT_WARM:
      return "HOT2";
    case STATE_HOT_OVER:
      return "HOTX";
    case STATE_COLD_CHILLY:
      return "COLD";
    case STATE_COLD_SAD:
      return "COLD2";
    case STATE_COLD_FREEZE:
      return "COLDX";
    case STATE_HEART_MAPLE:
      return "HEART2";
    case STATE_THUMBUP_MAPPLE:
      return "THUMB";
    default:
      return "FACE";
  }
}

int buddy_select_face(int current_state, int happiness_level, int random_event_id, uint32_t random_event_end_ms,
                      uint32_t now_ms, int hour, bool has_temp, float temp_c) {
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
  // Temperature comfort overrides time-of-day mood when the room is harsh
  if (has_temp) {
    if (temp_c > 30.0f) {
      return STATE_HOT_OVER;
    }
    if (temp_c >= 28.0f) {
      return STATE_HOT_WARM;
    }
    if (temp_c > 25.0f) {
      return STATE_HOT_MILD;
    }
    if (temp_c < 17.0f) {
      return STATE_COLD_FREEZE;
    }
    if (temp_c <= 18.0f) {
      return STATE_COLD_SAD;
    }
    if (temp_c < 19.0f) {
      return STATE_COLD_CHILLY;
    }
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
  face_anim_sync(face, now_ms);
  const bool sleepy_mode = (face == STATE_SLEEPY_MAPLE) && (sleepy_phase(now_ms) == SLEEPY_DROWSY);
  blink_update(now_ms, sleepy_mode);
  if (face_uses_living_gaze(face) || face == STATE_SMILE_ACTIVE || face == STATE_HOT_MILD || face == STATE_HOT_WARM ||
      face == STATE_COLD_CHILLY) {
    gaze_update(now_ms);
  }
  if (face == STATE_BORED || face == STATE_BORED_MAPLE) {
    bored_wink_update(now_ms);
  }

  // During sleepy yawn/asleep phases, don't apply random blink on top
  bool blink = s_blink_closed;
  if (face == STATE_SLEEPY_MAPLE && sleepy_phase(now_ms) != SLEEPY_DROWSY) {
    blink = false;
  }

  draw_base_eyes(it, face, now_ms, blink, s_gaze, dizzy_severe, dizzy_started_ms);

  if (face == STATE_HEART_EYES || face == STATE_HEART_EYES_ANIM || face == STATE_HEART_MAPLE || face == STATE_HAPPY ||
      face == STATE_HAPPY_ANIM || face == STATE_THUMBUP_MAPPLE || face == STATE_SMILE_ACTIVE || face == STATE_SMILE ||
      face == STATE_LUNCH) {
    blush(it);
  }

  if (face == STATE_SLEEP || (face == STATE_SLEEPY_MAPLE && sleepy_phase(now_ms) == SLEEPY_ASLEEP)) {
    it.line(MX + 6, MY + 2, MX + 6, MY + 8, Color(1));
    it.filled_circle(MX + 6, MY + 9, 1, Color(1));
    zzz(it, now_ms);
  }

  draw_base_mouth(it, face, now_ms, mouth_talking);
}
