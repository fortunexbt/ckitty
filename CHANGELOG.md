# Changelog

All notable changes to ckitty are documented here. The project follows
[Semantic Versioning](https://semver.org/).

## Unreleased

- Add warm amber, moon, and forest palettes with 256-color and eight-color support, respecting `--ascii` and `NO_COLOR`.
- Refine the terminal scene with stable ambient twinkles and a compact interface that adapts to window size.
- Add direct pose shortcuts, pause/resume, palette cycling, and an in-app help overlay that temporarily pauses the scene.
- Add `--quiet` and an interface visibility toggle for a calmer terminal companion.
- Preserve deterministic, ANSI-free `--dump` output across themes.

## 1.0.0 — 2026-07-16

- Ship the canonical C11 and ncurses implementation.
- Add deterministic `--dump`, `--seed`, and `--frame` rendering for scripts and tests.
- Add animated poses, resize-safe layout, color detection, and `NO_COLOR` support.
- Add the no-sudo installer, CLI/core/resize tests, and sanitizer checks.
- Add reproducible demo generation and the animated README gallery.
