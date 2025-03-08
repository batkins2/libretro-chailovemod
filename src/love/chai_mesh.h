#define __HAVE_CHAI_MESH__

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

namespace love {
class chai_gfx;
class chai_mesh {
    public:
    chai_mesh();
    chai_mesh(const chai_mesh &c);
    ~chai_mesh();
    void destroy() {
        if (mesh != nullptr) {
            delete mesh;
            mesh = nullptr;
        }
        for (auto i : meshes) {
            if (i != nullptr) {
                delete i;
            }
        }
        for (auto i : textures) {
            if (i != nullptr) {
                i->~Drawable();
            }
        }
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
    }
    chai_mesh *clone() const;
    chai_mesh& operator=(const chai_mesh& m) {
		return *this;
	};
    std::map<std::string, std::vector<float>> getCameraParams();
    bool newMesh(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type);
    bool newMeshFromFile(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::string *FileName, const std::string &type);
    bool wrap_setTexture(const std::string &texture);
    void playAnimation(const std::string &name, const bool loop);
    void endAnimation(const std::string &name);
    void draw(love::gfx::Graphics *gfx, const Matrix4 &m, chai_shader *shader, float dt);
    love::gfx::Graphics *instance;
    gfx::Mesh *mesh = nullptr;
    std::vector<gfx::Mesh *> meshes;
    std::vector<Matrix4> matrices;
    gfx::Texture *tex = nullptr;
    // std::vector<imagemod::ImageData *> image;
    Image *img = nullptr;
    std::vector<gfx::Texture::Slices *>slices;
    std::vector<void *> buf;
    chai_gfx *chaigfx;
    std::vector<gfx::Texture *> textures;
    std::vector<gfx::Buffer::DataDeclaration> vf;
    std::map<std::string, std::vector<float>> cameraParams;
    std::map<std::string, std::vector<float>> lightParams;
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
    // float jointMinValue;
};
}