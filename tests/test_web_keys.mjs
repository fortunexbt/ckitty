// Regression check for the web build: keys must reach ckitty both before and
// after the terminal is clicked. A click moves focus to xterm's helper
// textarea, which used to swallow keydown before the page could see it.
//
//   node tests/test_web_keys.mjs        (needs the `playwright` package)

import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

const { chromium } = await import(process.env.PLAYWRIGHT_MODULE ?? "playwright");

const root = fileURLToPath(new URL("../web/public/", import.meta.url));
const types = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".wasm": "application/wasm", ".svg": "image/svg+xml", ".png": "image/png",
  ".woff2": "font/woff2"
};

const server = createServer(async (req, res) => {
  const path = new URL(req.url, "http://x").pathname;
  const file = normalize(join(root, path === "/" ? "index.html" : path));
  try {
    if (!file.startsWith(root)) throw new Error("outside root");
    res.writeHead(200, { "content-type": types[extname(file)] ?? "application/octet-stream" });
    res.end(await readFile(file));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const url = `http://127.0.0.1:${server.address().port}/`;

const browser = await chromium.launch({ args: ["--use-angle=metal"] });
let failures = 0;
const check = (ok, message) => {
  console.log(`${ok ? "ok" : "FAIL"} - ${message}`);
  if (!ok) failures++;
};

try {
  for (const [name, viewport] of [["desktop", { width: 1280, height: 800 }], ["phone", { width: 390, height: 844 }]]) {
    for (const clickFirst of [false, true]) {
      const label = `${name}, ${clickFirst ? "after clicking the terminal" : "without clicking"}`;
      const page = await browser.newPage({ viewport });
      const errors = [];
      page.on("console", (m) => m.type() === "error" && errors.push(m.text()));
      page.on("pageerror", (e) => errors.push(e.message));
      await page.goto(url);
      await page.waitForFunction(() => document.querySelector("#screen .xterm-rows")?.textContent.trim().length > 0);

      await page.evaluate(() => {
        const write = window.ckittyTerminalWrite;
        window.written = "";
        window.ckittyTerminalWrite = (bytes) => {
          window.written += typeof bytes === "string" ? bytes : new TextDecoder().decode(bytes);
          write(bytes);
        };
      });

      if (clickFirst) {
        await page.click("#screen");
        const focused = await page.evaluate(() => document.activeElement?.classList.contains("xterm-helper-textarea"));
        check(focused, `${label}: click focuses xterm's helper textarea`);
      }

      await page.keyboard.press("?");
      await page.waitForFunction(() => window.written.includes("open / close help"), null, { timeout: 5000 })
        .then(() => check(true, `${label}: '?' opens help`))
        .catch(() => check(false, `${label}: '?' opens help`));
      await page.keyboard.press("Escape");

      await page.keyboard.press("q");
      await page.waitForFunction(() => !document.getElementById("overlay").hidden, null, { timeout: 5000 })
        .then(() => check(true, `${label}: 'q' shows the Call it back overlay`))
        .catch(() => check(false, `${label}: 'q' shows the Call it back overlay`));

      await page.keyboard.press("q");
      await page.waitForFunction(() => document.getElementById("overlay").hidden, null, { timeout: 5000 })
        .then(() => check(true, `${label}: a key at the overlay restarts the cat`))
        .catch(() => check(false, `${label}: a key at the overlay restarts the cat`));

      check(errors.length === 0, `${label}: no console errors${errors.length ? ` (${errors[0]})` : ""}`);
      await page.close();
    }
  }
} finally {
  await browser.close();
  server.close();
}
process.exit(failures ? 1 : 0);
