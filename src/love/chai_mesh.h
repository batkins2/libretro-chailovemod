#define __HAVE_CHAI_MESH__

#ifndef __HAVE_CHAI_MESH_DATA__
#include "chai_meshData.h"
#endif
#ifndef __HAVE_CHAI_DEBUG__
#include "chai_debug.h"
#endif
#include "Image.h"
#include "filesystem/FileData.h"
#include "filesystem.h"
#ifndef __HAVE_CHAI_SHADER__
#include "chai_shader.h"
#include "gfx.h"
#endif

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif

#include <glm/glm.hpp>

#include <random>

#include "vulkan/Graphics.h"
namespace love {
class chai_gfx;
class chai_mesh {
    public:
    std::vector<chai_meshData*> data = std::vector<chai_meshData*>();
    chai_mesh() { 
        std::random_device rd;
        id = rd();
    };
    chai_mesh(std::vector<chai_meshData*> &data);
    chai_mesh(const chai_mesh &c);
    ~chai_mesh();
    void destroy() {
        if (mesh != nullptr) {
            delete mesh;
            mesh = nullptr;
        }
        for (auto i : meshes) {
            if (cloned) {
                i = nullptr;
            }
            if (i != nullptr) {
                delete i;
            }
        }
        meshes = std::vector<gfx::Mesh *>();
        if (!cloned) {
            for (auto i : textures) {
                i->~Drawable();
            }
        }
        textures = std::vector<gfx::Texture *>();
        if (tex != nullptr) {
            tex->~Drawable();
            tex = nullptr;
        }
        if (img != nullptr) {
            delete img;
            img = nullptr;
        }
        // for (auto i : image) {
        //     delete i;
        // }
        for (auto i : slices) {
            if (i != nullptr) {
                delete i;
            }
        }
        // cameraParams = std::map<std::string, std::vector<float>>();
    }
    bool cloned = false;
    chai_mesh *sourceInstance = nullptr;  // Points to source mesh for clones
    std::vector<chai_mesh*> instances;     // List of all instances (including self)
    chai_mesh *clone() const;
    chai_mesh& operator=(const chai_mesh& other) {
        if (this == &other) return *this;
        meshes            = other.meshes;
        nodeMatrix        = other.nodeMatrix;
        nodeParent        = other.nodeParent;
        nodeChildren      = other.nodeChildren;
        jointOrder        = other.jointOrder;
        skins             = other.skins;
        meshToNode        = other.meshToNode;
        jointList         = other.jointList;
        jointMatrix       = other.jointMatrix;
        activeAnimations  = other.activeAnimations;
        animations        = other.animations;
        nodeNames         = other.nodeNames;
        m_cachedAnimationDurations = other.m_cachedAnimationDurations;
        matrices          = other.matrices;
        offsetMatrices    = other.offsetMatrices;
        cameraParams      = other.cameraParams;
        lightParams       = other.lightParams;
        textures          = other.textures;
        vf                = other.vf;
        visible           = other.visible;
        subVisible        = other.subVisible;
        specular          = other.specular;
        normalTextures            = other.normalTextures;
        metallicRoughnessTextures = other.metallicRoughnessTextures;
        emissiveTextures          = other.emissiveTextures;
        occlusionTextures         = other.occlusionTextures;
        materialPropsList         = other.materialPropsList;
        currentTime = other.currentTime;
        mesh        = other.mesh;
        instance    = other.instance;
        tex         = other.tex;
        img         = other.img;
        slices      = other.slices;
        buf         = other.buf;
        cloned      = true;  // treat as a clone (shared texture pointers)
        return *this;
    };
    int getId() {
        return id;
    };
    std::map<std::string, std::vector<float>> getCameraParams(int index);
    std::map<std::string, std::vector<float>> getLightParams(int index);
    void setLightParams(const std::map<std::string, std::vector<float>> &params, int index);
    // bool newMesh(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type);
    chai_mesh *newMeshWithData(std::vector<chai_meshData*> &data);
    chai_mesh *newMesh();
    std::vector<chai_meshData*> loadMeshFromFile(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::string *FileName, const std::string &type);
    bool wrap_setTexture(const std::string &texture);
    void playAnimation(const std::string &name, const bool loop);
    void endAnimation(const std::string &name);
    void stopAnimations();
    bool isAnimationPlaying(const std::string &name);
    float getAnimationPercent(const std::string &name);
    void draw(love::gfx::Graphics *gfx, const Matrix4 &m, chai_shader *shader, float dt, chai_shader *computeShader, bool shadows);
    void setVisible(bool visible);
    void reloadMesh();
    // void preloadAnimations();
    void update(std::vector<float> position, std::vector<float> rotation, std::vector<float> scale, chai_debug *debug);
    float calculateAnimationDuration(const std::string &name);
    std::pair<glm::vec3, glm::vec3> getBoundingBox(const glm::mat4 &viewProjectionMatrix);
    std::vector<float> getMeshBoundingBox();
    Matrix4 getNodeMatrix(const std::string &node);
    bool isVisible() {
        return visible;
    };
    void loadSpecular(std::string texture);
    love::gfx::Graphics *instance;
    gfx::Mesh *mesh = nullptr;
    std::vector<gfx::Mesh *> meshes;
    std::vector<Matrix4> matrices;
    std::vector<Matrix4> offsetMatrices;
    gfx::Texture *tex = nullptr;
    // std::vector<imagemod::ImageData *> image;
    Image *img = nullptr;
    std::vector<gfx::Texture::Slices *>slices;
    std::vector<void *> buf;
    chai_gfx *chaigfx;
    std::vector<gfx::Texture *> textures;
    std::vector<gfx::Buffer::DataDeclaration> vf;
    std::vector<std::map<std::string, std::vector<float>>> cameraParams;
    std::vector<std::map<std::string, std::vector<float>>> lightParams;
    std::map< // Animation
        std::string, // Name
        std::map< // Channel
            std::string,
            std::map<
                int, // Node
                std::vector< // Keyframe
                    std::pair< // Keyframe data
                        float, // Time
                        glm::vec4 // Data
                    >
                >
            >        
        >
    > animations;
    std::map<std::string, std::map<float, std::map<int, glm::mat4>>> animationFrameMatrixCache;
    std::vector<float> m_modelMatrixCacheRaw;
    std::vector<glm::mat4> m_matrixCache;
    std::vector<int> m_isSpecularCache0;  // Pre-initialized with Boxed_Value(0)
    std::vector<int> m_isSpecularCache1;  // Pre-initialized with Boxed_Value(1)
    std::vector<glm::vec4> m_jointInfoCache;
    float currentTime = 0.0f;
    std::vector<int> nodes;
    std::map<std::string, int> nodeNames;
    std::map<int, glm::mat4> nodeActiveMatrix;
    Matrix4 activeMatrix;
    // std::vector<int> meshList;
    std::map<int, std::map<int, std::vector<float>>> skins;
    std::map<int, int> meshToNode;
    std::map<std::string, std::pair<float, bool>> activeAnimations;
    std::map<int, std::vector<int>> jointOrder;
    std::map<int, std::vector<int>> nodeChildren;
    std::vector<std::pair<int, int>> nodeParent;
    std::map<int, glm::mat4> nodeParentMatrix;
    std::vector<glm::mat4> nodeMatrix;
    std::vector<std::vector<int>> jointList;
    std::vector<std::map<int, glm::mat4>> jointMatrix;
    bool visible = true;
    std::vector<bool> subVisible;    
    std::vector<bool> specular;
    int specularW = 0;
    int specularH = 0;
    uint8_t* specData = nullptr;

