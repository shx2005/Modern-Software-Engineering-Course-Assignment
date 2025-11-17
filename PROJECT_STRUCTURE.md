# Tank Red Envelope Project Structure

This document captures how the repository is organized, which components live in each directory, and how the primary classes collaborate across the backend and frontend.

## Top-Level Layout

| Path | Purpose |
| --- | --- |
| `Makefile` | clang++ build rules that compile every `.cpp` file under `src/` into `bin/tank_red_envelope`. |
| `include/` | Public headers grouped by domain (`backend/`, `frontend/`). |
| `src/` | Source implementations mirroring the header layout. |
| `web/` | Browser UI (HTML/CSS/JS) served as `/` or `/static/index.html`. |
| `static/` | Auxiliary assets (images used by the UI). |
| `logs/` | Runtime log output; `backend::Logger` truncates `logs/server.log` on startup. |
| `bin/` | Build output target directory created by the Makefile. |
| `modification_log.txt` | Chronological development log for reference. |

## Backend Modules (`include/backend`, `src/backend`)

### Core Gameplay

- **`GameEngine`** (`GameEngine.hpp/.cpp`): Coordinates the simulation loop. Owns the `Tank`, a vector of `RedEnvelope` entities, `CollectionStats`, and timing state. Exposes movement (`moveTank`), envelope spawning (`spawnBonusEnvelopes`), pause control, time tracking, and read-only accessors for the frontend. Private helpers (`createRandomEnvelope`, `respawnEnvelope`, `handleCollisions`) ensure envelopes do not spawn on top of the tank and respawn immediately after collection.
- **`GameConfig`**: Simple struct for world dimensions, initial envelope count, and time limit. Passed into `GameEngine` at construction.
- **`CollectionStats`**: Aggregates how many envelopes have been collected and their accumulated value.

### Entities

- **`Tank`** (`Tank.hpp/.cpp`): Represents the player-controlled entity. Handles grid position, acceleration/momentum to create smoother movement, boundary clamping, and exposes `move`, `setPosition`, and basic getters. Helper `isColliding(const Tank&, const RedEnvelope&)` provides collision detection logic used by the engine.
- **`RedEnvelope`** (`RedEnvelope.hpp/.cpp`): Immutable metadata (id, size, value, collection radius) plus mutable position setters/getters. `EnvelopeSize` enum differentiates Small/Medium/Large envelopes.

### Logging

- **`Logger`** (`Logger.hpp/.cpp`): Thread-safe singleton that mirrors timestamped logs to both `logs/server.log` and stdout. Lazily creates its internal `Impl` containing the log stream, and exposes `initialize` plus `log`. Used heavily by the web server to audit HTTP requests and gameplay actions.

### Code Statistics Utility

- **`CodeStatsAnalyzer`** (`CodeStats.hpp/.cpp`): Filesystem walker that counts language-specific files (C/C++, Java, Python). Supports options to include blank/comment lines and collects Python function length details. Guards against escaping the workspace directory and skips known folders such as `.git`, `bin`, and `logs`.
- **`CodeStatsFacade`** (`CodeStatsFacade.hpp/.cpp`): Simplifies consuming `CodeStatsAnalyzer` through higher-level functions (`analyzeAll`, `analyzeCppOnly`, `analyzeJavaOnly`). Includes reporting helpers used by the frontend (`printLongestFunction`, `printShortestFunction`) and C-style wrappers (`get_cpp_code_stats`, etc.) for future FFI exposure.

## Frontend Modules (`include/frontend`, `src/frontend`)

### Layout Personalization

- **`LayoutManager`** (`LayoutManager.hpp/.cpp`): Thread-safe container storing per-user layout preferences (theme, preset, compact mode). Currently provides in-memory read/write operations plus TODO hooks (`initialize`, `persist`) for future storage and serialization (`exportPreferences`).

### HTTP + API Layer

- **`WebServer`** (`WebServer.hpp/.cpp`):
  - Owns references to the shared `backend::GameEngine` and `frontend::LayoutManager`.
  - Listens on a configurable port (defaults to 8080, with fallback attempts) and serves both static assets and REST-style endpoints.
  - Endpoints include gameplay actions (`/move`, `/reset`, `/rain`, `/pause`), state polling (`/state`), and code analytics (`/codestats`, `/codestats/export` supporting CSV/JSON/XLSX via an in-memory ZIP builder).
  - Uses parsing helpers (`parseDirection`, `parseLanguages`, etc.) to translate URL-encoded form data. Thread safety is enforced through `m_engineMutex` while mutating or reading the engine.
  - Response helpers (`sendHttpResponse`, `sendNotFound`, `sendBadRequest`, `sendInternalError`) centralize socket output formatting, while `loadStaticFile` prioritizes files in `web/` and falls back to project-root-relative paths.
  - Reporting helpers (`buildStateJson`, `buildCodeStatsJson`, `buildCsvReport`, `buildJsonReport`, `buildXlsxReport`, `buildLayoutSettingsJson`) provide the client UI with live game state and code statistics visualizations.

### Application Entry Point

- **`src/frontend/main.cpp`**: Initializes logging, configures the `GameEngine`, seeds RNG, resolves the listening port (`resolvePort`, `sanitizePort` consider env var `TANK_GAME_PORT` and CLI argument), creates the `LayoutManager`, and launches `WebServer::run()`. Any uncaught exception is logged and surfaced on stderr before exiting with a non-zero status.

## Build & Runtime Flow

1. `make` compiles all backend and frontend sources using C++17, outputting `bin/tank_red_envelope`.
2. Running the binary initializes the logger and engine, seeds randomness, and starts the HTTP server.
3. The frontend (HTML/JS under `web/`) polls `/state` and submits POST requests for gameplay and analytics actions.
4. The backend enforces gameplay rules and statistics, while results are serialized to JSON/CSV/XLSX for the browser to consume.

## Relationships & Data Flow Summary

- Frontend-to-backend communication happens exclusively through `WebServer`, which serializes requests into `GameEngine` calls and `CodeStatsFacade` invocations.
- `GameEngine` delegates spatial logic to `Tank` and `RedEnvelope`, and tracks stats for the client HUD.
- `Logger` is a shared facility used across backend and frontend code to keep a unified audit trail.
- `CodeStatsFacade` bridges backend analytics to frontend endpoints, letting the UI inspect repository code without leaving the workspace.

This overview should be updated whenever new modules are introduced or existing responsibilities change to keep the architectural snapshot aligned with the codebase.
