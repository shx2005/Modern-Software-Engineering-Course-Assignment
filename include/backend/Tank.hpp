// File: Tank.hpp
// Description: Declares the tank entity responsible for player movement actions.

#pragma once

#include "RedEnvelope.hpp"

namespace backend {

enum class MoveDirection { Up, Down, Left, Right, None };

class Tank {
public:
    explicit Tank(Position startPosition, int moveStep = 1);

    Position getPosition() const noexcept;
    int getMoveStep() const noexcept;

    bool move(MoveDirection direction, int worldWidth, int worldHeight) noexcept;
    void setPosition(Position newPosition) noexcept;

private:
    Position m_position;
    int m_moveStep;
};

bool isColliding(const Tank& tank, const RedEnvelope& envelope) noexcept;

}  // namespace backend
