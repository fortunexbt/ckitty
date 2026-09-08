#!/usr/bin/env python3
"""Exercise visible terminal state and controls through a real PTY.

The small screen reader handles ncurses' xterm output without dependencies.
Text assertions concern visible state; palette assertions inspect foreground
attributes because identical text can otherwise conceal monochrome output.
"""

from __future__ import annotations

import errno
import fcntl
import locale
import os
import pty
import re
import select
import signal
import struct
import sys
import termios
import time
import unicodedata


class Screen:
    def __init__(self, rows: int, cols: int) -> None:
        self.pending = b""
        self.resize(rows, cols)

    def resize(self, rows: int, cols: int) -> None:
        self.rows, self.cols = rows, cols
        self.cells = [[" "] * cols for _ in range(rows)]
        self.y = self.x = 0
        self.scroll_top, self.scroll_bottom = 0, rows - 1
        self.wrap = False
        self.last_printed = " "

    def put(self, character: str) -> None:
        if unicodedata.combining(character):
            col = self.x if self.wrap else self.x - 1
            while col >= 0 and not self.cells[self.y][col]:
                col -= 1
            if col >= 0:
                self.cells[self.y][col] += character
            return
        width = 2 if unicodedata.east_asian_width(character) in "WF" else 1
        if self.wrap:
            self.x, self.y = 0, min(self.rows - 1, self.y + 1)
        if self.x + width > self.cols:
            self.x, self.y = 0, min(self.rows - 1, self.y + 1)
        self.cells[self.y][self.x] = character
        if width == 2 and self.x + 1 < self.cols:
            self.cells[self.y][self.x + 1] = ""
        self.last_printed = character
        self.wrap = self.x + width >= self.cols
        self.x = min(self.cols - 1, self.x + width)

    def scroll(self, start: int, end: int, amount: int, down: bool) -> None:
        amount = min(amount, end - start)
        blank = [[" "] * self.cols for _ in range(amount)]
        self.cells[start:end] = (blank + self.cells[start:end - amount] if down else
                                 self.cells[start + amount:end] + blank)

    def feed(self, data: bytes) -> None:
        data = self.pending + data
        self.pending = b""
        i = 0
        while i < len(data):
            value = data[i]
            if value == 27:
                if i + 1 >= len(data):
                    break
                kind = data[i + 1]
                if kind == ord("["):
                    end = i + 2
                    while end < len(data) and not 0x40 <= data[end] <= 0x7E:
                        end += 1
                    if end == len(data):
                        break
                    self.csi(data[i + 2:end].decode("ascii"), chr(data[end]))
                    i = end + 1
                    continue
                if kind in (ord("]"), ord("P")):
                    # Terminal titles and other nonprinting control strings.
                    end = re.search(rb"\x07|\x1b\\", data[i + 2:])
                    if end is None:
                        break
                    i += 2 + end.end()
                    continue
                if kind in (ord("("), ord(")")):
                    if i + 2 >= len(data):
                        break
                    i += 3  # Character set selection; tested labels are ASCII.
                    continue
                if kind == ord("M"):  # Reverse index, possibly inside a scrolling region.
                    if self.y == self.scroll_top:
                        self.scroll(self.scroll_top, self.scroll_bottom + 1, 1, down=True)
                    else:
                        self.y = max(0, self.y - 1)
                    self.wrap = False
                i += 2
                continue
            if value == 13:
                self.x, self.wrap = 0, False
            elif value == 10:
                self.y = min(self.rows - 1, self.y + 1)
                self.wrap = False
            elif value == 8:
                self.x, self.wrap = max(0, self.x - 1), False
            elif value == 9:
                self.x = min(self.cols - 1, (self.x // 8 + 1) * 8)
            elif 32 <= value < 127:
                self.put(chr(value))
            elif value >= 128:
                length = 2 if value < 224 else 3 if value < 240 else 4
                if i + length > len(data):
                    break
                character = data[i:i + length].decode("utf-8", errors="strict")
                self.put(character)
                i += length
                continue
            i += 1
        self.pending = data[i:]

    def csi(self, arguments: str, command: str) -> None:
        if arguments.startswith(("?", ">", "!")):
            return  # Alternate screen, cursor visibility, etc.
        values = [int(value) if value.isdigit() else 0 for value in arguments.split(";")]
        amount = values[0] or 1
        if command in "Hf":
            self.y = min(self.rows - 1, amount - 1)
            self.x = min(self.cols - 1, (values[1] or 1) - 1) if len(values) > 1 else 0
        elif command == "A":
            self.y = max(0, self.y - amount)
        elif command in "Be":
            self.y = min(self.rows - 1, self.y + amount)
        elif command in "Ca":
            self.x = min(self.cols - 1, self.x + amount)
        elif command == "D":
            self.x = max(0, self.x - amount)
        elif command in "EF":
            self.x = 0
            self.y = max(0, min(self.rows - 1, self.y + (amount if command == "E" else -amount)))
        elif command in "G`":
            self.x = min(self.cols - 1, amount - 1)
        elif command == "d":
            self.y = min(self.rows - 1, amount - 1)
        elif command == "J":
            mode = values[0]
            for row in range(self.rows):
                for col in range(self.cols):
                    if mode == 2 or (mode == 0 and (row, col) >= (self.y, self.x)) or (
                        mode == 1 and (row, col) <= (self.y, self.x)
                    ):
                        self.cells[row][col] = " "
        elif command == "K":
            start = self.x if values[0] == 0 else 0
            end = self.x + 1 if values[0] == 1 else self.cols
            self.cells[self.y][start:end] = [" "] * (end - start)
        elif command == "X":
            end = min(self.cols, self.x + amount)
            self.cells[self.y][self.x:end] = [" "] * (end - self.x)
        elif command == "P":
            row = self.cells[self.y]
            row[self.x:] = (row[self.x + amount:] + [" "] * amount)[:self.cols - self.x]
        elif command == "@":
            row = self.cells[self.y]
            row[self.x:] = ([" "] * amount + row[self.x:])[:self.cols - self.x]
        elif command == "r":
            self.scroll_top = min(self.rows - 1, amount - 1)
            self.scroll_bottom = min(self.rows - 1, (values[1] or self.rows) - 1) if len(values) > 1 else self.rows - 1
            self.y = self.x = 0
        elif command in "LMST":
            start = self.y if command in "LM" else self.scroll_top
            end = self.scroll_bottom + 1
            if self.scroll_top <= start < end:
                self.scroll(start, end, amount, down=command in "LT")
        elif command == "b":
            for _ in range(amount):
                self.put(self.last_printed)
        if command not in "mb":
            self.wrap = False

    @property
    def text(self) -> str:
        return "\n".join("".join(row).rstrip() for row in self.cells)


def utf8_locale() -> str:
    previous = locale.setlocale(locale.LC_CTYPE)
    try:
        for candidate in ("C.UTF-8", "en_US.UTF-8", "UTF-8"):
            try:
                locale.setlocale(locale.LC_CTYPE, candidate)
                return candidate
            except locale.Error:
                pass
    finally:
        locale.setlocale(locale.LC_CTYPE, previous)
    raise AssertionError("PTY tests require a UTF-8 locale")


class Terminal:
    def __init__(self, binary: str, *args: str, ascii: bool = True,
                 no_color: bool = False, term: str = "xterm-256color",
                 rows: int = 30, cols: int = 120, locale_name: str | None = None) -> None:
        self.screen = Screen(rows, cols)
        self.status: int | None = None
        self.transcript = bytearray()
        locale_name = locale_name or utf8_locale()
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.environ["TERM"] = term
            os.environ["LC_ALL"] = locale_name
            if no_color:
                os.environ["NO_COLOR"] = "1"
            else:
                os.environ.pop("NO_COLOR", None)
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
            flags = ["--ascii"] if ascii else []
            os.execv(binary, [binary, *flags, "--seed", "123", *args])

    def __enter__(self) -> Terminal:
        return self

    def __exit__(self, *unused: object) -> None:
        # Every failed assertion must also reap its child, including slow loops.
        try:
            if self.alive():
                self.signal(signal.SIGTERM)
                self.drain(0.15)
            if self.alive():
                self.signal(signal.SIGKILL)
            if self.status is None:
                _, self.status = os.waitpid(self.pid, 0)
        finally:
            os.close(self.fd)

    def signal(self, number: int) -> None:
        try:
            os.kill(self.pid, number)
        except ProcessLookupError:
            pass  # Child may finish between waitpid(WNOHANG) and kill.

    def alive(self) -> bool:
        if self.status is None:
            waited, status = os.waitpid(self.pid, os.WNOHANG)
            if waited:
                self.status = status
        return self.status is None

    def drain(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.fd], [], [], min(0.025, max(0, deadline - time.monotonic())))
            if not ready:
                continue
            try:
                chunk = os.read(self.fd, 65536)
            except OSError as exc:
                if exc.errno == errno.EIO:
                    return
                raise
            if not chunk:
                return
            self.transcript.extend(chunk)
            self.screen.feed(chunk)

    def wait_for(self, predicate, description: str, timeout: float = 2.0) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.drain(0.025)
            if predicate(self.screen.text):
                return
            if not self.alive():
                break
        raise AssertionError(f"{description}\nVisible screen:\n{self.screen.text}\n"
                             f"Recent output: {bytes(self.transcript[-800:])!r}")

    def expect(self, text: str, timeout: float = 2.0) -> None:
        self.wait_for(lambda screen: text in screen, f"missing visible text: {text!r}", timeout)

    def key(self, key: bytes) -> None:
        assert self.alive(), f"process exited before key {key!r}: {self.status}"
        os.write(self.fd, key)

    def resize(self, rows: int, cols: int) -> None:
        self.drain(0.04)
        self.screen.resize(rows, cols)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))

    def unchanged(self, seconds: float, description: str) -> None:
        self.drain(0.08)  # Let the input-triggered redraw finish first.
        before = self.screen.text
        self.drain(seconds)
        assert self.alive(), f"process exited while {description}"
        assert self.screen.text == before, f"screen changed while {description}"

    def quit(self, timeout: float = 1.0) -> None:
        self.key(b"q")
        deadline = time.monotonic() + timeout
        while self.alive() and time.monotonic() < deadline:
            self.drain(0.025)
        assert not self.alive(), "q did not quit promptly"
        assert self.status is not None and os.WIFEXITED(self.status) and os.WEXITSTATUS(self.status) == 0, (
            f"terminal process failed: status={self.status}"
        )


