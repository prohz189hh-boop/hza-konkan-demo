# Contributing

This repository is a curated engineering showcase rather than the full HZA KONKAN production repository. Contributions are welcome to the MIT-licensed `packages/konkan-rules/` component and to documentation, tests, and security reports that do not expose private project material.

The rest of the repository is reserved. Do not copy, refactor, or redistribute Unreal source, production backend integrations, screenshots, branding, Blender assets, deployment details, or other proprietary material into a new public package without written approval from the project owner.

## Development

Run the default tests with Node.js 22.13 or newer:

```sh
npm test
python tools/verify_demo.py
```

For rules-only work:

```sh
cd packages/konkan-rules
npm test
```

Keep changes focused, add regression tests for rule behavior, and describe compatibility implications in the pull request. The project currently has no promise of API stability.

If you are reviewing the project, useful feedback includes:

- architecture or multiplayer correctness observations
- security/privacy concerns
- reproducible issues in the exported demo tests
- documentation corrections

Please do not post credentials, tokens, private player information, or suspected secrets in a public issue. Security concerns should be reported privately to the repository owner.
