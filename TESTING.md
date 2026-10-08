# Testing

## Reproduce the exported demo tests

Node.js 22.13+ is required. No npm install is needed for the default suite:

```sh
npm test
```

This runs copied project tests against the exported code: tile/rule validity and legal actions, hidden-state views, WebSocket ownership/replacement/malformed events, HTTPS enforcement, and cumulative match winner logic. The exact result from this exported folder is in [DEMO-TEST-RESULTS.txt](docs/DEMO-TEST-RESULTS.txt).

## Optional local Worker integration suite

```sh
npm install
npm run test:backend
```

This uses pinned Wrangler/Miniflare plus chess.js. It bundles the actual Worker, runs local Durable Objects/WebSockets, and substitutes a fake identity service that rejects unexpected outbound hosts. It tests assignments, membership, private-hand redaction, malformed messages and reconnect. It is not a production endpoint test. Dependencies are not bundled. This optional command has not been rerun in the exported folder; its source was previously run successfully in the private project.

## Verified in the original Unreal project

- Native compile/link: UE 5.8.3, MSVC 14.44, one compile action at a time.
- Four focused native tests: Auth.ProfileAndVault, Lobby.Lifecycle, Network.QueueCleanup, Network.SnapshotPrivacy; all passed, zero errors/warnings.
- Installed-map AuthFrontend, HandTouchInput and TablePresentation: all passed, zero errors/warnings, 60.57 seconds total.
- Real owner account restoration, mode selection, Find Match -> search -> cancel, private-room page and Profile/back navigation were inspected.

Native tests require the full private map/content and engine, intentionally absent here. Synthetic tests do not replace four separately authenticated Unreal clients completing a round. Physical touch on Android, release packaging and production load/performance validation remain pending.