def controls(binary: str) -> None:
    with Terminal(binary, "--delay", "15000", "walk") as terminal:
        terminal.expect("amber")
        terminal.key(b"p")
        terminal.expect("paused")
        terminal.unchanged(0.35, "paused")
        terminal.key(b"p")
        terminal.wait_for(lambda text: "paused" not in text, "pause did not release")
        moving = terminal.screen.text
        terminal.wait_for(lambda text: text != moving, "animation did not resume")

        for number, pose in ((1, "sit"), (2, "sleep"), (3, "play"), (4, "walk")):
            terminal.key(str(number).encode())
            terminal.expect(f"[{number} {pose}]")
        for theme in ("moon", "forest", "amber"):
            terminal.key(b"t")
            terminal.wait_for(lambda text: theme in text.splitlines()[0], f"theme did not change to {theme}")

        terminal.key(b"?")
        terminal.expect("make yourself at home")
        terminal.unchanged(0.35, "help is open")
        terminal.key(b"\x1b")
        terminal.wait_for(lambda text: "make yourself at home" not in text and "ckitty" in text,
                          "ESC did not close help while keeping kitty open")
        terminal.key(b"2")
        terminal.expect("[2 sleep]")
        terminal.key(b"h")
        terminal.wait_for(lambda text: "ckitty" not in text, "h did not hide chrome")
        terminal.key(b"h")
        terminal.expect("ckitty")
        terminal.key(b"?")
        terminal.expect("make yourself at home")
        terminal.quit()  # q must exit even while help has focus.


