# Codex for Open Source application notes

## Why does this repository qualify?

This repository is an honest, small open-source starting point rather than an established ecosystem project. The scoped `packages/konkan-rules/` component is MIT-licensed and contains deterministic tile-game rules, redacted-view bot decisions, rack helpers, tests, and documentation. The surrounding repository documents a real Unreal/TypeScript multiplayer game project while deliberately keeping production infrastructure and proprietary assets reserved.

The public component gives other developers a useful rules core they can test, adapt, and embed in experiments. It does not claim a large contributor base, adoption, downloads, or completed product.

## Why does the project need Codex Security?

The project crosses Unreal C++, TypeScript Workers, Durable Objects, Supabase authentication, WebSockets, and client-side redaction. Codex Security could help review authorization boundaries, recipient-specific state, token handling, dependency changes, and accidental publication of private infrastructure as the public package evolves.

## How would API credits help?

Credits would support focused test generation, documentation review, security regression analysis, and portability work for the MIT-licensed rules package. They would help compare rule behavior across implementations, improve contributor onboarding, and maintain Kurdish-first documentation and localization tooling without moving private production credentials or proprietary content into the public repository.

## Anything else reviewers should know?

HZA KONKAN is a Kurdish-first multiplayer game project. Kurdish Sorani and Badini support are part of the product direction, with Arabic and English planned as additional languages. The public repository intentionally separates a reusable rules component from reserved Unreal, backend, art, branding, and deployment material. Current tests prove the exported rules and local Worker flows; they do not claim a production release, four-client end-to-end deployment, or a mature open-source community.
