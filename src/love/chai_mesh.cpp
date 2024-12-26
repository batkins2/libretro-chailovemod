#include "../ChaiLove.h"
#include "data/DataModule.h"
#include "image/CompressedImageData.h"
#include "image/FormatHandler.h"

namespace love
{
chai_mesh::chai_mesh() {

}

bool chai_mesh::newMesh(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type) {
    instance = inst;

    std::vector<uint32_t> prepD;

    uint32_t vc = 0xffffffff;

    for (auto vectors : data) {
        for (auto value : chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(vectors)) {
            auto v = chaiscript::boxed_cast<float>(value);
            uint32_t fbits = 0;
            memcpy(&fbits, &v, sizeof(fbits));
            prepD.push_back(fbits);
        }
        // for (int i = 0; i < 4; i++) {
            // size_t currentSize = prepD->size();
            // prepD->resize(currentSize + sizeof(float));
            // std::memcpy(prepD->data() + currentSize, &vc, sizeof(uint8_t));
            prepD.push_back(vc);
        // }
    }

    // std::memcpy(d, &prepD, prepD->size() * sizeof(float));

    vf = std::vector<gfx::Buffer::DataDeclaration>();
    auto b = std::vector<gfx::Buffer::DataDeclaration>();
    for (int i = 0; i < vertexFormat.size(); i++) {
        auto t = chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(vertexFormat.at(i));
        auto temp = std::vector<std::string>();
        for (int c = 0; c < t.size(); c++) {
            auto p = chaiscript::boxed_cast<std::string>(t[c]);
            temp.push_back(p);
        }

        auto t1 = gfx::DATAFORMAT_UNORM8_VEC4;
        if (temp[1] == "byte") {
            vf.push_back(gfx::Buffer::DataDeclaration(temp[0], t1, 1));
            // vf.push_back(gfx::Buffer::DataDeclaration(temp[0], t1, atoi(temp[2].c_str())));
            
        } else if (temp[1] == "float") {
            auto t2 = atoi(temp[2].c_str());
            if (t2 > 2) {
                t1 = gfx::DATAFORMAT_FLOAT_VEC3;
            } else {
                t1 = gfx::DATAFORMAT_FLOAT_VEC2;
            }
            t2 = 1;
            vf.push_back(gfx::Buffer::DataDeclaration(temp[0], t1, t2));
        }
    }
    // vf.push_back(b.at(0));
    auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_STATIC;
    if (type == "triangles") {
        mesh = instance->newMesh(vf, prepD.data(), prepD.size() * sizeof(float), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
    }
    return true;
}

bool chai_mesh::wrap_setTexture(const std::string &texture) {
    img = new Image(texture);

    auto w = img->getWidth();
    auto h = img->getHeight();

    gfx::Texture::Settings settings;
    settings.width = w;
    settings.height = h;
    settings.format = PIXELFORMAT_RGBA8_UNORM;
    // settings.mipmaps = gfx::Texture::MipmapsMode::MIPMAPS_AUTO;

    size_t dataSize = w*h;

    SDL_LockSurface(img->surface);
    // Assuming pixelData is a byte array containing the ARGB data.
    uint8_t* pixelData = static_cast<uint8_t*>(img->surface->pixels);
    uint8_t* copyOfPixelData = new uint8_t[dataSize * 4];
    for (size_t i = 0; i < dataSize * 4; i += 4) {
        uint8_t alpha = pixelData[i];        // ARGB - Alpha at index 0
        uint8_t blue = pixelData[i + 1];      // ARGB - Red at index 1
        uint8_t green = pixelData[i + 2];    // ARGB - Green at index 2
        uint8_t red = pixelData[i + 3];     // ARGB - Blue at index 3

        // Swap to RGBA format
        copyOfPixelData[i] = red;                 // RGBA - Red at index 0
        copyOfPixelData[i + 1] = green;           // RGBA - Green at index 1
        copyOfPixelData[i + 2] = blue;            // RGBA - Blue at index 2
        copyOfPixelData[i + 3] = alpha;           // RGBA - Alpha at index 3
    }

    SDL_UnlockSurface(img->surface);

    img->~Image();

    img = NULL;

    // auto cl = ChaiLove::getInstance();
    // auto i = cl->getImageModule();
    // auto image = i->newImageData(w, h, settings.format);
    auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);

    auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    // for (size_t i = 0; i < dataSize * 4; i += 4) {
    //     uint8_t red = copyOfPixelData[i];        // ARGB - Alpha at index 0
    //     uint8_t green = copyOfPixelData[i + 1];      // ARGB - Red at index 1
    //     uint8_t blue = copyOfPixelData[i + 2];    // ARGB - Green at index 2
    //     uint8_t alpha = copyOfPixelData[i + 3];     // ARGB - Blue at index 3

    //     int pixelIndex = i / 4;
    //     int x = pixelIndex % w;
    //     int y = pixelIndex / w;

    //     auto color = Colorf(red, green, blue, alpha);

    //     image->setPixel(x, y, color);
    // }

    tex = gfx->newTexture(settings, &slices);
    tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
    // image.push_back(i->newImageData(w, h, settings.format, pixelData, false));
    // auto image = i->newImageData(w, h, settings.format, copyOfPixelData, false);

    // slices.set(0, 0, image);

    // slices.validate();

    // slices->clear();

    // tex->replacePixels(copyOfPixelData, dataSize * 4, 0, 0, rect, false);


    // tex->replacePixels(image->getData(), image->getSize(), 0, 0, rect, false);
    mesh->setTexture(tex);


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
    // image = c.image;
    img = c.img;

    slices = c.slices;
    buf = c.buf;
}

chai_mesh *chai_mesh::clone() const
{
	return new chai_mesh(*this);
}
}
