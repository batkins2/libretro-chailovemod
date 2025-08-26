#define __HAVE_CHAI_PARTICLES__
#include <string>
#include "gfx.h"

namespace love 
{
class chai_particles
{
public:
    chai_particles();
    ~chai_particles();  
    chai_particles *clone() const
    {
        return new chai_particles(*this);
    };
    chai_particles& operator=(const chai_particles& m)
    {
        return *this;
    };
    chai_particles *newParticles() const
    {
        return new chai_particles();
    };

    struct ParticleDetails {
        std::string name;
        std::vector<float> values;
    };

    void setParticleSystem(const std::string &texture, int size);
    void updateParticleSystem(float dt);
    void setParameter(const std::string &name, const std::vector<float> values);
    void draw();
    void setParticleDetails(float x, float y, float z, float angle, float scaleX, float scaleY, float scaleZ);
    void setParent(chai_mesh *parent, const std::string &node);

    ParticleDetails details;
    gfx::ParticleSystem *ps = nullptr;
    std::string texName = "";
    gfx::Texture *tex = nullptr;
    chai_mesh *bindParent = nullptr;
    std::string bindNode = "";
    int sz = 0;
};
}