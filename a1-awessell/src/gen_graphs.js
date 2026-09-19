#!/usr/bin/env node
/*
 * Generate an Erdos-Renyi and an RMAT graph as Matrix Market (.mtx) files.
 *
 *   node scripts/gen_graphs.js --outdir graphs               # 2^20 vertices, avg degree 16
 *   node scripts/gen_graphs.js --scale 14 --outdir /tmp/g    # small test
 *
 * Options: --scale (n = 2^scale, default 20), --avg-degree (default 16),
 *          --seed (default 1), --outdir (default "graphs")
 *
 * Both graphs are undirected, simple (no self-loops, no duplicate edges) and are
 * written as "coordinate pattern symmetric", 1-indexed, one line per undirected
 * edge. Number of undirected edges = n * avgDegree / 2, so the average degree is
 * exactly avgDegree. RMAT edges use probabilities (a,b,c,d) = (0.57,0.19,0.19,0.05);
 * duplicates are removed and more edges are drawn until the target is met, then
 * vertex labels are randomly permuted (as Graph500 does).
 *
 * An edge {lo,hi} (lo < hi) is stored as the number lo*n+hi. With n <= 2^26 this
 * stays below 2^52, so it is exact in a double and can live in a Float64Array.
 */
"use strict";
const fs = require("fs");
const path = require("path");

// ---------- seeded PRNG (sfc32, seeded through splitmix32) ----------
function makeRng(seed) {
  let s = seed >>> 0;
  const splitmix = () => {
    s = (s + 0x9e3779b9) >>> 0;
    let z = s;
    z = Math.imul(z ^ (z >>> 16), 0x85ebca6b) >>> 0;
    z = Math.imul(z ^ (z >>> 13), 0xc2b2ae35) >>> 0;
    return (z ^ (z >>> 16)) >>> 0;
  };
  let a = splitmix(), b = splitmix(), c = splitmix(), d = splitmix();
  const next = () => {                       // uniform in [0, 1)
    const t = (((a + b) >>> 0) + d) >>> 0;
    d = (d + 1) >>> 0;
    a = b ^ (b >>> 9);
    b = (c + (c << 3)) >>> 0;
    c = ((c << 21) | (c >>> 11)) >>> 0;
    c = (c + t) >>> 0;
    return t / 4294967296;
  };
  for (let i = 0; i < 20; i++) next();       // warm up
  return next;
}

// ---------- helpers ----------
// Sort ascending and drop duplicates; returns a (possibly shorter) Float64Array.
function sortUnique(arr) {
  arr.sort();                                 // typed-array sort is numeric
  let w = 0;
  for (let r = 0; r < arr.length; r++) {
    if (r === 0 || arr[r] !== arr[r - 1]) arr[w++] = arr[r];
  }
  return arr.subarray(0, w);
}

function mergeUnique(a, b) {
  const all = new Float64Array(a.length + b.length);
  all.set(a, 0);
  all.set(b, a.length);
  return sortUnique(all);
}

// Turn raw endpoint pairs into sorted unique keys, dropping self-loops.
function canonKeys(us, vs, count, n) {
  const keys = new Float64Array(count);
  let w = 0;
  for (let i = 0; i < count; i++) {
    const u = us[i], v = vs[i];
    if (u === v) continue;
    keys[w++] = u < v ? u * n + v : v * n + u;
  }
  return sortUnique(keys.slice(0, w));
}

// Random subset of exactly `target` keys (partial Fisher-Yates), sorted.
function trim(keys, target, rng) {
  if (keys.length <= target) return keys;
  const a = Float64Array.from(keys);
  for (let i = 0; i < target; i++) {
    const j = i + Math.floor(rng() * (a.length - i));
    const t = a[i]; a[i] = a[j]; a[j] = t;
  }
  const out = a.slice(0, target);
  out.sort();
  return out;
}

// ---------- generators ----------
function genER(n, target, rng) {
  let keys = new Float64Array(0);
  while (keys.length < target) {
    const k = Math.floor((target - keys.length) * 1.05) + 16;
    const us = new Int32Array(k), vs = new Int32Array(k);
    for (let i = 0; i < k; i++) { us[i] = Math.floor(rng() * n); vs[i] = Math.floor(rng() * n); }
    keys = mergeUnique(keys, canonKeys(us, vs, k, n));
  }
  return trim(keys, target, rng);
}