def quiet_and_slow_controls(binary: str) -> None:
    with Terminal(binary, "--quiet", "--theme", "forest", "--delay", "2147483647", "sit") as terminal:
        terminal.wait_for(lambda text: bool(text.strip()), "quiet mode did not render kitty")
        terminal.drain(0.1)
        assert "ckitty" not in terminal.screen.text, "--quiet showed chrome at startup"
        terminal.key(b"h")
        terminal.expect("forest", timeout=1.0)
        terminal.key(b"p")
        terminal.expect("paused", timeout=1.0)
        terminal.resize(7, 19)
        terminal.expect("make room for kitty", timeout=1.0)
        terminal.expect("resize to continue", timeout=1.0)
        terminal.resize(30, 120)
        terminal.expect("forest", timeout=1.0)
        terminal.key(b"?")
        terminal.expect("make yourself at home", timeout=1.0)
        terminal.quit()


def compact_help_and_sleep(binary: str) -> None:
    with Terminal(binary, "--delay", "2147483647", "sleep") as terminal:
        terminal.expect("ckitty")
        for rows, cols in ((8, 20), (12, 38), (17, 38), (17, 44)):
            terminal.resize(rows, cols)
            terminal.expect("( -.- )")
            lines = terminal.screen.text.splitlines()
            ears = next(line for line in lines if "/\\_/\\" in line)
            face = next(line for line in lines if "( -.- )" in line)
            assert ears.index("/") == face.index("(") + 1, "sleeping face shifted away from ears"
            assert face.index("z") == face.index("(") + 8, "sleep marker moved the face"
            terminal.key(b"?")
            if cols < 44:
                for line in ("space / 1-4 pose", "n new kitty", "p pause / resume",
                             "t palette", "h hide / show UI", "? / esc back", "q quit"):
                    terminal.expect(line)
            else:
                for line in ("choose a pose", "meet a new kitty", "pause / resume",
                             "change palette", "hide / show interface", "open / close help",
                             "back, then quit", "quit anytime", "take your time. kitty is waiting."):
                    terminal.expect(line)
            terminal.key(b"\x1b")
            terminal.expect("( -.- )")
        terminal.quit()


