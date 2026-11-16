const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { BACKLOG_DATA: data, validateIndex, selectTasks } = require('../BACKLOG/backlog.js');

const root = path.join(__dirname, '../BACKLOG');
const clone = () => structuredClone(data);

test('BACKLOG_DATA preserves every legacy entry, status and Markdown target', () => {
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
    assert.throws(() => validateIndex(invalid), /BACKLOG_DATA/);
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

test('classic script starts from BACKLOG_DATA and builds the page without file or network APIs', () => {
  // Minimal DOM surface: execute the real browser entry point, not a separate loader.
  class Element {
    constructor() { this.children = []; this.dataset = {}; this.listeners = {}; this.attributes = {}; this.hidden = true; }
    append(...nodes) { this.children.push(...nodes.flatMap((node) => node.fragment ? node.children : [node])); }
    replaceChildren(...nodes) { this.children = []; this.append(...nodes); }
    get lastChild() { return this.children.at(-1); }
    addEventListener(event, callback) { this.listeners[event] = callback; }
    setAttribute(key, value) { this.attributes[key] = value; }
  }
  const html = fs.readFileSync(path.join(root, 'index.html'), 'utf8');
  const elements = new Map([...html.matchAll(/\bid="([^"]+)"/g)].map(([, id]) => [id, new Element()]));
  const context = vm.createContext({
    document: {
      getElementById: (id) => elements.get(id) ?? null,
      createElement: () => new Element(),
      createDocumentFragment: () => Object.assign(new Element(), { fragment: true }),
    },
    // No fetch, FileReader, location or CommonJS module: just a classic browser script.
  });
  assert.match(html, /<script src="backlog\.js" defer><\/script>/);
  vm.runInContext(fs.readFileSync(path.join(root, 'backlog.js'), 'utf8'), context);
  const get = (id) => elements.get(id);
  assert.equal(get('workspace').hidden, false);
  assert.equal(get('load-message').hidden, true);
  assert.equal(get('total').textContent, data.tasks.length);
  assert.equal(get('task-list').children.length, data.tasks.length);
  assert.equal(get('task-list').children[0].children[1].children[0].href, selectTasks(data)[0].file);
  get('search').listeners.input({ target: { value: '09 руль' } });
  assert.equal(get('task-list').children.length, 0);
  assert.equal(get('empty').hidden, false);
  get('reset').listeners.click();
  assert.equal(get('task-list').children.length, data.tasks.length);
});
