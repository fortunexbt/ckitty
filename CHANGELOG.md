# Changelog

All notable changes to ckitty are documented here. The project follows
[Semantic Versioning](https://semver.org/).

## 1.1.0 — 2026-09-08

- Redraw all four poses with broad feline faces, rounded haunches, paired paws, and short curved tails; lower the walking and playing silhouettes and curl up the sleeping kitty.
- Add warm amber, moon, and forest palettes with 256-color and eight-color support, respecting `--ascii` and `NO_COLOR`.
- Refine the terminal scene with stable ambient twinkles and a compact interface that adapts to window size.
- Add direct pose shortcuts, pause/resume, palette cycling, and an in-app help overlay that temporarily pauses the scene.
- Add `--quiet` and an interface visibility toggle for a calmer terminal companion.
- Preserve deterministic, ANSI-free `--dump` output across themes.
- Keep tails connected, whiskers clear of the body, toys distinct, and birds visible between animation frames.
- Fit help into compact panes and keep compact animation and screensaver replacements responsive at slow reveal speeds.
- Render Unicode messages within their display-column limit, with a safe fallback outside UTF-8 locales.

## 1.0.0 — 2026-07-16

- Ship the canonical C11 and ncurses implementation.
- Add deterministic `--dump`, `--seed`, and `--frame` rendering for scripts and tests.
- Add animated poses, resize-safe layout, color detection, and `NO_COLOR` support.
- Add the no-sudo installer, CLI/core/resize tests, and sanitizer checks.
- Add reproducible demo generation and the animated README gallery.
