# Security and privacy

This public demo intentionally excludes deployment configuration, credentials, environment files, browser data, vault contents, player records, private UUIDs, logs and raw build directories. Public production endpoint identifiers and the publishable client key were also replaced with placeholders in the source selections.

The code contains **synthetic test values** such as `synthetic-access-token`, reserved example domains and all-zero-prefixed fixture UUIDs. They are nonfunctional test fixtures, not credentials. The screenshots show a public game display name and counters, not email, account UUID or session data.

Windows native session restoration uses the OS credential vault. No equivalent verified Android secure persistence is claimed. Private/service credentials belong only in server-side secret stores. Never put them in Unreal client code or a public repository.

The authoritative backend authenticates users, validates membership and legal actions, and sends recipient-specific snapshots. Economy, purchases, entitlements and daily claims must remain server-authoritative. Production authorization, concurrency and device gates remain part of the release checklist.

Before publishing modifications, repeat the included scan, review every added screenshot, and inspect Git history. The scan is a concrete pattern/identifier check, not a guarantee against every possible secret format. Report security concerns privately to the repository owner; never post tokens publicly.
