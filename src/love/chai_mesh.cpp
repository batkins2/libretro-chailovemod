#include "../ChaiLove.h"
#include "data/DataModule.h"
#include "image/CompressedImageData.h"
#include "image/FormatHandler.h"
#include <glm/glm.hpp>

#ifndef TINY_GLTF_H_
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_FS
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "../../vendor/tinygltf/tiny_gltf.h"
#endif

namespace love
{
chai_mesh::chai_mesh() {

}

std::vector<gfx::Buffer::DataDeclaration> vertexFormatLoader(const std::vector<chaiscript::Boxed_Value> &vertexFormat) {
    auto vf = std::vector<gfx::Buffer::DataDeclaration>();
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
    return vf;
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

    vf = vertexFormatLoader(vertexFormat);

    auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_DYNAMIC;
    if (type == "triangles") {
        mesh = instance->newMesh(vf, prepD.data(), prepD.size() * sizeof(uint32_t), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
    }
    return true;
}

void loadTexture(gfx::Mesh *mesh, std::string texture) {
    auto img = new Image(texture);

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

    auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);

    auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    auto tex = gfx->newTexture(settings, &slices);
    tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
    
    mesh->setTexture(tex);
}

gfx::Mesh *loadMesh(size_t i, tinygltf::Model &model, love::gfx::Graphics *instance, const std::string &type, std::vector<gfx::Buffer::DataDeclaration> &vf, chai_mesh *cm) {
    std::vector<uint32_t> prepD;
    auto primitive = model.meshes[i].primitives[0];
    auto positionAccessor = model.accessors[primitive.attributes["POSITION"]];
    auto positionBufferView = model.bufferViews[positionAccessor.bufferView];
    auto positionBuffer = model.buffers[positionBufferView.buffer];
    const float* positions = reinterpret_cast<const float*>(&positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset]);

    auto texCoordAccessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
    auto texCoordBufferView = model.bufferViews[texCoordAccessor.bufferView];
    auto texCoordBuffer = model.buffers[texCoordBufferView.buffer];
    const float* texCoords = reinterpret_cast<const float*>(&texCoordBuffer.data[texCoordAccessor.byteOffset + texCoordBufferView.byteOffset]);

    auto normalAccessor = model.accessors[primitive.attributes["NORMAL"]];
    auto normalBufferView = model.bufferViews[normalAccessor.bufferView];
    auto normalBuffer = model.buffers[normalBufferView.buffer];
    const float* normals = reinterpret_cast<const float*>(&normalBuffer.data[normalAccessor.byteOffset + normalBufferView.byteOffset]);

    auto indiceAccessor = model.accessors[primitive.indices];
    auto indiceBufferView = model.bufferViews[indiceAccessor.bufferView];
    auto indiceBuffer = model.buffers[indiceBufferView.buffer];

    auto weightAccessor = model.accessors[primitive.attributes["WEIGHTS_0"]];
    auto weightBufferView = model.bufferViews[weightAccessor.bufferView];
    auto weightBuffer = model.buffers[weightBufferView.buffer];
    const float* weights = reinterpret_cast<const float*>(&weightBuffer.data[weightAccessor.byteOffset + weightBufferView.byteOffset]);

    auto jointAccessor = model.accessors[primitive.attributes["JOINTS_0"]];
    auto jointBufferView = model.bufferViews[jointAccessor.bufferView];
    auto jointBuffer = model.buffers[jointBufferView.buffer];
    const unsigned short* joints = reinterpret_cast<const unsigned short*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

