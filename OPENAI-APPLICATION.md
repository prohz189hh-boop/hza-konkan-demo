# Codex for Open Source application notes

## Why does this repository qualify?

This repository is an honest, small open-source starting point rather than an established ecosystem project. The scoped `packages/konkan-rules/` component is MIT-licensed and contains deterministic tile-game rules, redacted-view bot decisions, rack helpers, tests, and documentation. The surrounding repository documents a real Unreal/TypeScript multiplayer game project while deliberately keeping production infrastructure and proprietary assets reserved.

The public component gives other developers a useful rules core they can test, adapt, and embed in experiments. It does not claim a large contributor base, adoption, downloads, or completed product.

The browser playground is only a technical demonstration of that package. The complete HZA KONKAN game is a separate Unreal Engine project and is not open source. This separation lets reviewers evaluate a real, reusable contribution without implying that proprietary game code, art, infrastructure, or branding has been released.

## Why does the project need Codex Security?

The project crosses Unreal C++, TypeScript Workers, Durable Objects, Supabase authentication, WebSockets, and client-side redaction. Codex Security could help review authorization boundaries, recipient-specific state, token handling, dependency changes, and accidental publication of private infrastructure as the public package evolves.

## How would API credits help?

Credits would support focused test generation, documentation review, security regression analysis, and portability work for the MIT-licensed rules package. They would help compare rule behavior across implementations, improve contributor onboarding, and maintain Kurdish-first documentation and localization tooling without moving private production credentials or proprietary content into the public repository.

For an independent developer with limited resources, that assistance would make it practical to review edge cases, explain server-authoritative design clearly, and keep the public package useful to TypeScript and Node.js developers while preserving the private production boundary.

## What is the open-source value?

- **Reusable TypeScript rules engine:** deterministic tile, meld, joker, scoring, rack, mode, and action-validation primitives can be tested or embedded independently.
- **Server-authoritative multiplayer design:** the exported APIs model intent validation and redacted views so clients do not need to be trusted with opponents' private hands.
- **Secure private-state handling:** tests exercise invalid actions and recipient-specific views; production credentials and service integrations remain outside the public package.
- **Kurdish-language and cultural representation:** the project documents a Kurdish-first product direction and includes English, Sorani, and Badini onboarding material for review and future contribution.
- **Automated collaboration:** package tests, type checking, linting, coverage, and the GitHub Actions workflow give contributors a repeatable baseline.

## Anything else reviewers should know?

HZA KONKAN is a Kurdish-first multiplayer game project. Kurdish Sorani and Badini support are part of the product direction, with Arabic and English planned as additional languages. The public repository intentionally separates a reusable rules component from reserved Unreal, backend, art, branding, and deployment material. Current tests prove the exported rules and local Worker flows; they do not claim a production release, four-client end-to-end deployment, or a mature open-source community.
