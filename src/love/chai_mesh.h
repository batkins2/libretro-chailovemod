#define __HAVE_CHAI_MESH__
#include "gfx.h"
#include "Image.h"
#include "filesystem/FileData.h"
#include "filesystem.h"

#ifdef __HAVE_CHAISCRIPT__
#include <chaiscript/chaiscript.hpp>
#endif


namespace love {
class chai_gfx;
class chai_mesh {
    public:
    chai_mesh();
    chai_mesh(const chai_mesh &c);
    ~chai_mesh();
    chai_mesh *clone() const;
    chai_mesh& operator=(const chai_mesh& m) {
		return *this;
	};
    bool newMesh(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type);
    bool wrap_setTexture(const std::string &texture);
    love::gfx::Graphics *instance;
    gfx::Mesh *mesh;
    gfx::Texture *tex;
    std::vector<imagemod::ImageData *> image;
    Image *img;
    std::vector<gfx::Texture::Slices *>slices;
    std::vector<void *> buf;
    chai_gfx *chaigfx;
    std::vector<gfx::Buffer::DataDeclaration> vf;
};
}