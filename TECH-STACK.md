# Tech stack

| Component | Current role |
| --- | --- |
| Unreal Engine 5.8.3 / C++ / UMG | Native client, input, HUD, menus and presentation |
| MSVC 14.44 / Windows SDK 10.0.22621 | Verified native Windows build toolchain |
| Blender 5.2.2 LTS | Mesh production and Blender-to-Unreal workflow |
| TypeScript / Node.js | Shared game rules, backend and automated tests |
| Cloudflare Workers / Durable Objects | API, matchmaking and authoritative live rooms |
| Supabase Auth / PostgreSQL | Identity and persistent account/game systems |
| LiveKit | Intended voice infrastructure; full client verification pending |

Windows and Android landscape are primary native targets. iOS is a later preparation target. Prior web/client code exists in the private project but is not represented as a finished native release.