    // ---- Blender-level PBR material data ----------------------------------------
    struct PBRMaterialProps {
        float metallicFactor    = 0.0f;   // glTF pbrMetallicRoughness.metallicFactor
        float roughnessFactor   = 1.0f;   // glTF pbrMetallicRoughness.roughnessFactor
        float normalScale       = 1.0f;   // glTF normalTexture.scale
        float occlusionStrength = 1.0f;   // glTF occlusionTexture.strength
        glm::vec3 emissiveFactor = glm::vec3(0.0f); // glTF emissiveFactor [r,g,b]
        float alphaCutoff       = 0.5f;   // glTF alphaCutoff (MASK mode)
        bool hasNormalMap            = false;
        bool hasMetallicRoughnessMap = false;
        bool hasEmissiveMap          = false;
        bool hasOcclusionMap         = false;
        bool doubleSided             = false;
        bool alphaBlend              = false; // true when alphaMode == "BLEND"
        bool alphaMask               = false; // true when alphaMode == "MASK"
    };
    // Per-submesh additional PBR texture maps (indexed same as meshes[])
    std::vector<gfx::Texture*> normalTextures;            // tangent-space normal map
    std::vector<gfx::Texture*> metallicRoughnessTextures; // glTF ORM: R=occlusion(opt), G=roughness, B=metallic
    std::vector<gfx::Texture*> emissiveTextures;          // emissive / glow map
    std::vector<gfx::Texture*> occlusionTextures;         // ambient occlusion map
    std::vector<PBRMaterialProps> materialPropsList;       // scalar factors per submesh
    // ---- end PBR ----------------------------------------------------------------
    // GLuint specularMap = 0;
    std::map<std::string, float> m_cachedAnimationDurations;
    // std::map<int, GLuint> cachedVBOs;
    std::map<int, size_t> vboSizes;
    protected:
    int id;
    int frameCount = 0;
    // float jointMinValue;
};
}