    if (indiceAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const unsigned int* indices = reinterpret_cast<const unsigned int*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
        uint32_t fbits = 0xffffffff;
        for (size_t i = 0; i < indiceAccessor.count; ++i) {
            // Extract and push positions
            for (size_t j = 0; j < 3; ++j) {
                memcpy(&fbits, &positions[indices[i] * 3 + j], sizeof(fbits));
                prepD.push_back(fbits);
            }

            // Extract and push texture coordinates
            for (size_t j = 0; j < 2; ++j) {
                memcpy(&fbits, &texCoords[indices[i] * 2 + j], sizeof(fbits));
                prepD.push_back(fbits);
            }

            // Extract and push normals
            for (size_t j = 0; j < 3; ++j) {
                memcpy(&fbits, &normals[indices[i] * 3 + j], sizeof(fbits));
                prepD.push_back(fbits);
            }

            // // Extract and push weights
            // for (size_t j = 0; j < 4; ++j) {
            //     memcpy(&fbits, &weights[indices[i] * 4 + j], sizeof(fbits));
            //     prepD.push_back(fbits);
            // }

            // // Extract and push joints
            // for (size_t j = 0; j < 4; ++j) {
            //     memcpy(&fbits, &joints[indices[i] * 4 + j], sizeof(fbits));
            //     prepD.push_back(fbits);
            // }

            prepD.push_back(0xffffffff); // Placeholder for any additional data
        }
    } else {
        const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
        uint32_t fbits = 0xffffffff;
        for (size_t i = 0; i < indiceAccessor.count; ++i) {
            // Extract and push positions
            for (size_t j = 0; j < 3; ++j) {
                memcpy(&fbits, &positions[indices[i] * 3 + j], sizeof(fbits));
                prepD.push_back(fbits);
            }

            // Extract and push texture coordinates
            for (size_t j = 0; j < 2; ++j) {
                memcpy(&fbits, &texCoords[indices[i] * 2 + j], sizeof(fbits));
                prepD.push_back(fbits);
            }

            // Extract and push normals
            for (size_t j = 0; j < 3; ++j) {
                memcpy(&fbits, &normals[indices[i] * 3 + j], sizeof(fbits));
                prepD.push_back(fbits);
            }

            // // Extract and push weights
            // for (size_t j = 0; j < 4; ++j) {
            //     memcpy(&fbits, &weights[indices[i] * 4 + j], sizeof(fbits));
            //     prepD.push_back(fbits);
            // }

            // // Extract and push joints
            // for (size_t j = 0; j < 4; ++j) {
            //     memcpy(&fbits, &joints[indices[i] * 4 + j], sizeof(fbits));
            //     prepD.push_back(fbits);
            // }

            prepD.push_back(0xffffffff); // Placeholder for any additional data
        }
    }
    
    auto material = model.materials[primitive.material];
    auto texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
    auto tex = cm->textures[texture.source];
    // auto imageBufferView = model.bufferViews[image->bufferView];
    // auto imageBuffer = model.buffers[imageBufferView.buffer];
    // const unsigned char* imageData = &imageBuffer.data[imageBufferView.byteOffset];
    
