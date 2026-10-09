#!/usr/bin/env python3
# *****************************************************************************
#  tools/vircon32/v32io-script.py — turn keyboard/mouse actions into an
#  input script for v32run-style tools (v32peek, v32shot, v32prof)
#
#  The headless tools drive a cartridge with gamepad presses and releases.
#  A v32io device IS a gamepad whose controls carry data, so v32io input
#  can be scripted too: this encodes keyboard and mouse actions with the
#  exact protocols the v32io adaptor and the modified DesktopEmulator use
#  (strobe / press / 7-bit key code for v32kbd; Gray code + trit movement
#  counters and buttons for v32mouse), one gamepad state per frame.
#
#  usage: v32io-script.py actions.txt > script.txt
#
#  actions (one per line, '#' comments; frames advance as actions run):
#    at <frame>                 jump forward to this frame
#    wait <frames>              let frames pass with nothing changing
#    connect <port> <0|1>       plug / unplug a gamepad (no frame used)
#    kbd <port> press <key>     one key event per frame
#    kbd <port> release <key>
#    kbd <port> tap <key>       press, then release
#    kbd <port> type <text>     rest of the line, typed with shift as
#                               needed (US layout); \n is Enter
#    mouse <port> move <dx> <dy>   in device steps; at most 5 per axis per
#                                  frame, so a long move spans frames
#    mouse <port> down <left|right|middle>
#    mouse <port> up   <left|right|middle>
#    mouse <port> click <left|right|middle>   down, wait 2 frames, up
#
#  Key names: a single character ('a', '1', '/', 'space' for ' '), or one
#  of up down left right capslock lshift rshift backspace tab lctrl rctrl
#  lalt enter f1..f12 ralt escape lgui rgui delete.
#
#  The protocols are described in docs/V32IO.md. This is a development
#  tool (tests/119sample.input was made with it); nothing in a build needs
#  Python.
# *****************************************************************************
import sys

CONTROLS = ["left", "right", "up", "down", "start", "a", "b", "x", "y", "l", "r"]

NAMED = {"up": 1, "down": 2, "left": 3, "right": 4, "capslock": 5,
         "lshift": 6, "rshift": 7, "backspace": 8, "tab": 9, "lctrl": 10,
         "rctrl": 11, "lalt": 12, "enter": 13, "ralt": 26, "escape": 27,
         "lgui": 28, "rgui": 29, "delete": 127, "space": 32}
for n in range(1, 13):
    NAMED["f%d" % n] = 13 + n

SHIFTED = {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6',
           '&': '7', '*': '8', '(': '9', ')': '0', '_': '-', '+': '=',
           '{': '[', '}': ']', '|': '\\', ':': ';', '"': "'", '<': ',',
           '>': '.', '?': '/', '~': '`'}


def key_code(name):
    if name in NAMED:
        return NAMED[name]
    if len(name) == 1 and 32 <= ord(name) < 127:
        return ord(name.lower()) if name.isalpha() else ord(name)
    raise SystemExit("v32io-script: unknown key '%s'" % name)


class Port:
    def __init__(self):
        self.state = [False] * 11
        self.strobe = 0          # keyboard: 0 none yet, 1 left, 2 right
        self.cx = 1              # mouse counters, at rest at position 1
        self.cy = 1
        self.buttons = set()


ports = {}
frame = 0
out = []
shown = {}                       # port -> controls as last written


def port(n):
    if n not in ports:
        ports[n] = Port()
        shown[n] = [False] * 11
    return ports[n]


def emit_frame():
    """write the changes for this frame, then move to the next one"""
    global frame
    for n in sorted(ports):
        want, have = ports[n].state, shown[n]
        # releases first, so opposite directions are never both pressed
        for i in range(11):
            if have[i] and not want[i]:
                out.append("%d release %d %s" % (frame, n, CONTROLS[i]))
        for i in range(11):
            if want[i] and not have[i]:
                out.append("%d press %d %s" % (frame, n, CONTROLS[i]))
        shown[n] = list(want)
    frame += 1


def key_event(n, code, pressed):
    p = port(n)
    p.strobe = 2 if p.strobe == 1 else 1
    s = [False] * 11
    s[0] = p.strobe == 1
    s[1] = p.strobe == 2
    s[2] = pressed
    s[3] = not pressed
    for bit in range(7):
        s[4 + bit] = bool(code & (1 << bit))
    p.state = s
    emit_frame()


def counter_controls(pos):
    group, t = divmod(pos, 3)
    trit = (2 - t) if group & 1 else t
    gray = [(False, False), (False, True), (True, True), (True, False)][group]
    return trit == 0, trit == 2, gray[0], gray[1]


def mouse_state(n):
    p = port(n)
    xn, xp, xh, xl = counter_controls(p.cx)
    yn, yp, yh, yl = counter_controls(p.cy)
    s = [False] * 11
    s[0], s[1], s[7], s[8] = xn, xp, xh, xl
    s[2], s[3], s[9], s[10] = yn, yp, yh, yl
    s[4] = "middle" in p.buttons
    s[5] = "left" in p.buttons
    s[6] = "right" in p.buttons
    p.state = s


def main():
    global frame
    lines = open(sys.argv[1]).read().splitlines() if len(sys.argv) > 1 else sys.stdin.read().splitlines()
    for raw in lines:
        line = raw.split("#", 1)[0].rstrip() if not raw.lstrip().startswith("kbd") else raw.rstrip()
        words = line.split()
        if not words:
            continue
        op = words[0]
        if op == "at":
            target = int(words[1])
            while frame < target:
                emit_frame()
        elif op == "wait":
            for _ in range(int(words[1])):
                emit_frame()
        elif op == "connect":
            port(int(words[1]))
            out.append("%d connect %s %s" % (frame, words[1], words[2]))
        elif op == "kbd":
            n, action = int(words[1]), words[2]
            if action == "type":
                # everything after "type " exactly, leading spaces included
                text = line.lstrip()[len(" ".join(words[:3])) + 1:]
                text = text.replace("\\n", "\n")
                for ch in text:
                    if ch == "\n":
                        key_event(n, 13, True); key_event(n, 13, False)
                        continue
                    base = SHIFTED.get(ch, ch)
                    shift = ch in SHIFTED or ch.isupper()
                    if shift:
                        key_event(n, 6, True)
                    code = key_code(base)
                    key_event(n, code, True); key_event(n, code, False)
                    if shift:
                        key_event(n, 6, False)
            else:
                code = key_code(words[3])
                if action in ("press", "tap"):
                    key_event(n, code, True)
                if action in ("release", "tap"):
                    key_event(n, code, False)
        elif op == "mouse":
            n, action = int(words[1]), words[2]
            p = port(n)
            if action == "move":
                dx, dy = int(words[3]), int(words[4])
                while dx or dy:
                    sx = max(-5, min(5, dx))
                    sy = max(-5, min(5, dy))
                    p.cx = (p.cx + sx) % 12
                    p.cy = (p.cy + sy) % 12
                    dx -= sx
                    dy -= sy
                    mouse_state(n)
                    emit_frame()
            elif action in ("down", "up", "click"):
                if action in ("down", "click"):
                    p.buttons.add(words[3]); mouse_state(n); emit_frame()
                if action == "click":
                    emit_frame(); emit_frame()
                if action in ("up", "click"):
                    p.buttons.discard(words[3]); mouse_state(n); emit_frame()
        else:
            raise SystemExit("v32io-script: unknown action '%s'" % op)
    out.append("%d end" % frame)
    print("# generated by tools/vircon32/v32io-script.py -- do not edit")
    print("\n".join(out))


main()
