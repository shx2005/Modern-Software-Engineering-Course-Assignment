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
    double getExactX() const noexcept;
    double getExactY() const noexcept;
    int getMoveStep() const noexcept;

    bool move(MoveDirection direction, int worldWidth, int worldHeight) noexcept;
    void setPosition(Position newPosition) noexcept;

private:
    Position m_position;
    int m_moveStep;
    double m_exactX;
    double m_exactY;
    double m_currentMomentum{0.0};
    double m_momentumIncrement{0.0};
    double m_momentumDecay{0.0};
    double m_momentumMax{0.0};
    MoveDirection m_lastDirection{MoveDirection::None};
};

bool isColliding(const Tank& tank, const RedEnvelope& envelope) noexcept;

}  // namespace backend
