#pragma once

#include "engine/core/config.h"
#include <string>

namespace engine::ui {

class ContentBrowserPanel {
public:
    ContentBrowserPanel();
    void render();

private:
    std::string m_current_virtual_dir{"/assets"};
};

} // namespace engine::ui
