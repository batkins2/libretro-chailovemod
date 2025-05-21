#define __HAVE_CHAI_MESH_DATA__
#include <vector>

#include <map>

#include <string>

#include "glm/glm.hpp"

namespace love 
{
class chai_meshData {
public:
    chai_meshData() {
        prepD = std::vector<uint32_t>();
    };
    chai_meshData(std::vector<uint32_t> &prepD, std::map<std::string, std::map<std::string, std::map<int, std::vector<std::pair<float, glm::vec4>>>>> &animations) {
        this->anims = animations;
        this->prepD = prepD;
    };
    ~chai_meshData() {
        prepD.clear();
    };
    chai_meshData(const chai_meshData &c) {
        this->prepD = c.prepD;
    };
    chai_meshData& operator=(const chai_meshData &c) {
        this->prepD = c.prepD;
        return *this;
    };
    chai_meshData *clone() const {
        return new chai_meshData(*this);
    };
    std::vector<uint32_t> prepD;
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
    > anims;
};
}