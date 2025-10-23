// File: GameEngine.hpp
// Description: Declares the core backend engine coordinating tank movement,
//              red envelope generation, and gameplay timing.

#pragma once

#include "Tank.hpp"

#include <chrono>
#include <random>
#include <vector>

namespace backend {

struct GameConfig {
    int worldWidth{40};
    int worldHeight{20};
    int initialEnvelopeCount{8};
    int timeLimitSeconds{60};
};

struct CollectionStats {
    int collectedCount{0};
    int collectedValue{0};
};

class GameEngine {
public:
    explicit GameEngine(GameConfig config = {});

    void reset();

    bool moveTank(MoveDirection direction);

    bool isTimeUp() const;
    double elapsedSeconds() const;

    const Tank& getTank() const noexcept;
    const std::vector<RedEnvelope>& getEnvelopes() const noexcept;
    CollectionStats getStats() const noexcept;
    const GameConfig& getConfig() const noexcept;

    void setRandomSeed(unsigned int seed);
    int spawnBonusEnvelopes(int minCount, int maxCount);
    void pause();
    void resume();
    bool togglePause();
    bool isPaused() const noexcept;

private:
    GameConfig m_config;
    Tank m_tank;
    std::vector<RedEnvelope> m_envelopes;
    CollectionStats m_stats;
    std::mt19937 m_rng;
    std::chrono::steady_clock::time_point m_startTime;
    std::size_t m_nextEnvelopeId{0};
    bool m_paused{false};
    std::chrono::duration<double> m_pausedAccumulated{0.0};
    std::chrono::steady_clock::time_point m_pauseStart{};

    RedEnvelope createRandomEnvelope(std::size_t id);
    void respawnEnvelope(std::size_t index);
    void handleCollisions();
};

}  // namespace backend
