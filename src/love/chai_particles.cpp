#include "../ChaiLove.h"
#include "particlesystem.h"

namespace love {
chai_particles::chai_particles() {
    
}
chai_particles::~chai_particles() {
    
}

void chai_particles::setParticleSystem(const std::string &texture, int size) {
    // Set up the particle system with the given texture and size
    // This is a placeholder implementation
    printf("Setting up particle system with texture: %s and size: %d\n", texture.c_str(), size);
    texName = texture;
    sz = size;
    auto cg = ChaiLove::getInstance()->chai_gfx;
    SDL_RWops* rw = ChaiLove::getInstance()->filesystem.openRW(texture);
    auto img = new Image(rw);

    auto w = img->getWidth();
    auto h = img->getHeight();
    gfx::Texture::Settings settings;
    settings.width = w;
    settings.height = h;
    settings.format = PIXELFORMAT_RGBA8_UNORM;
    auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

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

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    tex = cg.instance->newTexture(settings, &slices);
    tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
    ps = cg.instance->newParticleSystem(tex, size);
}

void chai_particles::updateParticleSystem(float dt) {
    // Update the particle system with the given delta time
    // This is a placeholder implementation
    if (ps) {
        ps->update(dt);
        // printf("Updating particle system with delta time: %f\n", dt);
    } else {
        printf("Particle system not initialized.\n");
    }

}

void chai_particles::setParameter(const std::string &name, const std::vector<float> values) {
    // Set a parameter for the particle system
    // This is a placeholder implementation
    if (ps) {  
        printf("Setting parameter '%s' with values: ", name.c_str());
        for (const auto &value : values) {
            printf("%f ", value);
        }      
        if (name.find("emissionRate") == 0) {
            ps->setEmissionRate(values[0]);
        } else if (name.find("particleLifetime") == 0) {
            ps->setParticleLifetime(values[0], values[1]);
        } else if (name.find("sizeVariation") == 0) {
            ps->setSizeVariation(values[0]);
        } else if (name.find("linearAcceleration") == 0) {
            ps->setLinearAcceleration(values[0], values[1], values[2], values[3]);
        // Add more cases for other parameters as needed
        } else if (name.find("radialAcceleration") == 0) {
            ps->setRadialAcceleration(values[0], values[1]);
        } else if (name.find("tangentialAcceleration") == 0) {
            ps->setTangentialAcceleration(values[0], values[1]);
        } else if (name.find("rotation") == 0) {
            ps->setRotation(values[0], values[1]);
        } else if (name.find("spin") == 0) {
            ps->setSpin(values[0], values[1]);
        } else if (name.find("color") == 0) {
            std::vector<Colorf> newColors = std::vector<Colorf>();
            for (size_t i = 0; i < values.size(); i += 4) {
                if (i + 3 < values.size()) {
                    Colorf color(values[i], values[i + 1], values[i + 2], values[i + 3]);
                    newColors.push_back(color);
                } else {
                    printf("Invalid color values provided.\n");
                    return;
                }
            }
            ps->setColor(newColors);
        } else {
            printf("Unknown parameter: %s\n", name.c_str());
        }     
    } else {
        printf("Particle system not initialized.\n");
    }

}

void chai_particles::draw(float x, float y, float z, float angle, float scaleX, float scaleY, float scaleZ) {
    // Draw the particle system at the specified position and angle
    // This is a placeholder implementation
    if (ps) {
        // printf("Drawing particle system at position (%f, %f) with angle %f and scale (%f, %f)\n", x, y, angle, scaleX, scaleY);
        auto mat = Matrix4(new float[16]{
            scaleX * cos(angle), -scaleY * sin(angle), 0.0f, 0.0f,
            scaleX * sin(angle), scaleY * cos(angle), 0.0f, 0.0f,
            0.0f, 0.0f, scaleZ, 0.0f,
            x, y, z, 1.0f
        });
        // setParticleSystem(tex, sz);
        ps->draw(ChaiLove::getInstance()->chai_gfx.instance, mat);
    } else {
        printf("Particle system not initialized.\n");
    }

}

}