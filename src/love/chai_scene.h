#define __HAVE_CHAI_SCENE__
#ifndef __HAVE_CHAI_MESH__
#include "chai_mesh.h"
#endif
#ifndef __HAVE_CHAI_SHADER__
#include "chai_shader.h"
#endif
#include "opengl/Graphics.h"
// #include "gfx.h" // Assuming you have a Graphics class for rendering

namespace love 
{
class chai_scene {
public:
    chai_scene();
    ~chai_scene();
    void addMesh(chai_mesh *mesh);
    void setShader(chai_shader *shader);
    void setMatrix(std::vector<chaiscript::Boxed_Value> matrix, int index);
    void finalize();    
    void drawMeshes(bool shadows);
    void draw();

private:
    std::vector<chai_mesh *> meshes;
    chai_shader *sceneShader;
    std::vector<Matrix4> matrices;
    float currentTime = 0.0f;
    std::vector<chaiscript::Boxed_Value> viewMatrix;
    GLuint shadowMapFBO = 0;
    GLuint shadowMap = 0;
};
} // namespace love