// File: LayoutManager.hpp
// Description: Declares a global layout manager used to orchestrate and
//              persist user-specific personalization settings.

#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace frontend {

struct UserLayoutPreferences {
    std::string theme;
    std::string layoutPreset;
    bool compactMode{false};
};

class LayoutManager {
public:
    LayoutManager();
    ~LayoutManager();

    void initialize();
    void applyPreferences(const std::string& userId, const UserLayoutPreferences& preferences);
    UserLayoutPreferences getPreferences(const std::string& userId) const;
    std::string exportPreferences(const std::string& userId) const;
    void persist();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, UserLayoutPreferences> m_userPreferences;
};

}  // namespace frontend