def resizes(binary: str, *args: str) -> None:
    with Terminal(binary, "--delay", "15000", *args, "sit") as terminal:
        terminal.expect("ckitty")
        for rows, cols in ((1, 1), (7, 19), (8, 20), (12, 34), (24, 80), (30, 120)):
            terminal.resize(rows, cols)
            terminal.drain(0.18)
            assert terminal.alive(), f"process failed at {cols}x{rows}: {terminal.status}"
            if (rows, cols) == (7, 19):
                terminal.expect("make room for kitty")
                terminal.expect("resize to continue")
            elif (rows, cols) == (30, 120):
                terminal.expect("ckitty")
                terminal.key(b"4")
                terminal.expect("[4 walk]")
        terminal.quit()


def foregrounds(data: bytes) -> set[str]:
    """Read the basic and indexed foreground attributes emitted by ncurses."""
    colors = set()
    for match in re.finditer(rb"\x1b\[([0-9;]*)m", data):
        values = [int(value or b"0") for value in match[1].split(b";")]
        i = 0
        while i < len(values):
            value = values[i]
            if value in (38, 48) and values[i + 1:i + 2] == [5]:
                if value == 38 and i + 2 < len(values):
                    colors.add(f"38;5;{values[i + 2]}")
                i += 3
            else:
                if 30 <= value <= 37 or 90 <= value <= 97:
                    colors.add(str(value))
                i += 1
    return colors


def palettes(binary: str) -> None:
    for term, amber, moon in (("xterm-256color", "38;5;222", "38;5;153"),
                              ("xterm", "33", "36")):
        with Terminal(binary, "--delay", "2147483647", "sit", ascii=False, term=term) as terminal:
            terminal.expect("amber")
            terminal.drain(0.05)
            assert amber in foregrounds(terminal.transcript), f"{term}: amber foreground missing"
            start = len(terminal.transcript)
            terminal.key(b"t")
            terminal.expect("moon")
            terminal.drain(0.05)
            changed = foregrounds(terminal.transcript[start:])
            assert moon in changed and amber not in changed, f"{term}: palette did not recolor: {changed}"
            terminal.quit()

    # Each independent opt-out must suppress color, including after a theme key.
    for ascii, no_color in ((True, False), (False, True)):
        with Terminal(binary, "--delay", "2147483647", "sit",
                      ascii=ascii, no_color=no_color) as terminal:
            terminal.expect("amber")
            terminal.key(b"t")
            terminal.expect("moon")
            terminal.drain(0.05)
            assert not foregrounds(terminal.transcript), (
                f"color escaped opt-out: --ascii={ascii}, NO_COLOR={no_color}"
            )
            terminal.quit()


