// File: Tank.cpp
// Description: Implements tank movement handling and collision helper routines.

#include "backend/Tank.hpp"

#include <algorithm>
#include <cmath>

namespace backend {

Tank::Tank(Position startPosition, int moveStep)
    : m_position(startPosition),
      m_moveStep(std::max(1, moveStep)),
      m_exactX(static_cast<double>(startPosition.x)),
      m_exactY(static_cast<double>(startPosition.y)) {
    const double base = static_cast<double>(m_moveStep);
    m_momentumIncrement = base * 0.45;
    m_momentumDecay = base * 0.6;
    m_momentumMax = base * 3.5;
}

Position Tank::getPosition() const noexcept {
    return m_position;
}

double Tank::getExactX() const noexcept {
    return m_exactX;
}

double Tank::getExactY() const noexcept {
    return m_exactY;
}

int Tank::getMoveStep() const noexcept {
    return m_moveStep;
}

bool Tank::move(MoveDirection direction, int worldWidth, int worldHeight) noexcept {
    if (direction == MoveDirection::None) {
        m_currentMomentum = 0.0;
        m_lastDirection = MoveDirection::None;
        return false;
    }

    if (m_lastDirection == direction) {
        m_currentMomentum = std::min(m_currentMomentum + m_momentumIncrement, m_momentumMax);
    } else if (m_currentMomentum > 0.0) {
        m_currentMomentum = std::max(0.0, m_currentMomentum - m_momentumDecay);
    }
    const double delta = static_cast<double>(m_moveStep) + m_currentMomentum;

    double nextX = m_exactX;
    double nextY = m_exactY;
    switch (direction) {
        case MoveDirection::Up:
            nextY -= delta;
            break;
        case MoveDirection::Down:
            nextY += delta;
            break;
        case MoveDirection::Left:
            nextX -= delta;
            break;
        case MoveDirection::Right:
            nextX += delta;
            break;
        case MoveDirection::None:
            break;
    }

    const double minX = 0.0;
    const double minY = 0.0;
    const double maxX = std::max(0, worldWidth - 1);
    const double maxY = std::max(0, worldHeight - 1);

    bool clamped = false;
    if (nextX < minX) {
        nextX = minX;
        clamped = true;
    } else if (nextX > maxX) {
        nextX = maxX;
        clamped = true;
    }

    if (nextY < minY) {
        nextY = minY;
        clamped = true;
    } else if (nextY > maxY) {
        nextY = maxY;
        clamped = true;
    }

    if (clamped) {
        m_currentMomentum = 0.0;
    }

    const Position previous = m_position;
    m_exactX = nextX;
    m_exactY = nextY;
    m_position = {static_cast<int>(std::round(nextX)),
                  static_cast<int>(std::round(nextY))};
    m_lastDirection = direction;

    return m_position.x != previous.x || m_position.y != previous.y;
}

void Tank::setPosition(Position newPosition) noexcept {
    m_position = newPosition;
    m_exactX = static_cast<double>(newPosition.x);
    m_exactY = static_cast<double>(newPosition.y);
    m_currentMomentum = 0.0;
    m_lastDirection = MoveDirection::None;
}

bool isColliding(const Tank& tank, const RedEnvelope& envelope) noexcept {
    const Position tankPos = tank.getPosition();
    const Position envelopePos = envelope.getPosition();
    const int dx = tankPos.x - envelopePos.x;
    const int dy = tankPos.y - envelopePos.y;
    const int radius = std::max(0, envelope.getCollectionRadius());

    const int distanceSquared = dx * dx + dy * dy;
    return distanceSquared <= radius * radius;
}

}  // namespace backend
