#define __HAVE_CHAI_EDITOR__
namespace love {
class chai_editor {
public:
    chai_editor();
    ~chai_editor();
    void spawn(const std::string &filename);
    void toggleEditMode();
    bool isEditMode();
    bool editMode = false;
};
}