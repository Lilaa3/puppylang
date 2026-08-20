#!/usr/bin/env node
const fs = require("fs");
const path = require("path");
const { marked } = require("marked");

const root = path.join(__dirname, "..");
const md = fs.readFileSync(path.join(root, "MECHANISMS.md"), "utf8");
const body = marked.parse(md);

const html = `<!doctype html>
<html lang="en">

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>puplang mechanisms</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>

  <link href="https://fonts.googleapis.com/css2?family=Luckiest+Guy&display=swap" rel="stylesheet">
  <link href="https://fonts.googleapis.com/css2?family=Gorditas:wght@400;700&display=swap" rel="stylesheet">

  <link rel="stylesheet" href="style.css">
</head>

<body>
  <div class="veil"></div>

<main class="mechanisms">
    <a class="repo" href="https://github.com/Lilaa3/puppylang" title="puplang on GitHub">
      <span>View on github</span>
      <img src="Octicons-mark-github.svg" alt="">
    </a>
    <article>
${body}
    </article>
    <a class="back" href="index.html"><-- back to the woofs</a>
  </main>

  <footer>🐾 © 2026 Lilaa3</footer>
</body>

</html>
`;

fs.writeFileSync(path.join(root, "web/dist/html/mechanisms.html"), html);