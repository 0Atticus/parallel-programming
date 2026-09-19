const fs = require('fs');

// Helper to calculate median
function getMedian(numbers) {
  if (!numbers.length) return 0;
  const sorted = [...numbers].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  return sorted.length % 2 !== 0
    ? sorted[mid]
    : (sorted[mid - 1] + sorted[mid]) / 2;
}

// Helper to calculate mode(s)
function getMode(numbers) {
  const counts = {};
  let maxCount = 0;
  numbers.forEach((num) => {
    counts[num] = (counts[num] || 0) + 1;
    if (counts[num] > maxCount) maxCount = counts[num];
  });

  const modes = Object.keys(counts)
    .filter((num) => counts[num] === maxCount)
    .map(Number);

  return modes.length === numbers.length ? [modes[0]] : modes;
}

// Render simple ASCII bar chart in console
function printAsciiChart(title, data) {
  console.log(`\n================ ${title} ================`);
  const maxVal = Math.max(...data.map((d) => d.median));
  const barLength = 30;

  data.forEach((item) => {
    const fill = Math.round((item.median / maxVal) * barLength);
    const bar = '█'.repeat(fill) + '░'.repeat(barLength - fill);
    console.log(`${item.setting.padEnd(32)} | ${bar} | ${item.median} ns`);
  });
}

// 1. Process part1.csv (whitespace-delimited values)
function processPart1(filePath) {
  const fileContent = fs.readFileSync(filePath, 'utf8');
  const lines = fileContent.trim().split('\n').slice(1); // skip header row

  const groups = {};

  lines.forEach((line) => {
    const parts = line.trim().split(/\s+/);
    if (parts.length < 5) return;

    const [type, variant, input, n, timeStr] = parts;
    const settingKey = `${type}_${variant}_${input}_n=${n}`;
    const timeNs = parseFloat(timeStr);

    if (!groups[settingKey]) groups[settingKey] = [];
    groups[settingKey].push(timeNs);
  });

  const results = Object.keys(groups).map((key) => ({
    setting: key,
    median: getMedian(groups[key]),
    mode: getMode(groups[key]),
  }));

  return results;
}

// 2. Process part2.csv (comma-delimited values)
function processPart2(filePath) {
  const fileContent = fs.readFileSync(filePath, 'utf8');
  const lines = fileContent.trim().split('\n').slice(1);

  const groups = {};

  lines.forEach((line) => {
    if (!line.trim()) return;
    const [opt, type, n, rep, timeStr] = line.split(',').map((s) => s.trim());
    const settingKey = `${opt}_${type}_n=${n}`;
    const timeNs = parseFloat(timeStr);

    if (!groups[settingKey]) groups[settingKey] = [];
    groups[settingKey].push(timeNs);
  });

  const results = Object.keys(groups).map((key) => ({
    setting: key,
    median: getMedian(groups[key]),
    mode: getMode(groups[key]),
  }));

  return results;
}

// Execution
try {
  const part1Stats = processPart1('data/part1.csv');
  const part2Stats = processPart2('data/part2.csv');

  console.log('--- PART 1 STATS (Median & Mode) ---');
  console.table(part1Stats);

  console.log('--- PART 2 STATS (Median & Mode) ---');
  console.table(part2Stats);

  // Visual terminal charts
  printAsciiChart('PART 1 MEDIAN TIMINGS', part1Stats);
  printAsciiChart('PART 2 MEDIAN TIMINGS', part2Stats);
} catch (err) {
  console.error('Error processing CSV files:', err.message);
}