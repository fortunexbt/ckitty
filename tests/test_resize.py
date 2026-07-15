#!/usr/bin/env python3
"""Exercise the ncurses frontend through real pseudo-terminal resizes."""

from __future__ import annotations

import errno
import fcntl
import os
import pty
import select
import struct
import sys
import termios
import time


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} PATH_TO_CKITT", file=sys.stderr)
        return 2

    binary = os.path.abspath(sys.argv[1])
    pid, fd = pty.fork()
    if pid == 0:
        os.environ.update(TERM="xterm-256color", NO_COLOR="1")
        os.execv(binary, [binary, "--ascii", "sit"])

    output = bytearray()

    def resize(rows: int, cols: int) -> None:
        fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))

    def drain(seconds: float) -> None:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            ready, _, _ = select.select([fd], [], [], 0.03)
            if not ready:
                continue
            try:
                output.extend(os.read(fd, 65536))
            except OSError as exc:
                if exc.errno == errno.EIO:
                    return
                raise

    status = 1
    try:
        for rows, cols in ((24, 80), (1, 1), (8, 20), (30, 120), (12, 34), (24, 80)):
            resize(rows, cols)
            drain(0.22)

        os.write(fd, b"q")
        drain(0.45)
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            waited, status = os.waitpid(pid, os.WNOHANG)
            if waited:
                break
            drain(0.05)
        else:
            os.kill(pid, 9)
            _, status = os.waitpid(pid, 0)
    finally:
        try:
            os.close(fd)
        except OSError:
            pass

    if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
        print(f"resize process failed: status={status}", file=sys.stderr)
        return 1

    text = output.decode("utf-8", "replace")
    if "make room for kitty" not in text or "resize to continue" not in text:
        print("compact resize screen was not rendered", file=sys.stderr)
        return 1

    print("RESIZE OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
