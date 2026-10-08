# Worker protocol (source contract)

Worker: `hza-konkan-multiplayer`; production deployment not changed here.
REST authentication: `Authorization: Bearer <Supabase access token>`. Worker validates through Supabase `/auth/v1/user`; client user/seat claims are ignored.

## Rooms
- `POST /api/rooms/create`: name, mode (regular/turbo/ranked), optional password, theme, public, botLevel, seatPlan, drawSeconds/playSeconds. Current response nests summary as `room.room`.
- `GET /room/CODE`: summary only.
- WebSocket `/room/CODE`: subprotocols `hza-konkan`, `auth.<access-token>`, optionally `roompw.<SHA256('room:'+password)>`. Do not put tokens in URLs/logs.
- Server `welcome`: seat (0–3), team (1–2), host, room. `state`: room, optional event. `error`: code.
- Client: get_state, ready {ready}, seat {seat}, add_bot {level}, remove_bot {id}, set_mode {mode}, start, message {text}, leave.
- `move {version, action}` uses exact shared engine Action union: draw {source:deck/discard,enterKonkan?}, discard {tile}, meld {groups:string[][]}, extend {meld,tile,side:start/end}, steal {meld,tile,joker}, finish {groups,discard}, next.
- There is no separate `open` action; opening uses meld. Do not invent a wire action.
- `room.game` is a seat-redacted GameView. Only the receiving seat has hand identities. Other hands are empty arrays plus tileCount; deck is deckCount; seed/entropy are absent. Room deadlines are server timestamps. Version tracks gameplay, not lobby changes.
- Reconnect authenticates again, replaces the previous socket and returns a new welcome. It preserves deadlines. Client should request get_state after uncertain delivery, never blindly replay a move.

## Matchmaking
POST `/api/matchmaking/join` {mode,region?,allowBots?}; POST leave; GET status.
Status is authenticated and user-specific: queued/idle or matched {roomCode,roomId}. Assignments persist for every participant for ten minutes, including repeated joins. Clients poll status while queued; no queue WebSocket exists yet. Leave cancels queued groups, not a completed assignment. Ten-minute assignment expiry is a delivery window, not match expiry.
New backend bot fallback and block filtering still need implementation/verification; allowBots alone does not implement fallback.

## Voice and persistence
POST `/api/voice/token` {room}: authenticated non-banned room members only; returns voice_not_configured until LiveKit secrets are configured. No secrets stored in clients.
Supabase RPCs implement economy/social contracts; schema and RLS are absent from this source download. Production project `YOUR_PROJECT` was not visible to the connected Supabase account (only a different inactive project was listed). No DB changes made.
Failed match settlement now remains pending and retries by alarm when a match ID and server credential exist. Production `settle_match` must be verified idempotent before release. Match creation failure recovery remains incomplete.
Match winner now follows accumulated scores, matching the existing web result screen; `game.winner` is only the final round winner. Ties remain unawarded/pending until the production RPC draw representation is verified; no arbitrary team receives tie rewards.

## Verification
`npm run test:backend` runs actual local Workers/Durable Objects and four WebSockets with a fake outbound identity service. It cannot contact production. Tests cover assignment delivery, auth requirement, voice membership, malformed/oversized payloads, hidden hands, reconnect, illegal ownership, stale moves and turn ownership. Client tests cover overlapping connects, close-before-open and invalid messages.
The four-socket test also plays a complete round using only each recipient's redacted view, verifies scoring and starts the next round. This proves the local server transport flow, not native Unreal integration.
Pinned workerd supports May 2026, so tests use 2026-05-15; production config retains 2026-09-26. Production runtime parity still needs validation.
