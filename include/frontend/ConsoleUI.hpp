// File: ConsoleUI.hpp
// Description: Declares the console-based frontend controller for user I/O.

#pragma once

#include "backend/GameEngine.hpp"

#include <string>

namespace frontend {

class ConsoleUI {
public:
    explicit ConsoleUI(backend::GameEngine& engine);

    void run();

private:
    backend::GameEngine& m_engine;

    void render() const;
    backend::MoveDirection parseCommand(char command) const;
};

}  // namespace frontend
