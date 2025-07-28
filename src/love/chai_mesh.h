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

#include "opengl/Graphics.h"
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
    chai_mesh *clone() const;
    chai_mesh& operator=(const chai_mesh& m) {
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
    void draw(love::gfx::Graphics *gfx, const Matrix4 &m, chai_shader *shader, float dt);
    void setVisible(bool visible);
    void reloadMesh();
    void update(std::vector<float> position, std::vector<float> rotation, std::vector<float> scale, chai_debug *debug);
    std::pair<glm::vec3, glm::vec3> getBoundingBox(const glm::mat4 &viewProjectionMatrix);
    std::vector<float> getMeshBoundingBox();
    bool isVisible() {
        return visible;
    };
    void loadSpecular();
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
    float currentTime = 0.0f;
    std::vector<int> nodes;
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
    GLuint specularMap = 0;
    protected:
    int id;
    // float jointMinValue;
};
}