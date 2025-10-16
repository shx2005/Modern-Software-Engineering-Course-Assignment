// File: RedEnvelope.cpp
// Description: Implements the red envelope data object accessors.

#include "backend/RedEnvelope.hpp"

namespace backend {

RedEnvelope::RedEnvelope(std::size_t id,
                         EnvelopeSize size,
                         int value,
                         Position position,
                         int collectionRadius)
    : m_id(id),
      m_size(size),
      m_value(value),
      m_position(position),
      m_collectionRadius(collectionRadius) {}

std::size_t RedEnvelope::getId() const noexcept {
    return m_id;
}

EnvelopeSize RedEnvelope::getSize() const noexcept {
    return m_size;
}

int RedEnvelope::getValue() const noexcept {
    return m_value;
}

Position RedEnvelope::getPosition() const noexcept {
    return m_position;
}

int RedEnvelope::getCollectionRadius() const noexcept {
    return m_collectionRadius;
}

void RedEnvelope::setPosition(Position newPosition) noexcept {
    m_position = newPosition;
}

}  // namespace backend
