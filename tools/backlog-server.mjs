// Local, read-only backlog viewer. No packages or game build tools required.
import http from "node:http";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("../BACKLOG/", import.meta.url));
const port = Number(process.argv[2] ?? 8765);
if (!Number.isInteger(port) || port < 0 || port > 65535) {
  console.error("Usage: node tools/backlog-server.mjs [port: 0..65535]");
  process.exit(1);
}
const types = { ".html": "text/html", ".css": "text/css", ".js": "text/javascript", ".json": "application/json", ".md": "text/plain" };
const server = http.createServer(async (request, response) => {
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setHeader("Cache-Control", "no-store");
  if (request.method !== "GET" && request.method !== "HEAD") {
    response.writeHead(405, { Allow: "GET, HEAD" }).end();
    return;
  }
  try {
    // Decode before normalizing, and restrict reads to the backlog directory.
    const pathname = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
    const filename = path.resolve(root, `.${pathname === "/" ? "/index.html" : pathname}`);
    const relative = path.relative(root, filename);
    const extension = path.extname(filename);
    if (relative.startsWith("..") || path.isAbsolute(relative) || !Object.hasOwn(types, extension) || !(await stat(filename)).isFile()) {
      response.writeHead(404).end("Not found");
      return;
    }
    const contents = await readFile(filename);
    response.writeHead(200, { "Content-Type": `${types[extension]}; charset=utf-8` });
    response.end(request.method === "HEAD" ? undefined : contents);
  } catch (error) {
    response.writeHead(error instanceof URIError ? 400 : 404).end("Not found");
  }
});
server.on("error", (error) => {
  console.error(error.code === "EADDRINUSE" ? `Port ${port} is busy. Choose another: node tools/backlog-server.mjs 8766` : error.message);
  process.exitCode = 1;
});
server.listen(port, "127.0.0.1", () => {
  console.log(`Backlog: http://127.0.0.1:${server.address().port}/`);
  console.log("Press Ctrl+C to stop. Markdown files open as UTF-8 text.");
});
