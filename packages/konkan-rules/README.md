# HZA KONKAN rules package

This package contains the reusable, deterministic TypeScript rules core for the HZA KONKAN tile game. It includes tile generation, joker and meld validation, scoring, round actions, redacted views, bot decisions, match modes, and rack ordering helpers.

The package is the only part of this repository released under MIT. The surrounding repository remains a reserved project showcase; see the root `LICENSE-NOTICE.md`.

## Use

The source is dependency-free and targets modern TypeScript runtimes. The included tests run directly with Node.js 22:

```sh
node --experimental-strip-types --test tests/engine.test.ts
```

This package is not a complete multiplayer server or Unreal plugin. Authentication, persistence, networking, production services, artwork, branding, and deployment code remain outside its license.
