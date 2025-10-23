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
- 红包根据面额分为小（圆形）、中（菱形）与大（六边形）三种形态，便于快速识别；网格仅在图形中央显示金额数值。
- 坦克拾取体积通过虚线圈实时标识，同时 HUD 卡片提醒其作用范围。
- “暂停/继续”按钮可随时冻结或恢复计时，暂停时方向键与红包雨操作会被禁止。
- 重新开始会清零红包数量与金额统计，并重新生成避开坦克触发范围的红包。
- 左下角小鸭助手提供隐藏菜单：点击图标弹出对话框，输入命令可触发红包雨福利或其他响应。

## Logging

The service writes timestamped logs to `logs/server.log` while mirroring the same content to the console. The file is truncated on each launch to simplify debugging sessions.

## Static Assets

Static files placed under `web/` 或 `static/` 可通过 `/static/<file>` 访问，例如 README 中展示的小鸭插图 `static/duck.png`。服务器在 `web/` 未找到资源时会自动回退到项目根目录查找。

## Modification History Snapshot

长期演进的详细记录见 `modification_log.txt`，以下为关键里程碑摘录：

- **2025-10-16**：搭建基础框架、实现坦克与红包后端逻辑，并构建最初的控制台界面。
- **2025-10-16**：上线 WebServer 与首版网页前端，实现红包随机生成、统计与控制台分离。
- **2025-10-23**：移除旧版控制台，仅保留 HTML 前端；加入红包雨动画和更精细的网格渲染。
- **2025-10-23**：增强日志与随机机制、提供暂停功能、重置逻辑、防止红包生成于坦克范围内。
- **2025-10-23**：加入小鸭助手入口、命令面板及静态资源托管，完善交互与可视化效果。

## Cleaning

To remove compiled objects and the executable:

```bash
make clean
```
