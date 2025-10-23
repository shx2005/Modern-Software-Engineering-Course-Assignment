# Tank Red Envelope Game

This project implements a browser-playable tank game written in modern C++ with a clear backend/frontend separation. The backend simulates the grid world, while a lightweight HTTP server exposes the state to an HTML5 interface where players drive the tank to collect randomly spawned red envelopes within a time limit.

## Project Structure

```
include/
  backend/       # Public interfaces for game logic (engine, entities)
  frontend/      # Web server declarations
src/
  backend/       # Game logic implementations
  frontend/      # Web server and HTTP entry point
web/             # Static HTML/CSS/JS assets rendered in the browser
logs/            # Runtime log files (overwritten on each execution)
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

### Gameplay HUD

- 首次进入页面时需点击“开始游戏”按钮，计时器与移动控制才会激活。
- 每个红包的面额直接绘制在方块内，拾取后实时统计到顶部 HUD。
- 倒计时结束时自动弹出本局总结，包括红包数量与总金额，并禁用移动操作。

### 互动玩法

- 控制面板左下角的“红包雨”按钮会播放红包雨动画，结束后随机新增 5～10 个红包。
- 红包根据面额分为小（圆形）、中（菱形）与大（六边形）三种形态，便于快速识别。
- 坦克拾取体积通过虚线圈实时标识，同时 HUD 卡片提醒其作用范围。

## Logging

The service writes timestamped logs to `logs/server.log` while mirroring the same content to the console. The file is truncated on each launch to simplify debugging sessions.

## Cleaning

To remove compiled objects and the executable:

```bash
make clean
```
