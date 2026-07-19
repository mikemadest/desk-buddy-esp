#!/usr/bin/env python3
"""Render static 128x64 1-bit frames matching Desk Buddy procedural faces."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

W, H = 128, 64
EL, ER, EY = 40, 88, 24
MX, MY = 64, 50
EYE_R = 10
COS16 = [16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6, 0, 6, 11, 15]
SIN16 = [0, 6, 11, 15, 16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6]

OUT = Path(__file__).resolve().parents[1] / "assets" / "face-design"


class Canvas:
    def __init__(self) -> None:
        self.img = Image.new("1", (W, H), 0)
        self.d = ImageDraw.Draw(self.img)

    def px(self, x: int, y: int, v: int = 1) -> None:
        if 0 <= x < W and 0 <= y < H:
            self.img.putpixel((x, y), v)

    def line(self, x0: int, y0: int, x1: int, y1: int, v: int = 1) -> None:
        self.d.line((x0, y0, x1, y1), fill=v)

    def circle(self, cx: int, cy: int, r: int, v: int = 1) -> None:
        self.d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=v)

    def fill_circle(self, cx: int, cy: int, r: int, v: int = 1) -> None:
        self.d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=v)

    def fill_rect(self, x: int, y: int, w: int, h: int, v: int = 1) -> None:
        self.d.rectangle((x, y, x + w - 1, y + h - 1), fill=v)

    def save(self, name: str) -> None:
        OUT.mkdir(parents=True, exist_ok=True)
        path = OUT / name
        self.img.save(path)
        print(path)


def eye_closed(c: Canvas, cx: int, cy: int, w: int = 12) -> None:
    c.fill_rect(cx - w, cy - 2, w * 2 + 1, 4)


def brow(c: Canvas, cx: int, cy: int, nose_right: bool, tip: int, arch: int, dir_: int, y_base_off: int = 0) -> None:
    y_base = cy - EYE_R + y_base_off
    x_outer = (cx - EYE_R + 1) if nose_right else (cx + EYE_R - 1)
    x_nose = (cx + EYE_R + 1) if nose_right else (cx - EYE_R - 1)
    x_mid = (x_outer + x_nose) // 2
    if dir_ > 0:
        y_outer = y_base + (tip + 1) // 2
        y_nose = y_base - tip
    elif dir_ < 0:
        y_outer = y_base - tip
        y_nose = y_base + (tip + 1) // 2
    else:
        y_outer = y_nose = y_base
    y_mid = (y_outer + y_nose) // 2 - arch
    for t in range(2):
        c.line(x_outer, y_outer + t, x_mid, y_mid + t)
        c.line(x_mid, y_mid + t, x_nose, y_nose + t)


def eye_globe(
    c: Canvas,
    cx: int,
    cy: int,
    gaze: int,
    open_cut: int,
    iris_r: int,
    tip: int,
    arch: int,
    dir_: int,
    nose_right: bool,
    round_eye: bool = False,
) -> None:
    c.fill_circle(cx, cy, EYE_R)
    if not round_eye:
        for dx in range(-EYE_R, EYE_R + 1):
            cut = open_cut + (abs(dx) * 2) // EYE_R
            for dy in range(cut):
                c.px(cx + dx, cy - EYE_R - 1 + dy, 0)
        for dx in range(-EYE_R + 1, EYE_R):
            for dy in range(2):
                c.px(cx + dx, cy + EYE_R - dy, 0)
    c.fill_circle(cx + gaze * 3, cy + (0 if round_eye else 1), iris_r, 0)
    brow(c, cx, cy, nose_right, tip, arch, dir_)


def eye_pair(c: Canvas, gaze: int, open_cut: int, iris_r: int, tip: int, arch: int, dir_: int, round_eye: bool = False) -> None:
    eye_globe(c, EL, EY, gaze, open_cut, iris_r, tip, arch, dir_, True, round_eye)
    eye_globe(c, ER, EY, gaze, open_cut, iris_r, tip, arch, dir_, False, round_eye)


def mouth_line(c: Canvas, w: int = 9) -> None:
    c.fill_rect(MX - w, MY - 1, w * 2 + 1, 3)


def mouth_smile(c: Canvas, w: int = 18, h: int = 7) -> None:
    for t in range(3):
        prev_x, prev_y = MX - w, MY - 1 + t
        for i in range(1, 15):
            x = MX - w + (2 * w * i) // 14
            xn = x - MX
            y = MY - 1 + t + (h * (w * w - xn * xn)) // (w * w)
            c.line(prev_x, prev_y, x, y)
            prev_x, prev_y = x, y


def mouth_frown(c: Canvas, w: int = 12) -> None:
    for t in range(2):
        c.line(MX - w, MY + 4 + t, MX - w // 2, MY - 1 + t)
        c.line(MX - w // 2, MY - 1 + t, MX + w // 2, MY - 1 + t)
        c.line(MX + w // 2, MY - 1 + t, MX + w, MY + 4 + t)


def mouth_o(c: Canvas, r: int = 7) -> None:
    c.circle(MX, MY, r)
    c.circle(MX, MY, r - 1)


def mouth_dizzy(c: Canvas) -> None:
    for t in range(2):
        c.line(MX - 12, MY + t, MX - 4, MY + 4 + t)
        c.line(MX - 4, MY + 4 + t, MX + 4, MY - 4 + t)
        c.line(MX + 4, MY - 4 + t, MX + 12, MY + t)


def eye_x(c: Canvas, cx: int, cy: int, s: int = 8) -> None:
    c.line(cx - s, cy - s, cx + s, cy + s)
    c.line(cx - s + 1, cy - s, cx + s + 1, cy + s)
    c.line(cx - s, cy + s, cx + s, cy - s)
    c.line(cx - s + 1, cy + s, cx + s + 1, cy - s)


def eye_spiral(c: Canvas, cx: int, cy: int, phase: int = 0) -> None:
    rmax = 12
    prev_x, prev_y = cx, cy
    for i in range(40):
        a = (phase + i) % 16
        r = 2 + (i * (rmax - 2)) // 39
        x = cx + (COS16[a] * r) // 16
        y = cy + (SIN16[a] * r) // 16
        for t in range(-1, 2):
            c.line(prev_x, prev_y + t, x, y + t)
        prev_x, prev_y = x, y


def eye_sleepy(c: Canvas, cx: int, cy: int) -> None:
    c.line(cx - 10, cy, cx - 4, cy + 4)
    c.line(cx - 4, cy + 4, cx + 4, cy + 4)
    c.line(cx + 4, cy + 4, cx + 10, cy)
    c.line(cx - 10, cy + 1, cx - 4, cy + 5)
    c.line(cx + 4, cy + 5, cx + 10, cy + 1)


def sweat(c: Canvas, n: int) -> None:
    drops = [(EL - 14, EY + 2), (ER + 12, EY + 4), (EL - 10, EY + 14), (ER + 8, EY + 16)]
    for i in range(min(n, 4)):
        x, y = drops[i]
        c.px(x, y)
        c.line(x, y + 1, x, y + 4)
        c.px(x - 1, y + 3)
        c.px(x + 1, y + 3)
        c.fill_circle(x, y + 5, 1)


def zzz(c: Canvas) -> None:
    x, y = ER + 4, 9
    c.line(x, y, x + 5, y)
    c.line(x + 5, y, x, y + 5)
    c.line(x, y + 5, x + 5, y + 5)


def glasses(c: Canvas) -> None:
    hw, hh, rad = 14, 12, 4
    for cx in (EL, ER):
        x0, x1, y0, y1 = cx - hw, cx + hw, EY - hh, EY + hh
        c.line(x0 + rad, y0, x1 - rad, y0)
        c.line(x0 + rad, y1, x1 - rad, y1)
        c.line(x0, y0 + rad, x0, y1 - rad)
        c.line(x1, y0 + rad, x1, y1 - rad)
    c.line(EL + hw, EY, ER - hw, EY)


def happy_curve(c: Canvas, cx: int, cy: int) -> None:
    for t in range(3):
        c.line(cx - 10, cy + 3 + t, cx - 5, cy - 1 + t)
        c.line(cx - 5, cy - 1 + t, cx, cy - 3 + t)
        c.line(cx, cy - 3 + t, cx + 5, cy - 1 + t)
        c.line(cx + 5, cy - 1 + t, cx + 10, cy + 3 + t)


def render_all() -> None:
    frames: list[tuple[str, callable]] = []

    def add(name: str, fn) -> None:
        frames.append((name, fn))

    add("01_idle.png", lambda c: (eye_pair(c, 0, 2, 3, 1, 2, 1), mouth_line(c)))
    add("02_smile.png", lambda c: (eye_pair(c, 0, 2, 4, 1, 2, 1), mouth_smile(c)))
    add(
        "03_wink_open.png",
        lambda c: (eye_pair(c, 0, 2, 4, 1, 2, 1), mouth_smile(c, 16, 6)),
    )

    def wink_closed(c: Canvas) -> None:
        eye_globe(c, EL, EY, 0, 2, 4, 1, 2, 1, True)
        eye_closed(c, ER, EY)
        brow(c, ER, EY, False, 1, 2, 1)
        mouth_smile(c, 16, 6)

    add("03b_wink_closed.png", wink_closed)
    add(
        "04_happy.png",
        lambda c: (happy_curve(c, EL, EY), happy_curve(c, ER, EY), mouth_smile(c, 16, 8)),
    )
    add(
        "05_surprised.png",
        lambda c: (eye_pair(c, 0, 0, 2, 0, 2, 0, True), mouth_o(c, 8)),
    )
    add(
        "05b_surprised_mouth_line.png",
        lambda c: (eye_pair(c, 0, 0, 2, 0, 2, 0, True), mouth_line(c, 8)),
    )
    add(
        "06_dizzy.png",
        lambda c: (eye_spiral(c, EL, EY, 0), eye_spiral(c, ER, EY, 4), mouth_dizzy(c)),
    )

    def work(c: Canvas) -> None:
        eye_pair(c, 0, 2, 4, 1, 2, 1)
        glasses(c)
        mouth_line(c)

    add("07_work.png", work)
    add("08_focus.png", lambda c: (eye_pair(c, 0, 2, 4, 2, 1, -1), mouth_line(c)))
    add("09_bored.png", lambda c: (eye_pair(c, 0, 4, 3, 1, 1, 1, False), mouth_line(c)))
    add("10_sad.png", lambda c: (eye_pair(c, 0, 4, 3, 3, 0, 1), mouth_frown(c, 12)))
    add("11_sleep.png", lambda c: (eye_sleepy(c, EL, EY), eye_sleepy(c, ER, EY), mouth_line(c, 7), zzz(c)))
    add(
        "12_sleepy_drowsy.png",
        lambda c: (eye_pair(c, 0, 4, 3, 1, 1, 1), mouth_line(c)),
    )
    add(
        "12b_sleepy_yawn.png",
        lambda c: (eye_closed(c, EL, EY), eye_closed(c, ER, EY), mouth_o(c, 10)),
    )
    add(
        "12c_sleepy_asleep.png",
        lambda c: (eye_sleepy(c, EL, EY), eye_sleepy(c, ER, EY), mouth_line(c, 7), zzz(c)),
    )
    add("13_hot_mild.png", lambda c: (eye_pair(c, 0, 2, 3, 1, 1, 1), mouth_line(c), sweat(c, 2)))
    add("13b_hot_warm.png", lambda c: (eye_pair(c, 0, 4, 3, 1, 1, 1), mouth_line(c), sweat(c, 4)))
    add("13c_hot_over.png", lambda c: (eye_x(c, EL, EY), eye_x(c, ER, EY), mouth_line(c), sweat(c, 3)))
    add("14_cold_chilly.png", lambda c: (eye_pair(c, 0, 4, 3, 2, 1, -1), mouth_frown(c, 11)))
    add("14b_cold_sad.png", lambda c: (eye_pair(c, 0, 4, 3, 3, 0, 1), mouth_frown(c, 14)))
    add("14c_cold_freeze.png", lambda c: (eye_x(c, EL, EY - 1, 7), eye_x(c, ER, EY - 1, 7), mouth_frown(c, 14)))
    add("15_yawn.png", lambda c: (eye_closed(c, EL, EY), eye_closed(c, ER, EY), mouth_o(c, 11)))

    for name, fn in frames:
        c = Canvas()
        fn(c)
        c.save(name)


if __name__ == "__main__":
    render_all()
    print(f"Wrote frames to {OUT}")
