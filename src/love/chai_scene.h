#define __HAVE_CHAI_SCENE__
#ifndef __HAVE_CHAI_MESH__
#include "chai_mesh.h"
#endif
#ifndef __HAVE_CHAI_SHADER__
#include "chai_shader.h"
#endif
#ifndef __HAVE_CHAI_PARTICLES__
#include "chai_particles.h"
#endif
#include "opengl/Graphics.h"
// #include "gfx.h" // Assuming you have a Graphics class for rendering

namespace love 
{
class chai_scene {
public:
    chai_scene();
    ~chai_scene();    
    chai_scene *clone() const;
    chai_scene& operator=(const chai_scene& m) {
		return *this;
	};
    chai_scene *newScene() const;
    bool destroy();
    void addMesh(chai_mesh *mesh);
    void addChildMesh(chai_mesh *pmesh, chai_mesh *cmesh);
    void removeChildMesh(chai_mesh *pmesh, chai_mesh *cmesh);
    void addParticleSystem(chai_particles *ps);
    void hideMesh(chai_mesh *mesh);
    void showMesh(chai_mesh *mesh);
    void setShader(chai_shader *shader);
    void setMatrix(std::vector<chaiscript::Boxed_Value> matrix, int index);
    void finalize();    
    void drawMeshes(bool shadows, int view);
    void draw(std::vector<chaiscript::Boxed_Value> viewMatrix1, std::vector<chaiscript::Boxed_Value> viewMatrix2, std::vector<chaiscript::Boxed_Value> viewMatrix3, std::vector<chaiscript::Boxed_Value> viewMatrix4, int viewCount);
    void prepareScreen();
    void update(float dt);

private:
    std::vector<chai_mesh *> meshes;
    std::map<int, std::vector<chai_mesh *>> meshChildren;
    std::vector<chai_particles *> particleSystems;
    chai_shader *sceneShader = nullptr;
    std::vector<Matrix4> matrices;
    float currentTime = 0.01f;
    float deltaTime = 0.01f;
    std::vector<chaiscript::Boxed_Value> viewMatrix;
    GLuint shadowMapFBO = 0;
    GLuint shadowMap = 0;
    love::gfx::Texture *background_tex = nullptr;
    bool frameOddEven = false;
};
} // namespace love