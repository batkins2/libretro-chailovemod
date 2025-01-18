// #include <vector>
// #ifndef __HAVE_CHAI_MESH__
// #include "chai_mesh.h"
// #endif
// #include "gfx.h" // Assuming you have a Graphics class for rendering

namespace love 
{
class SceneMesh {
public:
    SceneMesh();
    ~SceneMesh();
    // void addMesh(chai_mesh *mesh);
    void finalize();
    // void draw(gfx::Graphics *gfx, const Matrix4 &m);

private:
    // std::vector<chai_mesh *> meshes;
};
} // namespace love