    // Retrieve the transformation matrix for the model
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    for (auto node : model.nodes) {
        if (node.mesh == i) {
            if (!node.matrix.empty()) {
                modelMatrix = glm::make_mat4(node.matrix.data());
            } else {
                if (!node.translation.empty()) {
                    modelMatrix = glm::translate(modelMatrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                }
                if (!node.rotation.empty()) {
                    glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    modelMatrix *= glm::mat4_cast(rotation);
                }
                if (!node.scale.empty()) {
                    modelMatrix = glm::scale(modelMatrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                }
            }
            break;
        }
    }

    auto matrix = Matrix4(new float[16] {
        modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2], modelMatrix[0][3],
        modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2], modelMatrix[1][3],
        modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2], modelMatrix[2][3],
        modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2], modelMatrix[3][3]
    });

    cm->matrices.push_back(matrix);

    auto camera = model.cameras[0];

    if (camera.type == "perspective") {
        cm->cameraParams["fov"] = std::vector<float> { camera.perspective.yfov };
        cm->cameraParams["aspectRatio"] = std::vector<float> { camera.perspective.aspectRatio };
        cm->cameraParams["near"] = std::vector<float> { camera.perspective.znear };
        cm->cameraParams["far"] = std::vector<float> { camera.perspective.zfar };
    }
    
    // Retrieve the camera position
    for (auto node : model.nodes) {
        if (node.camera == 0) {
            glm::vec3 cameraPosition(0.0f);
            glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
            glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector
            if (!node.translation.empty()) {
                cameraPosition = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            if (!node.rotation.empty()) {
                glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
                cameraUp = glm::vec3(rotationMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
                cameraDirection = glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            }
            glm::vec3 cameraTarget = cameraPosition + cameraDirection;
            cm->cameraParams["position"] = std::vector<float> { cameraPosition.x, cameraPosition.y, cameraPosition.z };
            cm->cameraParams["up"] = std::vector<float> { cameraUp.x, cameraUp.y, cameraUp.z };
            cm->cameraParams["target"] = std::vector<float> { cameraTarget.x, cameraTarget.y, cameraTarget.z };
            break;
        }
    }

    // Load animations
    std::vector< // Animation
        std::vector< // Channel
            std::map<
                int, // Node
                std::vector< // Keyframe
                    std::pair< // Keyframe data
                        float, // Time
                        glm::vec3 // Data
                    >
                >
            >
        >
    > anims;
    size_t anim = 0;
    for (auto animation : model.animations) {
        std::vector< // Channel
            std::map<
                int, // Node
                std::vector< // Keyframe
                    std::pair< // Keyframe data
                        float, // Time
                        glm::vec3 // Data
                    >
                >
            >
        > channels;
        
        size_t chan = 0;
        for (auto channel : animation.channels) {
            auto target_node = channel.target_node;
            
            auto sampler = animation.samplers[channel.sampler];
            auto inputAccessor = model.accessors[sampler.input];
            auto outputAccessor = model.accessors[sampler.output];

            auto inputBufferView = model.bufferViews[inputAccessor.bufferView];
            auto outputBufferView = model.bufferViews[outputAccessor.bufferView];

            auto inputBuffer = model.buffers[inputBufferView.buffer];
            auto outputBuffer = model.buffers[outputBufferView.buffer];

            const float* inputData = reinterpret_cast<const float*>(&inputBuffer.data[inputAccessor.byteOffset + inputBufferView.byteOffset]);
            const float* outputData = reinterpret_cast<const float*>(&outputBuffer.data[outputAccessor.byteOffset + outputBufferView.byteOffset]);

            auto keyframes = std::map<int, std::vector<std::pair<float, glm::vec3>>>({
                { target_node, std::vector<std::pair<float, glm::vec3>>() }
            });

            size_t keyframe = 0;

            // Apply animation data to nodes
            for (size_t i = 0; i < inputAccessor.count; ++i) {
                float time = inputData[i];
                
                // Apply transformation based on the target path (translation, rotation, scale)
                if (channel.target_path == "translation") {
                    glm::vec3 translation(outputData[i * 3], outputData[i * 3 + 1], outputData[i * 3 + 2]);
                    auto x = std::pair<float, glm::vec3> { time, translation };
                    keyframes[target_node].push_back(x);
                } else if (channel.target_path == "rotation") {
                    glm::quat rotation(outputData[i * 4 + 3], outputData[i * 4], outputData[i * 4 + 1], outputData[i * 4 + 2]);
                    auto x = std::pair<float, glm::vec3>  { time, glm::eulerAngles(rotation) };
                    keyframes[target_node].push_back(x);
                } else if (channel.target_path == "scale") {
                    glm::vec3 scale(outputData[i * 3], outputData[i * 3 + 1], outputData[i * 3 + 2]);
                    auto x = std::pair<float, glm::vec3> { time, scale };
                    keyframes[target_node].push_back(x);
                }

                keyframe++;                
            }

            channels.push_back(keyframes);
            chan++;
        }
        anims.push_back(channels);
        anim++;
    }

    cm->animations = anims;

    auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_DYNAMIC;
    if (type == "triangles") {
        auto m = instance->newMesh(vf, prepD.data(), prepD.size() * sizeof(float), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
        m->setTexture(tex);
        return m;
    }

    return nullptr;
}

bool LoadImageData(tinygltf::Image *image, const int image_idx, std::string *err, std::string *warn, int req_width, int req_height, const unsigned char *bytes, int size, void *user_data) {
    auto cm = static_cast<chai_mesh *>(user_data);
    // cm->images.push_back(image);

    auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

    auto img = new love::Types::Graphics::Image(SDL_RWFromConstMem(bytes, size));

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

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    auto tex = cm->instance->newTexture(settings, &slices);
    tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);

    cm->textures.push_back(tex);

    return true;
}

std::map<std::string, std::vector<float>> chai_mesh::getCameraParams() {
    return cameraParams;
}

bool chai_mesh::newMeshFromFile(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::string *FileName, const std::string &type) {
    instance = inst;

    auto cl = ChaiLove::getInstance();
    auto f = cl->getFSModule();
    auto s = f.getSize(*FileName);
    auto file = f.readBuffer(*FileName, s);

    // Allocate a character array and copy the file data into it
    unsigned char* data = new unsigned char[s];
    std::memcpy(data, file, s);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    
    // Set the LoadImageData callback
    loader.SetImageLoader(&LoadImageData, this);
    loader.LoadBinaryFromMemory(&model, &err, &warn, data, s, "", false);
    
    // Convert the Magnum mesh to your internal mesh format
    // This part depends on your internal mesh representation
    // Here is an example of how you might do it:

    vf = vertexFormatLoader(vertexFormat);

    for (size_t i = 0; i < model.meshes.size(); i++) {
        meshes.push_back(loadMesh(i, model, instance, type, vf, this));
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

void chai_mesh::draw(love::gfx::Graphics *gfx, const Matrix4 &m, chai_shader *shader) {
    if (mesh != nullptr) {
        mesh->draw(gfx, m);
    } else {
        size_t i = 0;

        auto position = glm::vec3(cameraParams.at("position")[0], cameraParams.at("position")[1], cameraParams.at("position")[2]);
        auto target = glm::vec3(cameraParams.at("target")[0], cameraParams.at("target")[1], cameraParams.at("target")[2]);
        auto up = glm::vec3(cameraParams.at("up")[0], cameraParams.at("up")[1], cameraParams.at("up")[2]);
        auto t = glm::lookAt(position, target, up);
        auto vm = glm::value_ptr(t);

        float fov = cameraParams.at("fov")[0];
        float aspectRatio = cameraParams.at("aspectRatio")[0];
        float nearClip = cameraParams.at("near")[0];
        float farClip = cameraParams.at("far")[0];
        auto t2 = glm::perspective(fov, aspectRatio, nearClip, farClip);
        auto pm = glm::value_ptr(t2);

        std::vector<chaiscript::Boxed_Value> viewMatrix;
        for (int c = 0; c < 16; ++c) {
            viewMatrix.push_back(chaiscript::Boxed_Value(vm[c]));
        }

        std::vector<chaiscript::Boxed_Value> projectionMatrix;
        for (int c = 0; c < 16; ++c) {
            projectionMatrix.push_back(chaiscript::Boxed_Value(pm[c]));
        }

        shader->send("viewMatrix", viewMatrix);
        shader->send("projectionMatrix", projectionMatrix);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (auto msh : meshes) {
            auto mat = matrices[i];
            std::vector<chaiscript::Boxed_Value> v;
            for (int c = 0; c < 4; ++c) {
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).x));
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).y));
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).z));
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).w));
            }            
            shader->send("modelMatrix", v);            
            msh->draw(gfx, m);
            i++;
        }
    }
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
