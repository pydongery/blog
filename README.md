# Pydong blog

The [pydong](https://pydong.org) blog is generated from this repository. Feel free to fix typos, suggest posts or changes via [issues](https://github.com/pydongery/blog/issues) or reach out to me on Discord if you have any questions.

[Discussions](https://github.com/pydongery/blog/discussions) in this repository are synchronized with the comment section underneath blog posts.

The site uses Astro. Articles and updates live in
`content/<type>/<slug>/index.md`, with entry-specific images stored beside their
Markdown file. Shared site assets live in `assets/images/site`.

## Development

Install [Bun](https://bun.sh/), then run:

```sh
bun install
bun run dev
```

The dev server runs at <http://localhost:4321>. A Docker-backed Dev Container is
included as an alternative. Before publishing, run `bun run format:check`,
`bun run lint`, `bun run typecheck`, `bun test`, and `bun run build`.
