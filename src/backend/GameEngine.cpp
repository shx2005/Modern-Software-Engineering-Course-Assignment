// File: GameEngine.cpp
// Description: Implements game loop coordination, envelope spawning, and timing.

#include "backend/GameEngine.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
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
    std::cout << "GameEngine::reset start" << std::endl;
    m_stats = {};
    m_envelopes.clear();
    m_envelopes.reserve(static_cast<std::size_t>(m_config.initialEnvelopeCount));
    m_tank.setPosition({m_config.worldWidth / 2, m_config.worldHeight / 2});
    m_nextEnvelopeId = 0;
    m_paused = false;
    m_pausedAccumulated = std::chrono::duration<double>{0.0};
    m_pauseStart = {};

    for (int i = 0; i < m_config.initialEnvelopeCount; ++i) {
        std::cout << "Creating envelope " << i << std::endl;
        m_envelopes.emplace_back(createRandomEnvelope(m_nextEnvelopeId++));
    }

    m_startTime = std::chrono::steady_clock::now();
    std::cout << "Before handleCollisions" << std::endl;
    const double exactX = m_tank.getExactX();
    const double exactY = m_tank.getExactY();
    handleCollisions(exactX, exactY);  // Ensure starting position collects envelopes if any overlap.
    std::cout << "GameEngine::reset end" << std::endl;
}

bool GameEngine::moveTank(MoveDirection direction) {
    if (isTimeUp() || m_paused) {
        return false;
    }

    const double previousX = m_tank.getExactX();
    const double previousY = m_tank.getExactY();
    const bool moved = m_tank.move(direction, m_config.worldWidth, m_config.worldHeight);
    handleCollisions(previousX, previousY);
    return moved;
}

bool GameEngine::isTimeUp() const {
    return elapsedSeconds() >= static_cast<double>(m_config.timeLimitSeconds);
}

double GameEngine::elapsedSeconds() const {
    if (m_startTime == std::chrono::steady_clock::time_point{}) {
        return 0.0;
    }
    auto now = std::chrono::steady_clock::now();
    if (m_paused && m_pauseStart != std::chrono::steady_clock::time_point{}) {
        now = m_pauseStart;
    }
    const std::chrono::duration<double> diff = now - m_startTime;
    const double pausedSeconds = m_pausedAccumulated.count();
    return std::max(0.0, diff.count() - pausedSeconds);
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

int GameEngine::spawnBonusEnvelopes(int minCount, int maxCount) {
    if (minCount <= 0) {
        minCount = 1;
    }
    if (maxCount < minCount) {
        maxCount = minCount;
    }

    std::uniform_int_distribution<int> countDist(minCount, maxCount);
    const int spawnCount = countDist(m_rng);
    for (int i = 0; i < spawnCount; ++i) {
        m_envelopes.emplace_back(createRandomEnvelope(m_nextEnvelopeId++));
    }

    return spawnCount;
}

void GameEngine::pause() {
    if (m_paused) {
        return;
    }
    m_paused = true;
    m_pauseStart = std::chrono::steady_clock::now();
}

void GameEngine::resume() {
    if (!m_paused) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (m_pauseStart != std::chrono::steady_clock::time_point{}) {
        m_pausedAccumulated += now - m_pauseStart;
    }
    m_paused = false;
    m_pauseStart = {};
}

bool GameEngine::togglePause() {
    if (m_paused) {
        resume();
    } else {
        pause();
    }
    return m_paused;
}

bool GameEngine::isPaused() const noexcept {
    return m_paused;
}

RedEnvelope GameEngine::createRandomEnvelope(std::size_t id) {
    std::uniform_int_distribution<int> widthDist(0, m_config.worldWidth - 1);
    std::uniform_int_distribution<int> heightDist(0, m_config.worldHeight - 1);

    EnvelopeSize size = pickRandomSize(m_rng);
    Position position{widthDist(m_rng), heightDist(m_rng)};
    const Position tankPos = m_tank.getPosition();

    bool foundFreeSpot = false;
    for (int attempts = 0; attempts < 150; ++attempts) {
        position = {widthDist(m_rng), heightDist(m_rng)};
        const int radius = radiusForSize(size);
        const int dxTank = position.x - tankPos.x;
        const int dyTank = position.y - tankPos.y;
        if (dxTank * dxTank + dyTank * dyTank <= radius * radius) {
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

void GameEngine::handleCollisions(double previousX, double previousY) {
    const double currentX = m_tank.getExactX();
    const double currentY = m_tank.getExactY();
    for (std::size_t i = 0; i < m_envelopes.size(); ++i) {
        const bool collidedAtPosition = isColliding(m_tank, m_envelopes[i]);
        const bool crossedDuringMove =
            intersectsMovementPath(previousX, previousY, currentX, currentY, m_envelopes[i]);
        if (collidedAtPosition || crossedDuringMove) {
            m_stats.collectedCount += 1;
            m_stats.collectedValue += m_envelopes[i].getValue();
            respawnEnvelope(i);
        }
    }
}

bool GameEngine::intersectsMovementPath(double startX,
                                        double startY,
                                        double endX,
                                        double endY,
                                        const RedEnvelope& envelope) const noexcept {
    const int radius = std::max(0, envelope.getCollectionRadius());
    if (radius == 0) {
        return false;
    }

    const double dx = endX - startX;
    const double dy = endY - startY;

    if (dx == 0.0 && dy == 0.0) {
        const Position center = envelope.getPosition();
        const double fx = startX - static_cast<double>(center.x);
        const double fy = startY - static_cast<double>(center.y);
        return (fx * fx + fy * fy) <= static_cast<double>(radius * radius);
    }

    const Position center = envelope.getPosition();
    const double fx = startX - static_cast<double>(center.x);
    const double fy = startY - static_cast<double>(center.y);

    const double a = dx * dx + dy * dy;
    const double b = 2.0 * (fx * dx + fy * dy);
    const double c = fx * fx + fy * fy - static_cast<double>(radius * radius);
    double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) {
        return false;
    }
    discriminant = std::sqrt(discriminant);
    const double invDenominator = 1.0 / (2.0 * a);
    const double t1 = (-b - discriminant) * invDenominator;
    const double t2 = (-b + discriminant) * invDenominator;
    return (t1 >= 0.0 && t1 <= 1.0) || (t2 >= 0.0 && t2 <= 1.0);
}

}  // namespace backend
