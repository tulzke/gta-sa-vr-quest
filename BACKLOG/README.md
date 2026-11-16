# Backlog

For the author's convenience, the entire backlog is maintained in Russian.

Open [index.html](index.html) directly; no server is needed. Edit `BACKLOG_DATA` at the top of [backlog.js](backlog.js), then refresh. Each task has `id`, `title`, `file`, `categories`, `priority` (`critical`, `high`, `medium`, `low`, `none`) and `status` (`open`, `in_progress`). Leave the page logic below the data block unchanged.

Keep [BACKLOG.md](BACKLOG.md) and task Markdown files in sync; closed tasks go to `ARCHIVE/`. See [AGENTS.md](../AGENTS.md) for priorities and workflow. Validate from the repository root: `node --test tools/test-backlog.cjs`.
