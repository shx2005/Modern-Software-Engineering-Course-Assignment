// File: RedEnvelope.hpp
// Description: Declares the red envelope data model used by the backend engine.

#pragma once

#include <cstddef>

namespace backend {

enum class EnvelopeSize { Small, Medium, Large };

struct Position {
    int x;
    int y;
};

class RedEnvelope {
public:
    RedEnvelope(std::size_t id,
                EnvelopeSize size,
                int value,
                Position position,
                int collectionRadius);

    std::size_t getId() const noexcept;
    EnvelopeSize getSize() const noexcept;
    int getValue() const noexcept;
    Position getPosition() const noexcept;
    int getCollectionRadius() const noexcept;

    void setPosition(Position newPosition) noexcept;

private:
    std::size_t m_id;
    EnvelopeSize m_size;
    int m_value;
    Position m_position;
    int m_collectionRadius;
};

}  // namespace backend
