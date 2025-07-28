#include "../ChaiLove.h"
namespace love
{
chai_editor::chai_editor() {
    // Constructor implementation
}
chai_editor::~chai_editor() {
    // Destructor implementation
}
void chai_editor::spawn(const std::string &filename) {
    
}
void chai_editor::toggleEditMode() {
    editMode = !editMode;
}
bool chai_editor::isEditMode() {
    return editMode;
}
} // namespace love