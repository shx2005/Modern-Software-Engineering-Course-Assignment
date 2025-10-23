// File: Tank.cpp
// Description: Implements tank movement handling and collision helper routines.

#include "backend/Tank.hpp"

#include <algorithm>
#include <cmath>

namespace backend {

Tank::Tank(Position startPosition, int moveStep)
    : m_position(startPosition), m_moveStep(std::max(1, moveStep)) {}

Position Tank::getPosition() const noexcept {
    return m_position;
}

int Tank::getMoveStep() const noexcept {
    return m_moveStep;
}

bool Tank::move(MoveDirection direction, int worldWidth, int worldHeight) noexcept {
    if (direction == MoveDirection::None) {
        return false;
    }

    Position updated = m_position;
    switch (direction) {
        case MoveDirection::Up:
            updated.y -= m_moveStep;
            break;
        case MoveDirection::Down:
            updated.y += m_moveStep;
            break;
        case MoveDirection::Left:
            updated.x -= m_moveStep;
            break;
        case MoveDirection::Right:
            updated.x += m_moveStep;
            break;
        case MoveDirection::None:
            break;
    }

    if (updated.x < 0 || updated.x >= worldWidth || updated.y < 0 || updated.y >= worldHeight) {
        return false;
    }

    m_position = updated;
    return true;
}

void Tank::setPosition(Position newPosition) noexcept {
    m_position = newPosition;
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