def messages(binary: str) -> None:
    with Terminal(binary, "--message", "caf\u00e9", "--delay", "2147483647", "sit") as terminal:
        terminal.expect("[1 sit]")
        terminal.drain(0.05)
        assert "caf\u00e9".encode() in terminal.transcript, "message lost its printable UTF-8 bytes"
        terminal.quit()

    # Emoji, ideographs and combining marks must remain intact and consume
    # display columns, even when a wide character crosses the clipping edge.
    for message, visible in (("caf\u00e9 \U0001f431 \u6f22\u5b57 e\u0301", "caf\u00e9 \U0001f431 \u6f22\u5b57 e\u0301"),
                             ("x" * 15 + "\U0001f431tail", "x" * 15),
                             ("x" * 14 + "\u6f22tail", "x" * 14 + "\u6f22"),
                             ("x" * 15 + "e\u0301tail", "x" * 15 + "e\u0301")):
        with Terminal(binary, "--message", message, "--delay", "2147483647", "sit",
                      rows=8, cols=20) as terminal:
            terminal.expect("ckitty")
            terminal.expect(visible)
            terminal.drain(0.05)
            assert terminal.screen.text.splitlines()[1].strip() == visible, "message clipped by bytes or wrapped"
            assert not terminal.screen.text.splitlines()[2].strip(), "message wrapped into the cat's row"
            bytes(terminal.transcript).decode("utf-8", errors="strict")
            terminal.quit()

    for locale_name in ("C", "ckitty-invalid-locale"):
        with Terminal(binary, "--message", "kitty \U0001f431 \u6f22\u5b57 e\u0301", "--delay", "2147483647", "sit",
                      rows=8, cols=20, locale_name=locale_name) as terminal:
            terminal.expect("kitty ")
            terminal.drain(0.05)
            assert all(value < 128 for value in terminal.transcript), "fallback locale emitted invalid multibyte text"
            assert not terminal.screen.text.splitlines()[2].strip(), "fallback message wrapped"
            terminal.quit()

    message = "before\x1b[2Jmiddle\nnext\rtab\tback\bend\x07del\x7fafter\u0085c1\u009bend"
    sanitized = "before [2Jmiddle next tab back end del after c1 end"
    with Terminal(binary, "--message", message, "--delay", "2147483647", "sit") as terminal:
        terminal.expect(sanitized)
        terminal.drain(0.05)
        lines = terminal.screen.text.splitlines()
        assert sanitized in lines[1], "message controls moved text out of its row"
        assert "ckitty" in lines[0] and "amber" in lines[0], "message controls damaged the title"
        assert "[1 sit]" in lines[3], "message controls damaged the pose tabs"
        assert "sit / watching the world" in lines[-2], "message controls damaged the status"
        assert message.encode() not in terminal.transcript, "unsanitized message reached the terminal"
        terminal.quit()


def scene_marks(terminal: Terminal, quiet: bool = False) -> int:
    """Count revealed art, excluding chrome, ambient dots and the ground shadow."""
    rows = terminal.screen.cells[1:-1] if quiet else terminal.screen.cells[5:-4]
    return sum(ch not in " .+_" for row in rows for ch in row)


def completed_live_layout(binary: str) -> None:
    with Terminal(binary, "--live", "--grow-delay", "1000", "--delay", "2147483647", "sit") as terminal:
        terminal.expect("sit / watching the world")
        complete = scene_marks(terminal)
        assert complete > 20, "live reveal completed without a visible kitty"
        terminal.key(b"h")
        terminal.wait_for(lambda text: "ckitty" not in text, "h did not hide the interface")
        terminal.drain(0.05)
        assert scene_marks(terminal, quiet=True) >= complete - 2, "h erased a completed live kitty"
        terminal.key(b"h")
        terminal.expect("ckitty")
        terminal.drain(0.05)
        assert "sit / watching the world" in terminal.screen.text, "h restarted completed live growth"
        terminal.resize(40, 140)
        terminal.expect("ckitty")
        terminal.drain(0.05)
        assert "sit / watching the world" in terminal.screen.text, "resize restarted completed live growth"
        assert scene_marks(terminal) >= complete - 2, "resize erased a completed live kitty"
        terminal.quit()


