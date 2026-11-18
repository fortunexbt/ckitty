/* Drives the WebAssembly build of ckitty: an xterm.js screen for its ANSI
 * output, a key queue for its getch(), and the page controls that feed it. */
(() => {
  "use strict";

  /* The cat is about 32 columns by 8 rows, so the font grows until the grid
   * is close to COLS x ROWS and the cat fills the window. Narrower than COLS,
   * ckitty drops its pose tabs and seed. Shorter than ROWS, it switches to its
   * compact "make room for kitty" drawing. The font never goes below either. */
  const COLS = 54;
  const ROWS = 19;
  const MIN_FONT = 8;
  const MAX_FONT = 40;
  const KEYS = new Set(["n", "p", "t", "h", "q", "?", " ", "1", "2", "3", "4"]);
  const PALETTES = { amber: "#16110d", moon: "#10121a", forest: "#0f140f" };

  const screen = document.getElementById("screen");
  const gridLabel = document.getElementById("grid-size");
  const commandLabel = document.getElementById("command");
  const overlay = document.getElementById("overlay");
  const restartButton = document.getElementById("restart");
  const paletteName = document.getElementById("palette-name");
  const pauseKey = document.getElementById("pause-key");
  const poseKeys = document.querySelectorAll(".key[data-pose]");
  const recipe = document.getElementById("recipe");
  const recipeCommand = document.getElementById("recipe-command");
  const themeColor = document.querySelector('meta[name="theme-color"]');
  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* ?seed=123 starts on a particular cat, the same one `ckitty --seed 123`
   * draws natively. Anything that is not a 32-bit unsigned number is ignored. */
  const seedParam = new URLSearchParams(location.search).get("seed");
  const startSeed = /^\d{1,10}$/.test(seedParam ?? "") && Number(seedParam) <= 0xffffffff ? seedParam : null;

  const term = new Terminal({
    disableStdin: true,
    cursorBlink: false,
    scrollback: 0,
    allowTransparency: true,
    fontFamily: 'ui-monospace, SFMono-Regular, Menlo, "DejaVu Sans Mono", monospace',
    fontSize: 15,
    lineHeight: 1.15,
    theme: {
      background: "rgba(0, 0, 0, 0)",
      foreground: "#efe4d3",
      selectionBackground: "rgba(255, 215, 135, 0.28)"
    }
  });
  const fit = new FitAddon.FitAddon();
  term.loadAddon(fit);
  term.open(screen);

  let app = null;
  let running = false;

  window.ckittyTerminalWrite = (bytes) => term.write(bytes);
  window.ckittySessionEnded = () => {
    running = false;
    app = null;
    overlay.hidden = false;
  };

  /* fit.proposeDimensions() reports the grid xterm would lay out at the
   * current font size, so try sizes from a first estimate downwards. */
  function gridAt(size) {
    term.options.fontSize = size;
    return fit.proposeDimensions();
  }

  function layout() {
    const probe = gridAt(20);
    if (!probe) return;
    /* Half-pixel steps: a whole pixel is a big jump at phone sizes. */
    let size = Math.floor(2 * 20 * Math.min(probe.cols / COLS, probe.rows / ROWS)) / 2;
    size = Math.max(MIN_FONT, Math.min(MAX_FONT, size));
    let grid = gridAt(size);
    while (size > MIN_FONT && (grid.cols < COLS || grid.rows < ROWS)) {
      size -= 0.5;
      grid = gridAt(size);
    }
    term.resize(grid.cols, grid.rows);
    gridLabel.textContent = `${term.cols}×${term.rows}`;
    if (app) app._ckitty_web_resize(term.cols, term.rows);
  }

  /* The page reads ckitty's own screen to light its keys: the palette name
   * in the top-right corner, and the pose, pause state and seed on the
   * status line. When the interface is hidden the last known state stays. */
  const state = { palette: "amber", pose: null, paused: false, seed: null };

  function lineText(y) {
    return term.buffer.active.getLine(y)?.translateToString(true) ?? "";
  }

  function readState() {
    const palette = lineText(0).match(/(amber|moon|forest)$/)?.[1];
    const status = lineText(term.rows - 2).match(/^\s*(sit|sleep|play|walk) \/ (.*?)(?:\s{2,}seed (\d+))?$/);
    const next = { ...state };
    if (palette) next.palette = palette;
    if (status) {
      next.pose = status[1];
      next.paused = status[2] === "paused";
      if (status[3]) next.seed = status[3];
    }
    if (Object.keys(next).every((key) => next[key] === state[key])) return;
    Object.assign(state, next);
    render();
  }

  function render() {
    document.documentElement.dataset.palette = state.palette;
    themeColor.content = PALETTES[state.palette];
    paletteName.textContent = state.palette;
    for (const key of poseKeys) {
      key.setAttribute("aria-pressed", String(key.dataset.pose === state.pose));
    }
    pauseKey.setAttribute("aria-pressed", String(state.paused));
    if (state.seed && state.pose) {
      const theme = state.palette === "amber" ? "" : ` --theme ${state.palette}`;
      recipeCommand.textContent = `ckitty --seed ${state.seed}${theme} ${state.pose}`;
      recipe.hidden = false;
    }
  }

  term.onWriteParsed(readState);

  /* A key press from any source pushes the matching keycap down briefly. */
  const keycaps = new Map([...document.querySelectorAll(".key[data-key]")].map((key) => [key.dataset.key, key]));
  function press(key) {
    const cap = keycaps.get(key);
    if (!cap || reduceMotion) return;
    cap.classList.add("is-down");
    setTimeout(() => cap.classList.remove("is-down"), 120);
  }

  function sendKey(key) {
    if (!running) {
      if (!overlay.hidden) start();
      return;
    }
    app._ckitty_web_key(key.codePointAt(0));
  }

  async function start() {
    overlay.hidden = true;
    term.reset();
    running = false;
    try {
      app = await createCkitty();
    } catch (error) {
      overlay.hidden = false;
      overlay.firstElementChild.textContent = "The cat could not be loaded.";
      console.error(error);
      return;
    }
    running = true;
    app._ckitty_web_resize(term.cols, term.rows);
    /* --live draws the cat in stroke by stroke, quicker than the terminal
     * default so a visitor meets it within a couple of seconds. With reduced
     * motion the cat arrives whole, still breathing but without the reveal. */
    const args = reduceMotion ? [] : ["--live", "--grow-delay", "22000"];
    if (startSeed) args.push("--seed", startSeed);
    commandLabel.textContent = ["ckitty", ...args].join(" ");
    app.callMain(args);
  }

  let pendingLayout = 0;
  new ResizeObserver(() => {
    cancelAnimationFrame(pendingLayout);
    pendingLayout = requestAnimationFrame(layout);
  }).observe(screen);

  function handleKeydown(event) {
    if (event.metaKey || event.ctrlKey || event.altKey) return;
    const onControl = document.activeElement instanceof HTMLButtonElement ||
      document.activeElement instanceof HTMLAnchorElement;
    if (onControl && (event.key === " " || event.key === "Enter")) return;
    const key = event.key === "Escape" ? "\u001b" : event.key;
    if (!KEYS.has(key) && key !== "\u001b") return;
    event.preventDefault();
    press(key);
    sendKey(key);
  }

  /* Clicking the terminal focuses xterm's helper textarea, which swallows
   * keydown before this listener hears it, so keys typed there go through
   * xterm's own hook below and are skipped here to avoid handling them twice. */
  document.addEventListener("keydown", (event) => {
    if (!term.element.contains(event.target)) handleKeydown(event);
  });

  /* Returning false keeps xterm from processing the key itself. */
  term.attachCustomKeyEventHandler((event) => {
    if (event.type === "keydown") handleKeydown(event);
    return false;
  });

  for (const key of keycaps.values()) {
    key.addEventListener("click", () => sendKey(key.dataset.key));
  }

  restartButton.addEventListener("click", start);

  for (const button of document.querySelectorAll(".copy")) {
    const label = button.querySelector("span");
    const code = button.parentElement.querySelector("code");
    let reset = 0;
    const flash = (text) => {
      label.textContent = text;
      button.dataset.done = "true";
      clearTimeout(reset);
      reset = setTimeout(() => {
        label.textContent = "Copy";
        delete button.dataset.done;
      }, 1600);
    };
    button.addEventListener("click", async () => {
      try {
        await navigator.clipboard.writeText(button.dataset.copy ?? code.textContent);
        flash("Copied");
      } catch {
        /* A browser can refuse clipboard access. Select the command instead,
         * so the keyboard shortcut still finishes the job. */
        const range = document.createRange();
        range.selectNodeContents(code);
        const selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
        flash("Selected");
      }
    });
  }

  layout();
  start();
})();
