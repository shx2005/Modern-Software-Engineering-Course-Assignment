// File: GameEngine.cpp
// Description: Implements game loop coordination, envelope spawning, and timing.

#include "backend/GameEngine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace backend {

namespace {

EnvelopeSize pickRandomSize(std::mt19937& rng) {
    static std::discrete_distribution<int> distribution{50, 35, 15};
    const int index = distribution(rng);
    switch (index) {
        case 0:
            return EnvelopeSize::Small;
        case 1:
            return EnvelopeSize::Medium;
        default:
            return EnvelopeSize::Large;
    }
}

int radiusForSize(EnvelopeSize size) {
    switch (size) {
        case EnvelopeSize::Small:
            return 1;
        case EnvelopeSize::Medium:
            return 2;
        case EnvelopeSize::Large:
            return 3;
    }
    return 1;
}

int randomValueForSize(EnvelopeSize size, std::mt19937& rng) {
    switch (size) {
        case EnvelopeSize::Small: {
            std::uniform_int_distribution<int> valueRange(5, 20);
            return valueRange(rng);
        }
        case EnvelopeSize::Medium: {
            std::uniform_int_distribution<int> valueRange(21, 60);
            return valueRange(rng);
        }
        case EnvelopeSize::Large: {
            std::uniform_int_distribution<int> valueRange(61, 120);
            return valueRange(rng);
        }
    }
    return 0;
}

}  // namespace

GameEngine::GameEngine(GameConfig config)
    : m_config(config),
      m_tank({config.worldWidth / 2, config.worldHeight / 2}),
      m_rng(std::random_device{}()) {
    if (config.worldWidth <= 0 || config.worldHeight <= 0) {
        throw std::invalid_argument("World dimensions must be positive.");
    }
    if (config.initialEnvelopeCount <= 0) {
        throw std::invalid_argument("At least one red envelope is required.");
    }
    if (config.timeLimitSeconds <= 0) {
        throw std::invalid_argument("Time limit must be positive.");
    }

    reset();
}

void GameEngine::reset() {
    m_stats = {};
    m_envelopes.clear();
    m_envelopes.reserve(static_cast<std::size_t>(m_config.initialEnvelopeCount));
    m_tank.setPosition({m_config.worldWidth / 2, m_config.worldHeight / 2});
    m_nextEnvelopeId = 0;

    for (int i = 0; i < m_config.initialEnvelopeCount; ++i) {
        m_envelopes.emplace_back(createRandomEnvelope(m_nextEnvelopeId++));
    }

    m_startTime = std::chrono::steady_clock::now();
    handleCollisions();  // Ensure starting position collects envelopes if any overlap.
}

bool GameEngine::moveTank(MoveDirection direction) {
    if (isTimeUp()) {
        return false;
    }

    const bool moved = m_tank.move(direction, m_config.worldWidth, m_config.worldHeight);
    handleCollisions();
    return moved;
}

bool GameEngine::isTimeUp() const {
    return elapsedSeconds() >= static_cast<double>(m_config.timeLimitSeconds);
}

double GameEngine::elapsedSeconds() const {
    if (m_startTime == std::chrono::steady_clock::time_point{}) {
        return 0.0;
    }
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> diff = now - m_startTime;
    return diff.count();
}

const Tank& GameEngine::getTank() const noexcept {
    return m_tank;
}

const std::vector<RedEnvelope>& GameEngine::getEnvelopes() const noexcept {
    return m_envelopes;
}

CollectionStats GameEngine::getStats() const noexcept {
    return m_stats;
}

const GameConfig& GameEngine::getConfig() const noexcept {
    return m_config;
}

void GameEngine::setRandomSeed(unsigned int seed) {
    m_rng.seed(seed);
}

RedEnvelope GameEngine::createRandomEnvelope(std::size_t id) {
    std::uniform_int_distribution<int> widthDist(0, m_config.worldWidth - 1);
    std::uniform_int_distribution<int> heightDist(0, m_config.worldHeight - 1);

    EnvelopeSize size = pickRandomSize(m_rng);
    Position position{widthDist(m_rng), heightDist(m_rng)};

    bool foundFreeSpot = false;
    for (int attempts = 0; attempts < 100; ++attempts) {
        position = {widthDist(m_rng), heightDist(m_rng)};
        if (position.x == m_tank.getPosition().x && position.y == m_tank.getPosition().y) {
            continue;
        }

        bool conflict = false;
        for (const auto& envelope : m_envelopes) {
            const Position existing = envelope.getPosition();
            if (existing.x == position.x && existing.y == position.y) {
                conflict = true;
                break;
            }
        }

        if (!conflict) {
            foundFreeSpot = true;
            break;
        }
    }

    if (!foundFreeSpot) {
        position = {widthDist(m_rng), heightDist(m_rng)};
    }

    return RedEnvelope(id,
                       size,
                       randomValueForSize(size, m_rng),
                       position,
                       radiusForSize(size));
}

void GameEngine::respawnEnvelope(std::size_t index) {
    if (index >= m_envelopes.size()) {
        return;
    }

    // Temporarily move the envelope outside the world to avoid conflicting with itself.
    m_envelopes[index].setPosition({-10, -10});
    m_envelopes[index] = createRandomEnvelope(m_nextEnvelopeId++);
}

void GameEngine::handleCollisions() {
    for (std::size_t i = 0; i < m_envelopes.size(); ++i) {
        if (isColliding(m_tank, m_envelopes[i])) {
            m_stats.collectedCount += 1;
            m_stats.collectedValue += m_envelopes[i].getValue();
            respawnEnvelope(i);
        }
    }
}

}  // namespace backend
