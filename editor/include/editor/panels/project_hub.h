#pragma once

#include "editor/core/editor_preferences.h"
#include <string>
#include <functional>

namespace editor {

enum class ProjectTemplate {
    Blank3D = 0,
    PhysicsSandbox = 1
};

class ProjectHub {
public:
    ProjectHub() = default;
    ~ProjectHub() = default;

    void render(EditorPreferences& preferences, bool* is_open, 
                std::function<void(const std::string& project_path)> on_project_selected);

    static bool create_project_from_template(const std::string& directory, 
                                            const std::string& name, 
                                            ProjectTemplate tmpl);

private:
    char m_new_project_name[128]{"MyNewGame"};
    char m_new_project_dir[256]{"projects/MyNewGame"};
    char m_open_project_path[256]{""};
    ProjectTemplate m_selected_template{ProjectTemplate::Blank3D};
    std::string m_error_message{""};
};

} // namespace editor
