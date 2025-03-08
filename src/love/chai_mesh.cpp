#include "../ChaiLove.h"
#include "data/DataModule.h"
#include "image/CompressedImageData.h"
#include "image/FormatHandler.h"

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
            if (t2 > 3) {
                t1 = gfx::DATAFORMAT_FLOAT_VEC4;
            } else if (t2 > 2) {
                t1 = gfx::DATAFORMAT_FLOAT_VEC3;
            } else {
                t1 = gfx::DATAFORMAT_FLOAT_VEC2;
            }
            t2 = 1;
            vf.push_back(gfx::Buffer::DataDeclaration(temp[0], t1, t2));
        } else if (temp[1] == "word") {
            vf.push_back(gfx::Buffer::DataDeclaration(temp[0], gfx::DATAFORMAT_UINT16_VEC4, 1));
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

gfx::Mesh *loadMesh(int i, tinygltf::Model &model, love::gfx::Graphics *instance, const std::string &type, std::vector<gfx::Buffer::DataDeclaration> &vf, chai_mesh *cm) {
    std::vector<uint32_t> prepD;
    gfx::Texture *tex = nullptr;
    if (i >= 0) {
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

        // cm->nodes = model.scenes[0].nodes;

        cm->skins = std::map<
            int,
            std::map<
                int, std::vector<float>
            >
        >();

        auto skinMaps = std::vector<std::map<int, std::vector<float>>>();
        int s = 0;
        printf("Test point");
        for (auto skin : model.skins) {
            auto inverseBindMatricesAccessor = model.accessors[skin.inverseBindMatrices];
            auto inverseBindMatricesBufferView = model.bufferViews[inverseBindMatricesAccessor.bufferView];
            auto inverseBindMatricesBuffer = model.buffers[inverseBindMatricesBufferView.buffer];
            const float* inverseBindMatrices = reinterpret_cast<const float*>(&inverseBindMatricesBuffer.data[inverseBindMatricesAccessor.byteOffset + inverseBindMatricesBufferView.byteOffset]);

            std::map<int, std::vector<float>> skinMap;

            for (size_t i = 0; i < inverseBindMatricesAccessor.count; ++i) {
                auto node = skin.joints[i];
                auto ibMatrix = std::vector<float>();
                for (size_t j = 0; j < 16; ++j) {
                    ibMatrix.push_back(inverseBindMatrices[i * 16 + j]);
                }
                skinMap[node] = ibMatrix;
            }
            skinMaps.push_back(skinMap);
            cm->jointOrder[s] = skin.joints;
            s++;
        }

        int index = 0;
        for (auto node : model.nodes) {
            if (node.mesh != -1 && node.skin != -1) {
                auto skin = skinMaps[node.skin];
                auto skinMap = std::map<int, std::vector<float>>();
                for (auto s : skin) {
                    skinMap[s.first] = s.second;
                }
                cm->skins[node.mesh] = skinMap;
            }
            
            if (node.mesh != -1) {
                // cm->meshList.push_back(index);
                cm->meshToNode[node.mesh] = index;
            }
            cm->nodeMatrix.push_back(glm::mat4(1.0f));
            if (node.translation.size() > 0) {
                cm->nodeMatrix[index] = glm::translate(cm->nodeMatrix[index], glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() > 0) {
                glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                cm->nodeMatrix[index] *= glm::mat4_cast(rotation);
            }
            if (node.scale.size() > 0) {
                cm->nodeMatrix[index] = glm::scale(cm->nodeMatrix[index], glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }

            if (node.children.size() > 0) {
                cm->nodeChildren[index] = node.children;
                for (auto child : node.children) {
                    cm->nodeParent.push_back(std::pair<int, int>(child, index));
                    printf("%s (%d)->%s (%d)\n", model.nodes[child].name.c_str(), child, model.nodes[index].name.c_str(), index);
                    // glm::mat4 parentMatrix = cm->nodeMatrix[index];

                    // if (node.translation.size() > 0) {
                    //     parentMatrix = glm::translate(parentMatrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                    // }
                    // if (node.rotation.size() > 0) {
                    //     glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    //     parentMatrix *= glm::mat4_cast(rotation);
                    // }
                    // if (node.scale.size() > 0) {
                    //     parentMatrix = glm::scale(parentMatrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                    // }

                    // cm->nodeParentMatrix[child] = parentMatrix;
                }
            }
            // if (node.children.size() > 0) {
            //     auto joints = std::vector<int>();
            //     auto m = -1;
            //     for (auto child : node.children) {
            //         if (model.nodes[child].mesh != -1) {
            //             m = child;
            //         } else {
            //             joints.push_back(child);
            //         }
            //     }
            //     if (m != -1) {
            //         cm->nodeChildren[m] = joints;
            //     }
            // }
            index++;
        }
        printf("Test point 2");

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

                // Extract and push weights
                // for (size_t j = 0; j < 4; ++j) {
                //     float weight = weights[indices[i] * 4 + j];
                //     // uint8_t joint = joints[indices[i] * 4 + j];

                //     // if (weight > 0.0f && joint == 0) {
                //     //     memcpy(&fbits, &weight, sizeof(fbits));
                //     // } else {
                //     //     fbits = 0.0f;
                //     // }
                //     memcpy(&fbits, &weight, sizeof(fbits));
                //     prepD.push_back(fbits);
                //     // printf("%f\n", weights[indices[i] * 4 + j]);
                //     // printf("%f\n", fbits);
                // }

                // Extract and push weights and joints
                if (jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* joints = reinterpret_cast<const uint8_t*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint8_t joint = joints[indices[i] * 4 + j];

                        // if (weight > 0.0f && joint == i) {
                        //     memcpy(&fbits, &weight, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        memcpy(&fbits, &weight, sizeof(fbits));
                        prepD.push_back(fbits);
                        // printf("%f\n", weights[indices[i] * 4 + j]);
                        // printf("%f\n", fbits);
                    }

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint8_t joint = joints[indices[i] * 4 + j];
                        // printf("%d\n", joint);
                        // if (weight > 0.0f && joint == i) {
                        //     auto v = static_cast<float>(joint);
                        //     memcpy(&fbits, &v, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        auto v = static_cast<float>(joint);
                        memcpy(&fbits, &v, sizeof(fbits));
                        // printf("%f\n", fbits);
                        prepD.push_back(fbits);
                    }
                } else if (jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    const float* joints = reinterpret_cast<const float*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        float joint = joints[indices[i] * 4 + j];

                        // if (weight > 0.0f && i == (int)joint) {
                        //     memcpy(&fbits, &weight, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        memcpy(&fbits, &weight, sizeof(fbits));
                        prepD.push_back(fbits);
                        // printf("%f\n", weights[indices[i] * 4 + j]);
                        // printf("%f\n", fbits);
                    }

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        float joint = joints[indices[i] * 4 + j];
                        // printf("%d\n", joint);
                        // if (weight > 0.0f && i == (int)joint) {
                        //     memcpy(&fbits, &joint, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        // auto v = static_cast<float>(joint);
                        memcpy(&fbits, &joint, sizeof(fbits));
                        // printf("%f\n", fbits);
                        prepD.push_back(fbits);
                    }

                } else {
                    const uint16_t* joints = reinterpret_cast<const uint16_t*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint16_t joint = joints[indices[i] * 4 + j];

                        // if (weight > 0.0f && joint == i) {
                        //     memcpy(&fbits, &weight, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        memcpy(&fbits, &weight, sizeof(fbits));
                        prepD.push_back(fbits);
                        // printf("%f\n", weights[indices[i] * 4 + j]);
                        // printf("%f\n", fbits);
                    }

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint16_t joint = joints[indices[i] * 4 + j];
                        // printf("%d\n", joint);
                        // if (weight > 0.0f && joint == i) {
                        //     auto v = static_cast<float>(joint);
                        //     memcpy(&fbits, &v, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        auto v = static_cast<float>(joint);
                        memcpy(&fbits, &v, sizeof(fbits));
                        // printf("%f\n", fbits);
                        prepD.push_back(fbits);
                    }
                }

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

                // Extract and push weights
                // for (size_t j = 0; j < 4; ++j) {
                //     float weight = weights[indices[i] * 4 + j];
                //     // uint8_t joint = joints[indices[i] * 4 + j];

                //     // if (weight > 0.0f && joint == 0) {
                //     //     memcpy(&fbits, &weight, sizeof(fbits));
                //     // } else {
                //     //     fbits = 0.0f;
                //     // }
                //     memcpy(&fbits, &weight, sizeof(fbits));
                //     prepD.push_back(fbits);
                //     // printf("%f\n", weights[indices[i] * 4 + j]);
                //     // printf("%f\n", fbits);
                // }

                // Extract and push weights and joints
                if (jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* joints = reinterpret_cast<const uint8_t*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint8_t joint = joints[indices[i] * 4 + j];

                        // if (weight > 0.0f && joint == i) {
                        //     memcpy(&fbits, &weight, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        memcpy(&fbits, &weight, sizeof(fbits));
                        prepD.push_back(fbits);
                        // printf("%f\n", weights[indices[i] * 4 + j]);
                        // printf("%f\n", fbits);
                    }

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint8_t joint = joints[indices[i] * 4 + j];
                        // printf("%d\n", joint);
                        // if (weight > 0.0f && joint == i) {
                        //     auto v = static_cast<float>(joint);
                        //     memcpy(&fbits, &v, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        auto v = static_cast<float>(joint);
                        memcpy(&fbits, &v, sizeof(fbits));
                        // printf("%f\n", fbits);
                        prepD.push_back(fbits);
                    }
                } else if (jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                    const float* joints = reinterpret_cast<const float*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        float joint = joints[indices[i] * 4 + j];

                        // if (weight > 0.0f && i == (int)joint) {
                        //     memcpy(&fbits, &weight, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        memcpy(&fbits, &weight, sizeof(fbits));
                        prepD.push_back(fbits);
                        // printf("%f\n", weights[indices[i] * 4 + j]);
                        // printf("%f\n", fbits);
                    }

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        float joint = joints[indices[i] * 4 + j];
                        // printf("%d\n", joint);
                        // if (weight > 0.0f && i == (int)joint) {
                        //     memcpy(&fbits, &joint, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        // auto v = static_cast<float>(joint);
                        memcpy(&fbits, &joint, sizeof(fbits));
                        // printf("%f\n", fbits);
                        prepD.push_back(fbits);
                    }

                } else {
                    const uint16_t* joints = reinterpret_cast<const uint16_t*>(&jointBuffer.data[jointAccessor.byteOffset + jointBufferView.byteOffset]);

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint16_t joint = joints[indices[i] * 4 + j];

                        // if (weight > 0.0f && joint == i) {
                        //     memcpy(&fbits, &weight, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        memcpy(&fbits, &weight, sizeof(fbits));
                        prepD.push_back(fbits);
                        // printf("%f\n", weights[indices[i] * 4 + j]);
                        // printf("%f\n", fbits);
                    }

                    for (size_t j = 0; j < 4; ++j) {
                        float weight = weights[indices[i] * 4 + j];
                        uint16_t joint = joints[indices[i] * 4 + j];
                        // printf("%d\n", joint);
                        // if (weight > 0.0f && joint == i) {
                        //     auto v = static_cast<float>(joint);
                        //     memcpy(&fbits, &v, sizeof(fbits));
                        // } else {
                        //     fbits = 0xffffffff;
                        // }
                        auto v = static_cast<float>(joint);
                        memcpy(&fbits, &v, sizeof(fbits));
                        // printf("%f\n", fbits);
                        prepD.push_back(fbits);
                    }
                }

                prepD.push_back(0xffffffff); // Placeholder for any additional data
            }
        }

        auto material = model.materials[primitive.material];
        auto texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
        tex = cm->textures[texture.source];
    }
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


    if (model.cameras.size() > 0) {
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
    } else {
        cm->cameraParams["fov"] = std::vector<float> { 1440.0f / 1080.0f };
        cm->cameraParams["aspectRatio"] = std::vector<float> { 1.33f };
        cm->cameraParams["near"] = std::vector<float> { 0.01 };
        cm->cameraParams["far"] = std::vector<float> { 1000.0f };

        glm::vec3 cameraPosition(0.0f);
        glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
        glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector

        glm::vec3 cameraTarget = cameraPosition + cameraDirection;
        cm->cameraParams["position"] = std::vector<float> { cameraPosition.x, cameraPosition.y, cameraPosition.z };
        cm->cameraParams["up"] = std::vector<float> { cameraUp.x, cameraUp.y, cameraUp.z };
        cm->cameraParams["target"] = std::vector<float> { cameraTarget.x, cameraTarget.y, cameraTarget.z };
    }


    glm::vec3 lightPosition(0.0f);
    glm::vec3 lightDirection(0.0f, -1.0f, 0.0f); // Default direction

    cm->lightParams["position"] = std::vector<float> { lightPosition.x, lightPosition.y, lightPosition.z };
    cm->lightParams["color"] = std::vector<float> { 1.0f, 1.0f, 1.0f };
    cm->lightParams["intensity"] = std::vector<float> { 1.0f };
    cm->lightParams["direction"] = std::vector<float> { lightDirection.x, lightDirection.y, lightDirection.z };

    // Retrieve the light position
    for (auto node : model.nodes) {
        if (node.extensions.find("KHR_lights_punctual") != node.extensions.end()) {
            auto light = node.extensions["KHR_lights_punctual"];
            auto lightNode = model.lights[0];
            // glm::vec3 lightPosition(0.0f);
            if (!node.translation.empty()) {
                lightPosition = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            cm->lightParams["position"] = std::vector<float> { lightPosition.x, lightPosition.y, lightPosition.z };
            cm->lightParams["color"] = std::vector<float> { lightNode.color[0], lightNode.color[1], lightNode.color[2] };
            cm->lightParams["intensity"] = std::vector<float> { lightNode.intensity/900.0f };

            if (lightNode.type == "directional") {
                // glm::vec3 lightDirection(0.0f, -1.0f, 0.0f); // Default direction
                if (!node.rotation.empty()) {
                    glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                    glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
                    lightDirection = glm::vec3(rotationMatrix * glm::vec4(lightDirection, 0.0f));
                }
                cm->lightParams["direction"] = std::vector<float> { lightDirection.x, lightDirection.y, lightDirection.z };
            }
            break;
        }
    }


    // Load animations
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
    size_t anim = 0;
    for (auto animation : model.animations) {
        auto name = animation.name;
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
        > channels;

        size_t chan = 0;
        for (auto channel : animation.channels) {
            auto target_node = channel.target_node;
            auto target_path = channel.target_path;

            auto sampler = animation.samplers[channel.sampler];
            auto inputAccessor = model.accessors[sampler.input];
            auto outputAccessor = model.accessors[sampler.output];

            auto inputBufferView = model.bufferViews[inputAccessor.bufferView];
            auto outputBufferView = model.bufferViews[outputAccessor.bufferView];

            auto inputBuffer = model.buffers[inputBufferView.buffer];
            auto outputBuffer = model.buffers[outputBufferView.buffer];

            const float* inputData = reinterpret_cast<const float*>(&inputBuffer.data[inputAccessor.byteOffset + inputBufferView.byteOffset]);
            const float* outputData = reinterpret_cast<const float*>(&outputBuffer.data[outputAccessor.byteOffset + outputBufferView.byteOffset]);

            auto keyframes = std::map<int, std::vector<std::pair<float, glm::vec4>>>({
                { target_node, std::vector<std::pair<float, glm::vec4>>() }
            });

            size_t keyframe = 0;

            // Apply animation data to nodes
            for (size_t i = 0; i < inputAccessor.count; ++i) {
                float time = inputData[i];

                // Apply transformation based on the target path (translation, rotation, scale)
                if (target_path == "translation") {
                    glm::vec4 translation(outputData[i * 3], outputData[i * 3 + 1], outputData[i * 3 + 2], 1.0f);
                    auto x = std::pair<float, glm::vec4> { time, translation };
                    keyframes[target_node].push_back(x);
                } else if (target_path == "rotation") {
                    glm::vec4 rotation(outputData[i * 4], outputData[i * 4 + 1], outputData[i * 4 + 2], outputData[i * 4 + 3]);
                    auto x = std::pair<float, glm::vec4>  { time, rotation };
                    keyframes[target_node].push_back(x);
                } else if (target_path == "scale") {
                    glm::vec4 scale(outputData[i * 3], outputData[i * 3 + 1], outputData[i * 3 + 2], 1.0f);
                    auto x = std::pair<float, glm::vec4> { time, scale };
                    keyframes[target_node].push_back(x);
                }

                keyframe++;
            }

            if (channels.find(target_path) == channels.end()) {
                channels[target_path] = keyframes;
            } else {
                channels[target_path].merge(keyframes);
            }
        }
        printf("ANIM: %s", name.c_str());
        anims[name] = channels;
        anim++;
    }

    cm->animations = anims;

    if (type == "triangles" && i >= 0) {
        auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_DYNAMIC;
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

    gfx::SamplerState sampler = gfx::SamplerState();

    sampler.wrapU = gfx::SamplerState::WrapMode::WRAP_REPEAT;
    sampler.wrapV = gfx::SamplerState::WrapMode::WRAP_REPEAT;
    sampler.wrapW = gfx::SamplerState::WrapMode::WRAP_REPEAT;

    tex->setSamplerState(sampler);

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

    vf = vertexFormatLoader(vertexFormat);

    for (size_t i = 0; i < model.meshes.size(); i++) {
        meshes.push_back(loadMesh(i, model, instance, type, vf, this));
    }

    if (meshes.size() == 0) {
        meshes.push_back(loadMesh(-1, model, instance, type, vf, this));
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

    gfx::SamplerState sampler = gfx::SamplerState();

    sampler.wrapU = gfx::SamplerState::WrapMode::WRAP_REPEAT;
    sampler.wrapV = gfx::SamplerState::WrapMode::WRAP_REPEAT;
    sampler.wrapW = gfx::SamplerState::WrapMode::WRAP_REPEAT;

    tex->setSamplerState(sampler);
    // tex->replacePixels(image->getData(), image->getSize(), 0, 0, rect, false);
    mesh->setTexture(tex);


    return true;
}

void chai_mesh::playAnimation(const std::string &name, const bool loop) {
    auto animation = animations.find(name);
    if (animation != animations.end()) {
        activeAnimations[name] = std::pair<float, bool>(0.1f, loop);
    }
}

void chai_mesh::endAnimation(const std::string &name) {
    auto animation = activeAnimations.find(name);
    if (animation != activeAnimations.end()) {
        activeAnimations.erase(animation);
    }
}

std::vector<int> getChildNodes(std::map<int, std::vector<int>> nodeChildren, int nodeChild, std::vector<int> nodes) {
    nodes.push_back(nodeChild);
    for (auto nc: nodeChildren[nodeChild]) {
        nodes = getChildNodes(nodeChildren, nc, nodes);
    }
    return nodes;
}

void chai_mesh::draw(love::gfx::Graphics *gfx, const Matrix4 &m, chai_shader *shader, float dt) {
    if (mesh != nullptr) {
        mesh->draw(gfx, m);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
        int i = 0;
        for (auto msh : meshes) {
            
            auto node = meshToNode[i];
            

            if (currentTime != dt) {
                if (jointList.size() > i) {                    
                    jointList[i] = std::vector<int>();
                    jointMatrix[i] = std::map<int, glm::mat4>();
                } else {
                    jointList.push_back(std::vector<int>());
                    jointMatrix.push_back(std::map<int, glm::mat4>());
                }
                auto jointIBMatrix = std::map<int, glm::mat4>();

                if (jointOrder.find(i) != jointOrder.end()) {
                    for (auto joint : jointOrder[i]) {
                        auto matrix = skins[i][joint];
                        jointIBMatrix[joint] = glm::mat4(
                            matrix[0], matrix[1], matrix[2], matrix[3],
                            matrix[4], matrix[5], matrix[6], matrix[7],
                            matrix[8], matrix[9], matrix[10], matrix[11],
                            matrix[12], matrix[13], matrix[14], matrix[15]
                        );
                        jointMatrix[i][joint] = glm::mat4(1.0);
                        jointList[i].push_back(joint);
                    }
                }

                auto animTime = 0.1f;
                auto loop = false;
                std::string name = "";
                std::map<std::string, bool> eraseAnimations;

                for (auto activeAnimation : activeAnimations) {
                    name = activeAnimation.first;
                    animTime = activeAnimation.second.first;
                    // animTime = 2.5f;
                    // animTime = 0.1f;
                    loop = activeAnimation.second.second;

                    auto animPlaying = -1;

                    auto animation = animations[name];
                    int nodeIndex = -1;

                    for (const auto& channel : animation) {

                        auto chan = channel.first;
                        for (const auto& keyframe : channel.second) {
                            nodeIndex = keyframe.first;

                            if (jointMatrix[i].find(nodeIndex) == jointMatrix[i].end()) {
                                continue;
                            }
                            const auto& keyframes = keyframe.second;

                            // Find the two keyframes to interpolate between
                            auto it = std::lower_bound(keyframes.begin(), keyframes.end(), animTime,
                                [](const std::pair<float, glm::vec4>& a, float b) {
                                    return a.first < b;
                                });

                            if (it == keyframes.end()) {
                                // Use the last keyframe if the current time is beyond the last keyframe
                                it = keyframes.end() - 1;
                            }

                            auto nextIt = it + 1;
                            if (nextIt == keyframes.end()) {
                                nextIt = it;
                            }

                            float t1 = it->first;
                            float t2 = nextIt->first;

                            const glm::vec4& v1 = it->second;
                            const glm::vec4& v2 = nextIt->second;

                            // Interpolate between the two keyframes
                            float t = (t1 - animTime) / (t2 - t1);
                            glm::vec4 interpolatedValue = glm::vec4(1.0);
                            if (chan != "rotation") {
                                interpolatedValue = glm::mix(v1, v2, t);
                            }

                            if (animPlaying == -1) {
                                animPlaying = 0;
                            }

                            // Update the joint matrix
                            if (glm::all(glm::isnan(interpolatedValue)) == false) {
                                auto nodes = std::vector<int>();
                                nodes.push_back(nodeIndex);
                                int parent = -1;
                                int root = -1;
                                int subroot = -1;
                                auto offset = glm::mat4(1.0f);
                                auto hierarchy = std::vector<int>();
                                int np = nodeIndex;

                                // Initialize the hierarchy vector with np
                                hierarchy.push_back(np);

                                // Traverse the nodeParent pairs to find the parent of the current node
                                while (true) {
                                    bool found = false;
                                    for (const auto& pair : nodeParent) {
                                        if (pair.first == np) {   
                                            subroot = np;                                         
                                            np = pair.second;
                                            hierarchy.push_back(np);
                                            found = true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        root = np;
                                        break; // Reached the root node
                                    }
                                }
                                
                                for (auto node : nodeChildren) {
                                    for (auto n : node.second) {
                                        if (n == nodeIndex) {
                                            parent = node.first;
                                            break;
                                        }
                                        
                                    }
                                    if (parent != -1) {
                                        break;
                                    }
                                    
                                }
                                for (auto nodeChild : nodeChildren[nodeIndex]) {
                                    nodes = getChildNodes(nodeChildren, nodeChild, nodes);
                                }
                                for (auto node : nodes) {
                                    if (chan == "translation") {

                                        animPlaying = 1;
                                        glm::vec3 v(interpolatedValue);
                                        jointMatrix[i][node] = glm::translate(jointMatrix[i][node], v) * jointMatrix[i][node];
                                        // printf("Translate: %d %f %f %f\n", node, interpolatedValue.x, interpolatedValue.y, interpolatedValue.z);
                                    } else if (chan == "rotation" && t <= 1.0f && t >= 0.0f) {
                                        animPlaying = 1;
                                        glm::quat rotation = glm::quat(1.0f, 1.0f, 1.0f, 1.0f);
                                        auto bindMatrix = glm::inverse(jointIBMatrix[node]);
                                        if (node == nodes[0]) {
                                            rotation = glm::slerp(glm::quat(v2.w, v2.x, v2.y, v2.z), glm::quat(v1.w, v1.x, v1.y, v1.z), t);
                                            auto parentMatrix = glm::mat4(1.0f);
                                            if (true || parent != root) {
                                                parentMatrix = jointIBMatrix[root];
                                            }
                                            
                                            auto invMatrix = glm::inverse(nodeMatrix[node]);
                                            
                                            auto hierarchyRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                                            auto hierarchyRotation2 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                                            for (auto h = hierarchy.rbegin(); h != hierarchy.rend(); ++h) {
                                                if (node == subroot) {
                                                    hierarchyRotation = hierarchyRotation * glm::quat_cast(jointMatrix[i][*h]);
                                                    hierarchyRotation2 = hierarchyRotation2 * glm::quat_cast(glm::inverse(nodeMatrix[*h]));
                                                } else {
                                                    if (*h == node) {
                                                        hierarchyRotation = hierarchyRotation * glm::quat_cast(jointMatrix[i][*h]);
                                                        hierarchyRotation2 = hierarchyRotation2 * glm::quat_cast(glm::inverse(nodeMatrix[*h]));
                                                    }
                                                }
                                            }

                                            rotation = hierarchyRotation * rotation;
                                            rotation = hierarchyRotation2 * rotation;
                                                                                            
                                            rotation = glm::normalize(rotation);
                                            
                                            jointMatrix[i][node] = bindMatrix * glm::mat4_cast(rotation) * jointIBMatrix[node] * jointMatrix[i][node];
                                            parent = node;
                                        } else {
                                            // Child node
                                            jointMatrix[i][node] = jointMatrix[i][parent] * jointMatrix[i][node];
                                        }
                                    } else if (chan == "scale") {
                                        animPlaying = 1;
                                        glm::vec3 v(interpolatedValue);
                                        jointMatrix[i][node] = glm::scale(jointMatrix[i][node], v) * jointMatrix[i][node];
                                    }
                                }
                            } 
                        }
                    }
                    if (animPlaying == 1) {
                        activeAnimations[name].first += 0.01f;
                    } else if (animPlaying == 0) {
                        if (loop) {
                            activeAnimations[name].first = 0.1f;
                        } else {
                            eraseAnimations[name] = true;
                        }
                    }
                }
                
                

                for (auto eraseAnimation : eraseAnimations) {
                    activeAnimations.erase(eraseAnimation.first);
                }
            }

            shader->send("jointCount", std::vector<chaiscript::Boxed_Value>({ chaiscript::Boxed_Value((int)jointList[i].size()) }));
            shader->sendMap("jointMatrix", jointMatrix[i], jointList[i]);

            auto mat = matrices[i] * m;
            std::vector<chaiscript::Boxed_Value> v;
            for (int c = 0; c < 4; ++c) {
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).x));
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).y));
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).z));
                v.push_back(chaiscript::Boxed_Value(mat.getColumn(c).w));
            }
            shader->send("modelMatrix", v);
            
            if (msh != nullptr) {
                msh->draw(gfx, m);
            }
            i++;            
        }
        currentTime = dt;
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