function rmatEdges(scale, k, rng, us, vs) {
  const a = 0.57, b = 0.19, c = 0.19;
  const ab = a + b, abc = a + b + c;
  for (let i = 0; i < k; i++) {
    let u = 0, v = 0;
    for (let l = 0; l < scale; l++) {
      const r = rng();
      const ubit = r >= ab ? 1 : 0;                           // quadrants c, d
      const vbit = ((r >= a && r < ab) || r >= abc) ? 1 : 0;  // quadrants b, d
      u = (u << 1) | ubit;
      v = (v << 1) | vbit;
    }
    us[i] = u; vs[i] = v;
  }
}

function genRMAT(scale, target, rng) {
  const n = 2 ** scale;
  let keys = new Float64Array(0);
  while (keys.length < target) {
    const k = Math.floor((target - keys.length) * 1.3) + 16;
    const us = new Int32Array(k), vs = new Int32Array(k);
    rmatEdges(scale, k, rng, us, vs);
    keys = mergeUnique(keys, canonKeys(us, vs, k, n));
  }
  keys = trim(keys, target, rng);

  // random vertex relabelling
  const perm = new Int32Array(n);
  for (let i = 0; i < n; i++) perm[i] = i;
  for (let i = n - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    const t = perm[i]; perm[i] = perm[j]; perm[j] = t;
  }
  const out = new Float64Array(keys.length);
  for (let i = 0; i < keys.length; i++) {
    const hi = keys[i] % n, lo = (keys[i] - hi) / n;
    const pl = perm[lo], ph = perm[hi];
    out[i] = pl < ph ? pl * n + ph : ph * n + pl;
  }
  return out;
}

// ---------- output ----------
function writeMtx(file, n, keys) {
  const fd = fs.openSync(file, "w");
  fs.writeSync(fd, "%%MatrixMarket matrix coordinate pattern symmetric\n");
  fs.writeSync(fd, "% generated by gen_graphs.js\n");
  fs.writeSync(fd, `${n} ${n} ${keys.length}\n`);
  const CHUNK = 200000;
  for (let s = 0; s < keys.length; s += CHUNK) {
    const e = Math.min(s + CHUNK, keys.length);
    const lines = new Array(e - s);
    for (let i = s; i < e; i++) {
      const hi = keys[i] % n, lo = (keys[i] - hi) / n;
      lines[i - s] = (hi + 1) + " " + (lo + 1);              // lower triangle, 1-indexed
    }
    fs.writeSync(fd, lines.join("\n") + "\n");
  }
  fs.closeSync(fd);
}

// ---------- main ----------
function parseArgs(argv) {
  const opt = { scale: 20, avgDegree: 16, seed: 1, outdir: "graphs" };
  for (let i = 0; i < argv.length; i++) {
    const flag = argv[i], val = argv[i + 1];
    if (flag === "--scale") { opt.scale = parseInt(val, 10); i++; }
    else if (flag === "--avg-degree") { opt.avgDegree = parseInt(val, 10); i++; }
    else if (flag === "--seed") { opt.seed = parseInt(val, 10); i++; }
    else if (flag === "--outdir") { opt.outdir = val; i++; }
    else { console.error(`unknown option: ${flag}`); process.exit(2); }
  }
  if (!(opt.scale >= 1 && opt.scale <= 26)) { console.error("--scale must be in 1..26"); process.exit(2); }
  return opt;
}

function main() {
  const opt = parseArgs(process.argv.slice(2));
  const n = 2 ** opt.scale;
  const target = Math.floor((n * opt.avgDegree) / 2);
  fs.mkdirSync(opt.outdir, { recursive: true });
  const rng = makeRng(opt.seed);

  const jobs = [
    ["er", () => genER(n, target, rng)],
    ["rmat", () => genRMAT(opt.scale, target, rng)],
  ];
  for (const [name, gen] of jobs) {
    const keys = gen();
    const file = path.join(opt.outdir, `${name}.mtx`);
    writeMtx(file, n, keys);
    console.log(`${name}: n=${n} undirected edges=${keys.length} -> ${file}`);
  }
}

main();