def partial_live_layout(binary: str) -> None:
    with Terminal(binary, "--live", "--grow-delay", "20000", "sit") as terminal:
        terminal.expect("growing, then settling in")
        terminal.wait_for(lambda text: scene_marks(terminal) >= 12, "live reveal did not make progress")
        terminal.key(b"p")
        terminal.expect("paused")
        terminal.drain(0.05)
        progress = scene_marks(terminal)
        terminal.key(b"h")
        terminal.wait_for(lambda text: "ckitty" not in text, "h did not hide the interface")
        terminal.drain(0.05)
        assert scene_marks(terminal, quiet=True) == progress, "h lost partial live progress"
        terminal.resize(40, 140)
        terminal.wait_for(lambda text: scene_marks(terminal, quiet=True) > 0, "resize erased live progress")
        terminal.drain(0.05)
        assert scene_marks(terminal, quiet=True) == progress, "resize changed paused live progress"
        terminal.key(b"h")
        terminal.expect("paused")
        terminal.drain(0.05)
        assert scene_marks(terminal) == progress, "showing the interface changed paused live progress"
        terminal.key(b"2")
        terminal.expect("[2 sleep]")
        terminal.drain(0.05)
        assert scene_marks(terminal) == 0, "a new pose did not restart the live reveal"
        terminal.key(b"p")
        terminal.expect("growing, then settling in")
        terminal.wait_for(lambda text: scene_marks(terminal) >= 8, "new pose did not reveal")
        terminal.key(b"p")
        terminal.expect("paused")
        terminal.key(b"n")
        terminal.wait_for(lambda text: "seed 123" not in text, "n did not choose a new kitty")
        assert scene_marks(terminal) == 0, "a new kitty did not restart the live reveal"
        terminal.quit()


def compact_live_timing(binary: str) -> None:
    for rows, cols in ((12, 38), (30, 120)):
        with Terminal(binary, "--live", "--grow-delay", "2147483647", "--delay", "15000", "sit",
                      rows=rows, cols=cols) as terminal:
            terminal.expect("ckitty")
            if cols == 120:
                terminal.resize(12, 38)
            terminal.expect("( o.o )", timeout=0.8)
            terminal.quit()

    # The reverse layout change must switch back to growth timing, even if
    # the compact animation's ordinary frame deadline is half an hour away.
    with Terminal(binary, "--live", "--grow-delay", "1000", "--delay", "2147483647", "sit",
                  rows=12, cols=38) as terminal:
        terminal.expect("( -.- )")
        terminal.resize(30, 120)
        terminal.expect("sit / watching the world", timeout=1.5)
        assert scene_marks(terminal) > 20, "expanding the compact kitty stalled its reveal"
        terminal.quit()


def screensaver_live_restart(binary: str) -> None:
    with Terminal(binary, "--live", "--screensaver", "--grow-delay", "2147483647", "sit") as terminal:
        terminal.expect("seed 123")
        terminal.wait_for(lambda text: "seed 123" not in text, "screensaver did not choose a new kitty", timeout=8.8)
        # Include the lowest paw row while excluding the ground row and the
        # optional ambient punctuation.
        terminal.wait_for(lambda text: any(ch not in " .+" for row in terminal.screen.cells[5:-4] for ch in row),
                          "screensaver inherited the previous kitty's long reveal deadline", timeout=0.5)
        terminal.quit()


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} PATH_TO_CKITTY", file=sys.stderr)
        return 2
    binary = os.path.abspath(sys.argv[1])
    try:
        controls(binary)
        quiet_and_slow_controls(binary)
        compact_help_and_sleep(binary)
        resizes(binary)
        resizes(binary, "--live", "--grow-delay", "1000")
        palettes(binary)
        messages(binary)
        completed_live_layout(binary)
        partial_live_layout(binary)
        compact_live_timing(binary)
        screensaver_live_restart(binary)
    except (AssertionError, OSError, UnicodeError) as exc:
        print(f"terminal test failed: {exc}", file=sys.stderr)
        return 1
    print("TERMINAL OK (controls, palettes, Unicode messages, compact help, quiet, pause/help, slow timing, live progress, screensaver, resizes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
