const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { validateIndex, selectTasks } = require('../BACKLOG/backlog.js');

const root = path.join(__dirname, '../BACKLOG');
const data = JSON.parse(fs.readFileSync(path.join(root, 'index.json'), 'utf8'));
const clone = () => structuredClone(data);

test('JSON preserves every legacy entry, status and Markdown target', () => {
  validateIndex(data);
  const legacy = [...fs.readFileSync(path.join(root, 'BACKLOG.md'), 'utf8').matchAll(/^(\d+)\\\. (.+)$/gm)];
  assert.ok(legacy.length > 0);
  assert.equal(data.tasks.length, legacy.length);
  for (const [, number, rawTitle] of legacy) {
    const task = data.tasks.find((task) => task.id === Number(number));
    assert.ok(task, `Missing task ${number}`);
    assert.equal(task.title, rawTitle.replace(/\s*\[В РАБОТЕ\]\s*$/, '').trim());
    assert.equal(task.status, rawTitle.includes('[В РАБОТЕ]') ? 'in_progress' : 'open');
    assert.ok(fs.statSync(path.join(root, task.file)).isFile(), task.file);
  }
});

test('invalid references, duplicate ids, priority/status and unsafe paths fail clearly', () => {
  for (const mutate of [
    (d) => { d.version = 2; },
    (d) => { d.tasks.push({ ...d.tasks[0] }); },
    (d) => { d.categories.push({ ...d.categories[0] }); },
    (d) => { d.tasks[0].categories = ['missing']; },
    (d) => { d.tasks[0].categories = []; },
    (d) => { d.tasks[0].priority = '__proto__'; },
    (d) => { d.tasks[0].status = 'done'; },
    (d) => { d.tasks[0].file = '../README.md'; },
    (d) => { d.tasks[0].file = 'javascript:alert(1)'; },
  ]) {
    const invalid = clone();
    mutate(invalid);
    assert.throws(() => validateIndex(invalid), /index.json/);
  }
});

test('priority sorting puts unassigned last both ways, breaks ties by id and leaves data intact', () => {
  const sample = { tasks: [
    { id: 9, priority: 'none' }, { id: 8, priority: 'low' },
    { id: 4, priority: 'high' }, { id: 2, priority: 'critical' },
    { id: 6, priority: 'medium' }, { id: 3, priority: 'high' },
    { id: 1, priority: 'none' },
  ].map((task) => ({ ...task, title: String(task.id), categories: ['test'], status: 'open' })) };
  const original = structuredClone(sample);
  assert.deepEqual(selectTasks(sample).map((task) => task.id), [2, 3, 4, 6, 8, 1, 9]);
  assert.deepEqual(selectTasks(sample, { sort: 'priority-asc' }).map((task) => task.id), [8, 6, 3, 4, 2, 1, 9]);
  assert.deepEqual(selectTasks(sample, { sort: 'id' }).map((task) => task.id), [1, 2, 3, 4, 6, 8, 9]);
  assert.deepEqual(sample, original);
});

test('search, multiple category membership and status filter compose', () => {
  const sample = { tasks: [
    { id: 9, title: 'Руль автомобиля', categories: ['car', 'hands'], status: 'in_progress', priority: 'none' },
    { id: 10, title: 'Руль лодки', categories: ['boat', 'hands'], status: 'open', priority: 'none' },
    { id: 11, title: 'Звук автомобиля', categories: ['car'], status: 'open', priority: 'none' },
  ] };
  assert.deepEqual(selectTasks(sample, { category: 'hands', status: 'in_progress', search: ' РУЛЬ 09 ' }).map((task) => task.id), [9]);
  assert.equal(selectTasks(sample, { category: 'car', search: 'лодки' }).length, 0);
  assert.equal(selectTasks(sample, { search: 'автомобиля' }).length, 2);
  assert.deepEqual(selectTasks(sample, { sort: 'title' }).map((task) => task.id), [11, 9, 10]);
  assert.equal(selectTasks(sample, { category: 'unknown' }).length, 0);
});

test('local server loads the page/JSON/Markdown and rejects writes and outside files', { timeout: 10000 }, async () => {
  const child = spawn(process.execPath, [path.join(__dirname, 'backlog-server.mjs'), '0'], { stdio: ['ignore', 'pipe', 'pipe'] });
  try {
    const url = await new Promise((resolve, reject) => {
      let output = '';
      child.stdout.on('data', (chunk) => {
        output += chunk;
        const match = output.match(/http:\/\/127\.0\.0\.1:\d+\//);
        if (match) resolve(match[0]);
      });
      child.once('error', reject);
      child.once('exit', (code) => reject(new Error(`Server exited: ${code}`)));
    });
    const page = await fetch(url);
    assert.equal(page.status, 200);
    assert.match(await page.text(), /lang="ru"/);
    const json = await fetch(`${url}index.json`);
    assert.equal(json.status, 200);
    assert.deepEqual(await json.json(), data);
    const markdown = await fetch(`${url}${data.tasks[0].file}`);
    assert.match(markdown.headers.get('content-type'), /text\/plain; charset=utf-8/);
    assert.equal(await markdown.text(), fs.readFileSync(path.join(root, data.tasks[0].file), 'utf8'));
    assert.equal((await fetch(`${url}index.json`, { method: 'PUT', body: '{}' })).status, 405);
    assert.equal((await fetch(`${url}..%2f.git%2fconfig`)).status, 404);
    assert.equal((await fetch(`${url}..%5cREADME.md`)).status, 404);
    assert.equal((await fetch(`${url}%ZZ`)).status, 400);
    assert.equal((await fetch(`${url}missing.md`)).status, 404);
  } finally {
    const stopped = once(child, 'exit');
    child.kill();
    await stopped;
  }
});
