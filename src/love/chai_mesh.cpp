#include "../ChaiLove.h"

namespace love
{
chai_mesh::chai_mesh() {

}

bool chai_mesh::newMesh(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type) {
    instance = inst;

    auto prepD = new std::vector<uint8_t>();

    uint8_t vc = 255;

    for (auto vectors : data) {
        for (auto value : chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(vectors)) {
            auto v = chaiscript::boxed_cast<float>(value);
            std::byte* ret = (std::byte*) std::malloc(4 * sizeof(std::byte));
            unsigned int asInt = *((int*)&v);

            for (int i = 3; i > 0; i--) {
                prepD->push_back((asInt >> 8 * i) & 0xFF);
            }

            // size_t currentSize = prepD->size();
            // prepD->resize(currentSize + sizeof(float));
            // std::memcpy(prepD->data() + currentSize, &ret, sizeof(float));
        }
        for (int i = 0; i < 4; i++) {
            // size_t currentSize = prepD->size();
            // prepD->resize(currentSize + sizeof(float));
            // std::memcpy(prepD->data() + currentSize, &vc, sizeof(uint8_t));
            prepD->push_back(vc);
        }
    }

    // std::memcpy(d, &prepD, prepD->size() * sizeof(float));

    auto vf = std::vector<gfx::Buffer::DataDeclaration>();
    for (int i = 0; i < vertexFormat.size(); i++) {
        auto t = chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(vertexFormat.at(i));
        auto temp = std::vector<std::string>();
        for (int c = 0; c < t.size(); c++) {
            auto p = chaiscript::boxed_cast<std::string>(t[c]);
            temp.push_back(p);
        }

        auto t1 = gfx::DATAFORMAT_UINT8_VEC4;
        if (temp[1] == "byte") {
            vf.push_back(gfx::Buffer::DataDeclaration(temp[0], t1, atoi(temp[2].c_str())));
        } else if (temp[1] == "float") {
            auto t2 = atoi(temp[2].c_str());
            if (t2 > 2) {
                t1 = gfx::DATAFORMAT_FLOAT_VEC3;
            } else {
                t1 = gfx::DATAFORMAT_FLOAT_VEC2;
            }
            vf.push_back(gfx::Buffer::DataDeclaration(temp[0], t1, t2));
        }
    }
    auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_STATIC;
    if (type == "triangles") {
        mesh = instance->newMesh(vf, prepD->data(), prepD->size() * sizeof(uint8_t), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
    }
    return true;
}

bool chai_mesh::wrap_setTexture(const std::string &texture) {
    img = new Image(texture);

    auto w = img->getWidth();
    auto h = img->getHeight();

    auto cl = ChaiLove::getInstance();
    // auto fs = cl->getFSModule();
    auto i = cl->getImageModule();
    // auto filesize = fs.getSize(texture);

    // buf.push_back(fs.readBuffer(texture, filesize));

    gfx::Texture::Settings settings;
    settings.width = w;
    settings.height = h;
    settings.format = PIXELFORMAT_RGBA8_UINT;

    // settings.renderTarget = true;

    auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);

    slices.push_back(new gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D));

    // slices.back()->add(0, 0, image.back());

    tex.push_back(gfx->newTexture(settings, slices.back()));

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    size_t dataSize = w*h;

    SDL_LockSurface(img->surface);
    // Assuming pixelData is a byte array containing the ARGB data.
    uint8_t* pixelData = static_cast<uint8_t*>(img->surface->pixels);
    for (size_t i = 0; i < dataSize * 4; i += 4) {
        uint8_t alpha = pixelData[i];        // ARGB - Alpha at index 0
        uint8_t blue = pixelData[i + 1];      // ARGB - Red at index 1
        uint8_t green = pixelData[i + 2];    // ARGB - Green at index 2
        uint8_t red = pixelData[i + 3];     // ARGB - Blue at index 3

        // Swap to RGBA format
        pixelData[i] = red;                 // RGBA - Red at index 0
        pixelData[i + 1] = green;           // RGBA - Green at index 1
        pixelData[i + 2] = blue;            // RGBA - Blue at index 2
        pixelData[i + 3] = alpha;           // RGBA - Alpha at index 3
    }
    tex.back()->replacePixels(pixelData, dataSize*8, 0, 0, rect, false);
    // image.push_back(i->newImageData(w, h, settings.format, img->surface->pixels, false));

    SDL_UnlockSurface(img->surface);

    mesh->setTexture(tex.back());

    img->~Image();

    img = NULL;

    return true;
}

chai_mesh::~chai_mesh() {
    // delete mesh;
    // delete instance;
    // mesh->~Mesh();
    // tex->~Drawable();
    // tex->~Resource();
    // slices->clear();
    // i->~Image();
    // image->~ImageData();
    // buf = NULL;
    // fs = NULL;
    // instance = NULL;
}

chai_mesh::chai_mesh(const chai_mesh &c) {
    mesh = c.mesh;
    instance = c.instance;

    tex = c.tex;
    image = c.image;
    img = c.img;

    slices = c.slices;
    buf = c.buf;
}

chai_mesh *chai_mesh::clone() const
{
	return new chai_mesh(*this);
}
}