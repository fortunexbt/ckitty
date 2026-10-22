/* Drives the WebAssembly build of ckitty: an xterm.js screen for its ANSI
 * output, a key queue for its getch(), and the page controls that feed it. */
(() => {
  "use strict";

  /* Below this grid the C app switches to its compact "make room for kitty"
   * layout, so the font shrinks until the full cat fits. */
  const MIN_COLS = 46;
  const MIN_ROWS = 20;
  const FONT_SIZES = [20, 18, 16, 15, 14, 13, 12, 11, 10, 9];
  const KEYS = new Set(["n", "p", "t", "h", "q", "?", " ", "1", "2", "3", "4"]);

  const screen = document.getElementById("screen");
  const gridLabel = document.getElementById("grid-size");
  const overlay = document.getElementById("overlay");
  const restartButton = document.getElementById("restart");
  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  const term = new Terminal({
    disableStdin: true,
    cursorBlink: false,
    scrollback: 0,
    allowTransparency: true,
    fontFamily: 'ui-monospace, SFMono-Regular, Menlo, "DejaVu Sans Mono", monospace',
    fontSize: 15,
    lineHeight: 1.15,
    theme: {
      background: "#151110",
      foreground: "#ece2d2",
      selectionBackground: "rgba(242, 184, 92, 0.26)"
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

  /* Try font sizes until the whole cat fits, measuring a real cell at each
   * step: xterm reports cell metrics only once it has laid the grid out. */
  function layout() {
    const style = getComputedStyle(screen);
    const width = screen.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
    const height = screen.clientHeight - parseFloat(style.paddingTop) - parseFloat(style.paddingBottom);
    if (width < 1 || height < 1) return;

    let chosen = null;
    for (const size of FONT_SIZES) {
      term.options.fontSize = size;
      fit.fit();
      const cell = {
        width: term.element.offsetWidth / term.cols,
        height: term.element.offsetHeight / term.rows
      };
      if (!cell.width || !cell.height) return;
      chosen = {
        size,
        cols: Math.max(8, Math.floor(width / cell.width)),
        rows: Math.max(4, Math.floor(height / cell.height))
      };
      if (chosen.cols >= MIN_COLS && chosen.rows >= MIN_ROWS) break;
    }

    term.options.fontSize = chosen.size;
    term.resize(chosen.cols, chosen.rows);
    gridLabel.textContent = `${term.cols}×${term.rows}`;
    if (app) app._ckitty_web_resize(term.cols, term.rows);
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
    /* --live draws the cat in stroke by stroke. With reduced motion the cat
     * arrives whole, still breathing but without the reveal. */
    app.callMain(reduceMotion ? [] : ["--live"]);
  }

  let pendingLayout = 0;
  new ResizeObserver(() => {
    cancelAnimationFrame(pendingLayout);
    pendingLayout = requestAnimationFrame(layout);
  }).observe(screen);

  document.addEventListener("keydown", (event) => {
    if (event.metaKey || event.ctrlKey || event.altKey) return;
    const onControl = document.activeElement instanceof HTMLButtonElement ||
      document.activeElement instanceof HTMLAnchorElement;
    if (onControl && (event.key === " " || event.key === "Enter")) return;
    const key = event.key === "Escape" ? "\u001b" : event.key;
    if (!KEYS.has(key) && key !== "\u001b") return;
    event.preventDefault();
    sendKey(key);
  });

  for (const chip of document.querySelectorAll(".chip[data-key]")) {
    chip.addEventListener("click", () => sendKey(chip.dataset.key));
  }

  restartButton.addEventListener("click", start);

  for (const button of document.querySelectorAll(".copy")) {
    const label = button.querySelector("span");
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
        await navigator.clipboard.writeText(button.dataset.copy);
        flash("Copied");
      } catch {
        /* A browser can refuse clipboard access. Select the command instead,
         * so the keyboard shortcut still finishes the job. */
        const range = document.createRange();
        range.selectNodeContents(button.parentElement.querySelector("code"));
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
