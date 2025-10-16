# Tank Red Envelope Game

This project implements a browser-playable tank game written in modern C++ with a clear backend/frontend separation. The backend simulates the grid world, while a lightweight HTTP server exposes the state to an HTML5 interface where players drive the tank to collect randomly spawned red envelopes within a time limit.

## Project Structure

```
include/
  backend/       # Public interfaces for game logic (engine, entities)
  frontend/      # Console and web server declarations
src/
  backend/       # Game logic implementations
  frontend/      # Console UI (legacy), web server, and HTTP entry point
web/             # Static HTML/CSS/JS assets rendered in the browser
bin/             # Generated executable output (created after build)
modification_log.txt  # Chronological record of development changes
Makefile         # clang++ build configuration
```

The backend manages deterministic gameplay rules such as spawning envelopes, resolving collisions, and tracking stats. The web frontend polls the engine state, renders an interactive grid, and issues move/reset commands via HTTP.

## Build Instructions

1. Ensure `clang++` and `make` are available in your environment.
2. From the project root run:

```bash
make
```

This produces `bin/tank_red_envelope`.

## Running the Game

After building, launch the executable via:

```bash
make run
```

The server listens on port 8080 by default. To choose another user-level port, either pass it as the first command-line argument or set `TANK_GAME_PORT`. Ports below 1024 are automatically rejected to avoid privileged binding:

```bash
./bin/tank_red_envelope 8081
# or
TANK_GAME_PORT=8082 ./bin/tank_red_envelope
```

Open a browser at `http://localhost:<port>` to play. Use the on-screen buttons or keyboard (W/A/S/D or arrow keys) to steer the tank. The interface displays remaining time, collected count, and total value in real time. Press “重新开始” to reseed the game.

## Cleaning

To remove compiled objects and the executable:

```bash
make clean
```
