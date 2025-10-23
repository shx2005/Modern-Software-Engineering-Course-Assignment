// File: ConsoleUI.cpp
// Description: Implements the console renderer and command handling loop.

#include "frontend/ConsoleUI.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace frontend {

ConsoleUI::ConsoleUI(backend::GameEngine& engine) : m_engine(engine) {}

void ConsoleUI::run() {
    std::cout << "=== 坦克红包挑战 ===\n";
    std::cout << "使用 W/A/S/D 控制坦克，Q 退出游戏。\n";

    while (true) {
        render();

        if (m_engine.isTimeUp()) {
            break;
        }

        std::cout << "输入指令 (W/A/S/D 或 Q 退出): ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        const char command = static_cast<char>(std::toupper(static_cast<unsigned char>(line.front())));
        if (command == 'Q') {
            std::cout << "玩家选择退出游戏。\n";
            break;
        }

        const backend::MoveDirection direction = parseCommand(command);
        if (direction == backend::MoveDirection::None) {
            std::cout << "无法识别的指令，请输入 W/A/S/D 或 Q。\n";
            continue;
        }

        const bool moved = m_engine.moveTank(direction);
        if (!moved) {
            std::cout << "移动失败：可能碰到边界或时间已到。\n";
        }
    }

    render();

    const backend::CollectionStats stats = m_engine.getStats();
    std::cout << "\n=== 游戏结束 ===\n";
    std::cout << "总共拾取红包数量: " << stats.collectedCount << "\n";
    std::cout << "累计金额: " << stats.collectedValue << "\n";
    std::cout << "感谢游玩！\n";
}

void ConsoleUI::render() const {
    const backend::GameConfig& config = m_engine.getConfig();
    std::vector<std::string> buffer(static_cast<std::size_t>(config.worldHeight),
                                    std::string(static_cast<std::size_t>(config.worldWidth), '.'));

    for (const backend::RedEnvelope& envelope : m_engine.getEnvelopes()) {
        const backend::Position pos = envelope.getPosition();
        if (pos.x < 0 || pos.x >= config.worldWidth || pos.y < 0 || pos.y >= config.worldHeight) {
            continue;
        }
        char symbol = 's';
        switch (envelope.getSize()) {
            case backend::EnvelopeSize::Small:
                symbol = 's';
                break;
            case backend::EnvelopeSize::Medium:
                symbol = 'm';
                break;
            case backend::EnvelopeSize::Large:
                symbol = 'L';
                break;
        }
        buffer[static_cast<std::size_t>(pos.y)][static_cast<std::size_t>(pos.x)] = symbol;
    }

    const backend::Position tankPos = m_engine.getTank().getPosition();
    if (tankPos.x >= 0 && tankPos.x < config.worldWidth && tankPos.y >= 0 && tankPos.y < config.worldHeight) {
        buffer[static_cast<std::size_t>(tankPos.y)][static_cast<std::size_t>(tankPos.x)] = 'T';
    }

    const double timeLeft =
        std::max(0.0, static_cast<double>(config.timeLimitSeconds) - m_engine.elapsedSeconds());
    const backend::CollectionStats stats = m_engine.getStats();

    std::cout << "\n剩余时间: " << std::fixed << std::setprecision(1) << timeLeft << " 秒\n";
    std::cout << "已拾取红包: " << stats.collectedCount << " 个, 金额合计: " << stats.collectedValue << "\n";
    std::cout << "地图 (" << config.worldWidth << " x " << config.worldHeight << "):\n";
    for (const std::string& row : buffer) {
        std::cout << row << "\n";
    }
    std::cout << "\n";
}

backend::MoveDirection ConsoleUI::parseCommand(char command) const {
    switch (command) {
        case 'W':
            return backend::MoveDirection::Up;
        case 'A':
            return backend::MoveDirection::Left;
        case 'S':
            return backend::MoveDirection::Down;
        case 'D':
            return backend::MoveDirection::Right;
        default:
            return backend::MoveDirection::None;
    }
}

}  // namespace frontend
