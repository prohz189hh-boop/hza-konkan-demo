# Source selection and adaptations

Origin: private HZA KONKAN checkpoint `8c0b4f3`, 2026-10-08. No private Git history is included.

- `selected-source/unreal/HzaNetwork`: existing native C++ source, headers and module build definition. Includes input, auth, lobby, room transport, table presentation and native tests. Deployment identifiers/key replaced with placeholders. Maps, engine, plugin descriptor/content and binaries omitted: this is not a standalone importable plugin.
- `selected-source/backend`: existing Worker, Durable Object, identity, result and voice integration source. No Wrangler deployment config or secrets included.
- `selected-source/lib`: existing game rules/supporting modules and WebSocket client. Some optional modules require dependencies; default tests use no external packages.
- `selected-source/blender/build_rack_historical.py`: historical rack construction workflow. Requires Blender and source inputs omitted from this curated archive; not final approved art and not a one-command demo.
- `tests`: copied actual project tests. Relative import and Worker entrypoint paths adjusted to the selected-source folder; logic otherwise preserved.
- `docs/BACKEND-PROTOCOL.md`: existing protocol notes. Describes private-project commands; use TESTING.md for this package's commands.

The package contains no raw private repository, production configuration, player data, compiled release, or guarantee of complete project reproducibility.
