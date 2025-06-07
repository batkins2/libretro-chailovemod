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

    void setParticleSystem(const std::string &texture, int size);
    void updateParticleSystem(float dt);
    void setParameter(const std::string &name, const std::vector<float> values);
    void draw(float x, float y, float z, float angle = 0.0f, float scaleX = 1.0f, float scaleY = 1.0f, float scaleZ = 1.0f);

    gfx::ParticleSystem *ps = nullptr;
    std::string texName = "";
    gfx::Texture *tex = nullptr;
    int sz = 0;
};
}