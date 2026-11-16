/* Shared data rules are also exercised by tools/test-backlog.cjs. No dependencies. */
"use strict";
(() => {
  const priorities = {
    critical: { label: "Критический", rank: 0 },
    high: { label: "Высокий", rank: 1 },
    medium: { label: "Средний", rank: 2 },
    low: { label: "Низкий", rank: 3 },
    none: { label: "Не задан", rank: 4 },
  };
  const statuses = { open: "Открыта", in_progress: "В работе" };
  const has = (object, key) => Object.hasOwn(object, key);

  function validateIndex(data) {
    const fail = (message) => { throw new Error(`Некорректный index.json: ${message}`); };
    if (!data || data.version !== 1 || !Array.isArray(data.categories) || !Array.isArray(data.tasks)) {
      fail("ожидаются version: 1, массивы categories и tasks.");
    }
    const categories = new Set();
    for (const category of data.categories) {
      if (!category || typeof category.id !== "string" || !/^[a-z][a-z0-9-]*$/.test(category.id) ||
          category.id === "all" || categories.has(category.id) ||
          typeof category.title !== "string" || !category.title.trim()) fail("проверьте категории и уникальность их id.");
      categories.add(category.id);
    }
    const ids = new Set();
    const files = new Set();
    for (const task of data.tasks) {
      if (!task || !Number.isInteger(task.id) || task.id < 1 || ids.has(task.id)) fail("номер задачи должен быть уникальным положительным числом.");
      if (typeof task.title !== "string" || !task.title.trim()) fail(`у задачи ${task.id} нет названия.`);
      if (typeof task.file !== "string" || !/^\d+-[a-z0-9-]+\.md$/.test(task.file) ||
          Number(task.file.split("-")[0]) !== task.id || files.has(task.file)) fail(`проверьте Markdown-файл задачи ${task.id}.`);
      if (!has(priorities, task.priority) || !has(statuses, task.status)) fail(`проверьте priority и status задачи ${task.id}.`);
      if (!Array.isArray(task.categories) || !task.categories.length ||
          new Set(task.categories).size !== task.categories.length ||
          task.categories.some((id) => !categories.has(id))) fail(`проверьте categories задачи ${task.id}.`);
      ids.add(task.id);
      files.add(task.file);
    }
    return data;
  }

  function selectTasks(data, { category = "all", status = "all", search = "", sort = "priority-desc" } = {}) {
    const words = search.trim().toLocaleLowerCase("ru").split(/\s+/).filter(Boolean);
    const tasks = data.tasks.filter((task) =>
      (category === "all" || task.categories.includes(category)) &&
      (status === "all" || task.status === status) &&
      words.every((word) => `${String(task.id).padStart(2, "0")} ${task.title}`.toLocaleLowerCase("ru").includes(word)));
    return tasks.sort((a, b) => {
      if (sort === "title") return a.title.localeCompare(b.title, "ru") || a.id - b.id;
      if (sort === "id") return a.id - b.id;
      // Unassigned tasks always follow assigned tasks, in either direction.
      if ((a.priority === "none") !== (b.priority === "none")) return a.priority === "none" ? 1 : -1;
      const direction = sort === "priority-asc" ? -1 : 1;
      return direction * (priorities[a.priority].rank - priorities[b.priority].rank) || a.id - b.id;
    });
  }

  if (typeof module !== "undefined" && module.exports) module.exports = { validateIndex, selectTasks, priorities };
  if (typeof document === "undefined") return;

  const $ = (id) => document.getElementById(id);
  let data;
  const state = { category: "all", status: "all", search: "", sort: "priority-desc" };

  function element(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function chooseCategory(id) {
    state.category = id;
    render();
  }

  function render() {
    const tasks = selectTasks(data, state);
    const categoryNames = new Map(data.categories.map((category) => [category.id, category.title]));
    const counts = new Map();
    for (const task of selectTasks(data, { ...state, category: "all" })) {
      for (const category of task.categories) counts.set(category, (counts.get(category) || 0) + 1);
    }
    // Keep category buttons mounted so keyboard focus survives filtering.
    for (const button of $("categories").children) {
      const id = button.dataset.category;
      button.setAttribute("aria-pressed", String(id === state.category));
      button.lastChild.textContent = id === "all"
        ? selectTasks(data, { ...state, category: "all" }).length : counts.get(id) || 0;
    }
    $("category-title").textContent = categoryNames.get(state.category) || "Все задачи";
    $("result-count").textContent = `${tasks.length} из ${data.tasks.length}`;
    $("empty").hidden = tasks.length > 0;
    const prioritySort = state.sort.startsWith("priority-");
    $("priority-heading").setAttribute("aria-sort", prioritySort ? (state.sort === "priority-desc" ? "descending" : "ascending") : "none");
    $("sort-arrow").textContent = prioritySort ? (state.sort === "priority-desc" ? "↓" : "↑") : "↕";
    const rows = document.createDocumentFragment();
    for (const task of tasks) {
      const row = element("tr");
      row.append(element("td", "number", String(task.id).padStart(2, "0")));
      const description = element("td");
      const link = element("a", "task-link", task.title);
      link.href = task.file;
      description.append(link);
      const tags = element("div", "tags");
      for (const category of task.categories) {
        const tag = element("button", "tag", categoryNames.get(category));
        tag.type = "button";
        tag.title = `Показать категорию «${categoryNames.get(category)}»`;
        tag.addEventListener("click", () => {
          chooseCategory(category);
          // The clicked row can disappear; move focus to the persistent filter.
          Array.from($("categories").children).find((button) => button.dataset.category === category).focus();
        });
        tags.append(tag);
      }
      description.append(tags);
      row.append(description);
      const status = element("td");
      status.append(element("span", `status${task.status === "in_progress" ? " active" : ""}`, statuses[task.status]));
      row.append(status);
      const priority = element("td");
      const badge = element("span", "priority", priorities[task.priority].label);
      badge.dataset.priority = task.priority;
      priority.append(badge);
      row.append(priority);
      rows.append(row);
    }
    $("task-list").replaceChildren(rows);
  }

  function loadIndex(value) {
    data = validateIndex(value);
    state.category = "all";
    $("total").textContent = data.tasks.length;
    $("in-progress").textContent = data.tasks.filter((task) => task.status === "in_progress").length;
    const categories = [{ id: "all", title: "Все задачи" }, ...data.categories];
    $("categories").replaceChildren(...categories.map((category) => {
      const button = element("button", "category");
      button.type = "button";
      button.dataset.category = category.id;
      button.append(element("span", "", category.title), element("span", "category-count"));
      button.addEventListener("click", () => chooseCategory(category.id));
      return button;
    }));
    $("load-message").hidden = true;
    $("workspace").hidden = false;
    render();
  }

  function showError(error) {
    $("workspace").hidden = true;
    $("total").textContent = $("in-progress").textContent = "—";
    $("load-message").textContent = `Не удалось загрузить индекс. ${error.message}`;
    $("load-message").hidden = false;
  }

  $("search").addEventListener("input", (event) => { state.search = event.target.value; render(); });
  $("status").addEventListener("change", (event) => { state.status = event.target.value; render(); });
  $("sort").addEventListener("change", (event) => { state.sort = event.target.value; render(); });
  $("priority-sort").addEventListener("click", () => {
    state.sort = state.sort === "priority-desc" ? "priority-asc" : "priority-desc";
    $("sort").value = state.sort;
    render();
  });
  $("reset").addEventListener("click", () => {
    Object.assign(state, { category: "all", status: "all", search: "", sort: "priority-desc" });
    $("search").value = "";
    $("status").value = "all";
    $("sort").value = state.sort;
    render();
  });
  let fileLoad = 0;
  $("json-file").addEventListener("change", async (event) => {
    const attempt = ++fileLoad;
    const file = event.target.files[0];
    if (!file) return;
    try {
      const value = JSON.parse(await file.text());
      if (attempt === fileLoad) loadIndex(value);
    } catch (error) { if (attempt === fileLoad) showError(error); }
  });

  if (location.protocol === "file:") {
    $("file-loader").hidden = false;
  } else {
    $("load-message").hidden = false;
    $("load-message").textContent = "Загружаем задачи…";
    fetch("index.json", { cache: "no-store" })
      .then((response) => {
        if (!response.ok) throw new Error(`HTTP ${response.status}. Проверьте наличие index.json рядом со страницей.`);
        return response.json();
      })
      .then(loadIndex).catch(showError);
  }
})();
