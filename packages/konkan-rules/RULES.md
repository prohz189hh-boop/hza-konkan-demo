# Rules core scope

The package models the deterministic rules used by the exported HZA KONKAN demo. It creates 106 physical tiles, computes the cup-derived joker, validates same-color runs and distinct-color groups, applies opening and Konkan constraints, enforces turn and ownership checks, calculates penalties, and produces recipient-redacted views.

The implementation is the authoritative description of this demo snapshot. Product rules may change in the private production game; consumers should pin a revision and add their own compatibility tests.

The package deliberately excludes authentication, storage, matchmaking, WebSocket transport, Unreal integration, economy, voice, and production configuration.
