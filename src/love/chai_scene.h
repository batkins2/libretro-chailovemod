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
#include "vulkan/Graphics.h"
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
    void setShader(chai_shader *shader, chai_shader *compute);
    void setMatrix(const std::vector<float> &matrix, int index);
    void finalize();    
    void drawMeshes(bool shadows, int view);
    void draw(const glm::mat4 &viewMatrix1, const glm::mat4 &viewMatrix2, const glm::mat4 &viewMatrix3, const glm::mat4 &viewMatrix4, int viewCount);
    void prepareScreen();
    void update(float dt);
    void initFramebuffer();
    float getFramerate();
    float getAverageFramerate();
    float getMinFramerate();
    float getMaxFramerate();
    void setFPSUpdateInterval(float interval) { m_fpsUpdateInterval = interval; }

private:
    std::vector<chai_mesh *> meshes;
    std::map<int, std::vector<chai_mesh *>> meshChildren;
    std::vector<chai_particles *> particleSystems;
    chai_shader *sceneShader = nullptr;
    chai_shader *computeShader = nullptr;
    std::vector<Matrix4> matrices;
    float currentTime = 0.01f;
    float deltaTime = 0.01f;
    std::vector<glm::mat4> viewMatrix;
    // GLuint shadowMapFBO = 0;
    // GLuint shadowMap = 0;
    love::gfx::Texture *background_tex = nullptr;
    bool frameOddEven = false;
    int skip = 0;
    // GLuint sceneFramebuffer = 0;
    // GLuint sceneColorTexture = 0;
    // GLuint sceneDepthTexture = 0;
    bool framebufferInitialized = false;
    std::map<int, int> meshGroups;
    // Cached vectors to avoid repeated allocations
    std::vector<glm::vec3> m_lightDirectionCache;
    std::vector<glm::vec3> m_lightColorCache;
    std::vector<glm::vec3> m_ambientColorCache;
    std::vector<glm::vec3> m_intensityCache;
    std::vector<glm::mat4> m_projectionMatrixCache;
    std::vector<glm::mat4> m_lightSpaceMatrixCache;
    std::vector<glm::mat4> m_vmCache;
    std::vector<glm::mat4> m_modelMatrixCache;
    std::vector<glm::vec3> m_lightIntensityCache;
    std::vector<glm::vec3> m_ambientColorCache2;
    std::vector<glm::mat4> m_projectionMatrixCache2;
    std::vector<glm::mat4> m_viewMatrixCache;
    std::vector<glm::mat4> m_shadowCache;
    std::vector<glm::mat4> m_viewMatrixCache2;
    std::vector<glm::mat4> m_jointInfoCache;
    std::vector<glm::vec4> m_jointInfoVec4Cache;
    std::vector<glm::mat4> m_projectionMatrixBoxedCache;
    std::vector<glm::mat4> m_viewMatrixBoxedCache;
    // Deferred rendering indices
    std::vector<int> m_deferredChildIndices;
    std::vector<int> m_deferredParentIndices;
    std::vector<int> m_deferredMeshIndices;
    std::vector<std::pair<int, int>> m_drawnPairs;
    // Framerate tracking
    float m_frameTime = 0.0f;
    float m_fps = 0.0f;
    int m_frameCount = 0;
    float m_fpsUpdateTime = 0.0f;
    float m_fpsUpdateInterval = 1.0f; // Update FPS every second
    std::vector<float> m_frameTimes;
    size_t m_maxFrameHistory = 60; // Keep 60 frames of history
};
} // namespace love