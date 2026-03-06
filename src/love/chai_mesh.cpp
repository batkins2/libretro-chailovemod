#include "../ChaiLove.h"
#include "data/DataModule.h"
#include "image/CompressedImageData.h"
#include "image/FormatHandler.h"
#include <map>

#ifndef TINY_GLTF_H_
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_FS
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "../../vendor/tinygltf/tiny_gltf.h"
#endif
// #include "../../vendor/MemPlumber/memplumber.h"
// #include "../../vendor/MemPlumber/memplumber-internals.h"

namespace love
{
chai_mesh::chai_mesh(std::vector<chai_meshData*> &data) {
    this->data = data;
    m_cachedAnimationDurations.clear();
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

chai_mesh *chai_mesh::newMeshWithData(std::vector<chai_meshData*> &data) {
    // auto d = std::vector<chai_meshData*>();
    // for (auto i : data) {
    //     auto d1 = chaiscript::boxed_cast<chai_meshData*>(i);
    //     d.push_back(d1);
    // }
    // auto m = new chai_mesh(d);
    auto m = new chai_mesh(data);
    return m;
}

chai_mesh *chai_mesh::newMesh() {
    // auto d = std::vector<chai_meshData*>();
    // for (auto i : data) {
    //     auto d1 = chaiscript::boxed_cast<chai_meshData*>(i);
    //     d.push_back(d1);
    // }
    // auto m = new chai_mesh(d);
    auto m = new chai_mesh();
    return m;
}

// bool chai_mesh::newMesh(love::gfx::Graphics *inst, const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::vector<chaiscript::Boxed_Value> &data, const std::string &type) {
//     instance = inst;

//     std::vector<uint32_t> prepD;

//     uint32_t vc = 0xffffffff;

//     for (auto vectors : data) {
//         for (auto value : chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(vectors)) {
//             auto v = chaiscript::boxed_cast<float>(value);
//             uint32_t fbits = 0;
//             memcpy(&fbits, &v, sizeof(fbits));
//             prepD.push_back(fbits);
//         }
//         // for (int i = 0; i < 4; i++) {
//             // size_t currentSize = prepD->size();
//             // prepD->resize(currentSize + sizeof(float));
//             // std::memcpy(prepD->data() + currentSize, &vc, sizeof(uint8_t));
//             prepD.push_back(vc);
//         // }
//     }

//     // std::memcpy(d, &prepD, prepD->size() * sizeof(float));

//     vf = vertexFormatLoader(vertexFormat);

//     auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_DYNAMIC;
//     if (type == "triangles") {
//         mesh = instance->newMesh(vf, prepD.data(), prepD.size() * sizeof(uint32_t), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
//     }
//     return true;
// }

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
    
    try {
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
    } catch (...) {
        SDL_UnlockSurface(img->surface);
        delete img;
        delete[] copyOfPixelData;
        throw;
    }

    SDL_UnlockSurface(img->surface);

    delete img;
    img = NULL;

    auto gfx = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);

    auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    auto tex = gfx->newTexture(settings, &slices);
    tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
    delete[] copyOfPixelData;  // Free allocated pixel buffer

    if (tex) {
        mesh->setTexture(tex);
        // Note: texture is not tracked here as loadTexture is a standalone function
        // The caller should manage the texture lifetime or it should be tracked elsewhere
    }
}

std::pair<gfx::Mesh*, chai_meshData*> loadMesh(int i, tinygltf::Model &model, love::gfx::Graphics *instance, const std::string &type, std::vector<gfx::Buffer::DataDeclaration> &vf, chai_mesh *cm, chai_meshData *readyData = nullptr) {
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

        // cm->skins = std::map<
        //     int,
        //     std::map<
        //         int, std::vector<float>
        //     >
        // >();

        // auto skinMaps = std::vector<std::map<int, std::vector<float>>>();
        // int s = 0;
        // // printf("Test point");
        // for (auto skin : model.skins) {
        //     auto inverseBindMatricesAccessor = model.accessors[skin.inverseBindMatrices];
        //     auto inverseBindMatricesBufferView = model.bufferViews[inverseBindMatricesAccessor.bufferView];
        //     auto inverseBindMatricesBuffer = model.buffers[inverseBindMatricesBufferView.buffer];
        //     const float* inverseBindMatrices = reinterpret_cast<const float*>(&inverseBindMatricesBuffer.data[inverseBindMatricesAccessor.byteOffset + inverseBindMatricesBufferView.byteOffset]);

        //     std::map<int, std::vector<float>> skinMap;

        //     for (size_t i = 0; i < inverseBindMatricesAccessor.count; ++i) {
        //         auto node = skin.joints[i];
        //         auto ibMatrix = std::vector<float>();
        //         for (size_t j = 0; j < 16; ++j) {
        //             ibMatrix.push_back(inverseBindMatrices[i * 16 + j]);
        //         }
        //         skinMap[node] = ibMatrix;
        //     }
        //     skinMaps.push_back(skinMap);
        //     cm->jointOrder[s] = skin.joints;
        //     s++;
        // }

        // int index = 0;
        // for (auto node : model.nodes) {
        //     if (node.mesh != -1 && node.skin != -1) {
        //         auto skin = skinMaps[node.skin];
        //         cm->nodeNames[node.name] = index;
        //         auto skinMap = std::map<int, std::vector<float>>();
        //         for (auto s : skin) {
        //             skinMap[s.first] = s.second;
        //         }
        //         cm->skins[node.mesh] = skinMap;
        //     }
            
        //     if (node.mesh != -1) {
        //         // cm->meshList.push_back(index);
        //         cm->meshToNode[node.mesh] = index;
        //     }
        //     cm->nodeMatrix.push_back(glm::mat4(1.0f));
        //     if (node.translation.size() > 0) {
        //         cm->nodeMatrix[index] = glm::translate(cm->nodeMatrix[index], glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        //     }
        //     if (node.rotation.size() > 0) {
        //         glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        //         cm->nodeMatrix[index] *= glm::mat4_cast(rotation);
        //     }
        //     if (node.scale.size() > 0) {
        //         cm->nodeMatrix[index] = glm::scale(cm->nodeMatrix[index], glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        //     }

        //     if (node.children.size() > 0) {
        //         cm->nodeChildren[index] = node.children;
        //         for (auto child : node.children) {
        //             cm->nodeParent.push_back(std::pair<int, int>(child, index));
        //             printf("%s (%d)->%s (%d)\n", model.nodes[child].name.c_str(), child, model.nodes[index].name.c_str(), index);
        //             // glm::mat4 parentMatrix = cm->nodeMatrix[index];

        //             // if (node.translation.size() > 0) {
        //             //     parentMatrix = glm::translate(parentMatrix, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        //             // }
        //             // if (node.rotation.size() > 0) {
        //             //     glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        //             //     parentMatrix *= glm::mat4_cast(rotation);
        //             // }
        //             // if (node.scale.size() > 0) {
        //             //     parentMatrix = glm::scale(parentMatrix, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        //             // }

        //             // cm->nodeParentMatrix[child] = parentMatrix;
        //         }
        //     }
        //     // if (node.children.size() > 0) {
        //     //     auto joints = std::vector<int>();
        //     //     auto m = -1;
        //     //     for (auto child : node.children) {
        //     //         if (model.nodes[child].mesh != -1) {
        //     //             m = child;
        //     //         } else {
        //     //             joints.push_back(child);
        //     //         }
        //     //     }
        //     //     if (m != -1) {
        //     //         cm->nodeChildren[m] = joints;
        //     //     }
        //     // }
        //     index++;
        // }
        // printf("Test point 2");

        if (readyData == nullptr && indiceAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
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
        } else if (readyData == nullptr) {
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
        // tex = nullptr;
        if (material.pbrMetallicRoughness.baseColorTexture.index > -1) {
        // #ifdef _WIN32
        // __debugbreak();
        // #else
        // raise(SIGTRAP);
        // #endif
            auto texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
            // Check if texture.source is within bounds of cm->textures vector
            if (texture.source >= 0 && texture.source < static_cast<int>(cm->textures.size())) {
                tex = cm->textures[texture.source];
            } else {
                printf("[WARNING] Texture source index %d out of bounds (textures size: %zu)\n", 
                       texture.source, cm->textures.size());
                tex = nullptr;
            }
        } else {
            tex = nullptr;
        }
        
        if (tex == nullptr) {
            // Set the texture to a single color texture using base color factor if no texture is found  
            gfx::Texture::Settings settings;
            settings.width = 64;
            settings.height = 64;
            settings.format = PIXELFORMAT_RGBA8_UNORM;
            auto slices = gfx::Texture::Slices(gfx::TextureType::TEXTURE_2D);

            uint8_t* colorPixel = new uint8_t[64*64*4];
            
            // Get base color from material or use white as default - FORCE OPAQUE
            uint8_t r = 255, g = 255, b = 255, a = 255;
            if (material.pbrMetallicRoughness.baseColorFactor.size() >= 4) {
                r = static_cast<uint8_t>(material.pbrMetallicRoughness.baseColorFactor[0] * 255.0f);
                g = static_cast<uint8_t>(material.pbrMetallicRoughness.baseColorFactor[1] * 255.0f);
                b = static_cast<uint8_t>(material.pbrMetallicRoughness.baseColorFactor[2] * 255.0f);
                // ALWAYS force alpha to 255 - ignore material alpha
                printf("[DEBUG] Base color factor: R=%d G=%d B=%d A=255 (FORCED) (raw: %.4f %.4f %.4f %.4f)\n", 
                    r, g, b,
                    material.pbrMetallicRoughness.baseColorFactor[0],
                    material.pbrMetallicRoughness.baseColorFactor[1],
                    material.pbrMetallicRoughness.baseColorFactor[2],
                    material.pbrMetallicRoughness.baseColorFactor[3]);
            }

            printf("[DEBUG] Creating fallback texture with color: R=%d G=%d B=%d A=%d\n", r, g, b, a);

            // Fill the pixel data with the base color
            // Using RGBA byte order to match PIXELFORMAT_RGBA8_UNORM
            for (int j = 0; j < 64*64*4; j+=4) {
                colorPixel[j] = r;         // Red
                colorPixel[j + 1] = g;     // Green
                colorPixel[j + 2] = b;     // Blue
                colorPixel[j + 3] = a;     // Alpha
            }
            
            // Verify first few pixels
            printf("[DEBUG] First pixel data (RGBA): R=%d G=%d B=%d A=%d\n", 
                colorPixel[0], colorPixel[1], colorPixel[2], colorPixel[3]);
            printf("[DEBUG] Last pixel data (RGBA): R=%d G=%d B=%d A=%d\n", 
                colorPixel[64*64*4-4], colorPixel[64*64*4-3], colorPixel[64*64*4-2], colorPixel[64*64*4-1]);
            
            auto gfxInstance = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);
            tex = gfxInstance->newTexture(settings, &slices);
            printf("[DEBUG] Created fallback texture: %p\n", (void*)tex);
            
            // Now update with actual pixel data
            Rect rect = {};   // zero-init: x=0, y=0, w=0, h=0
            rect.w = 64;
            rect.h = 64;
            tex->replacePixels(colorPixel, 4*64*64, 0, 0, rect, false);
            printf("[DEBUG] Texture pixels replaced\n");
            
            // Since replacePixels with false makes a copy, we can delete our copy
            delete[] colorPixel;
            gfx::SamplerState sampler = gfx::SamplerState();

            sampler.wrapU = gfx::SamplerState::WrapMode::WRAP_REPEAT;
            sampler.wrapV = gfx::SamplerState::WrapMode::WRAP_REPEAT;
            sampler.wrapW = gfx::SamplerState::WrapMode::WRAP_REPEAT;

            tex->setSamplerState(sampler);
            
        }
        // printf("Material: %s\n", material.name.c_str());
        // #ifdef _WIN32
        // __debugbreak();
        // #else
        // raise(SIGTRAP);
        // #endif
               
        if (material.extensions.find("KHR_materials_clearcoat") != material.extensions.end()) {
            auto clearcoatFactor = material.extensions.at("KHR_materials_clearcoat").Get("clearcoatFactor");
            if (clearcoatFactor.GetNumberAsDouble() > 0.0) {
                cm->specular[i] = true;
            } 
        }

        // ---- Blender-level PBR material data ----------------------------------------
        // Grow all PBR vectors to cover mesh index i (resize is safer than
        // while+push_back; avoids repeated capacity checks).
        if ((int)cm->normalTextures.size() <= i)            cm->normalTextures.resize(i + 1, nullptr);
        if ((int)cm->metallicRoughnessTextures.size() <= i) cm->metallicRoughnessTextures.resize(i + 1, nullptr);
        if ((int)cm->emissiveTextures.size() <= i)          cm->emissiveTextures.resize(i + 1, nullptr);
        if ((int)cm->occlusionTextures.size() <= i)         cm->occlusionTextures.resize(i + 1, nullptr);
        if ((int)cm->materialPropsList.size() <= i)         cm->materialPropsList.resize(i + 1);

        chai_mesh::PBRMaterialProps& props = cm->materialPropsList[i];

        // Scalar PBR factors
        props.metallicFactor  = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
        props.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
        if (material.emissiveFactor.size() >= 3) {
            props.emissiveFactor = glm::vec3(
                static_cast<float>(material.emissiveFactor[0]),
                static_cast<float>(material.emissiveFactor[1]),
                static_cast<float>(material.emissiveFactor[2]));
        }
        props.doubleSided  = material.doubleSided;
        props.alphaBlend   = (material.alphaMode == "BLEND");
        props.alphaMask    = (material.alphaMode == "MASK");
        props.alphaCutoff  = static_cast<float>(material.alphaCutoff);

        // Normal map (tangent-space)
        if (material.normalTexture.index > -1) {
            auto& normalTexRef = model.textures[material.normalTexture.index];
            if (normalTexRef.source >= 0 && normalTexRef.source < (int)cm->textures.size()) {
                cm->normalTextures[i]  = cm->textures[normalTexRef.source];
                props.normalScale      = static_cast<float>(material.normalTexture.scale);
                props.hasNormalMap     = true;
                printf("[PBR] Mesh %d: normal map source=%d scale=%.3f\n",
                       i, normalTexRef.source, props.normalScale);
            }
        }

        // Metallic-roughness map (glTF: G=roughness, B=metallic)
        if (material.pbrMetallicRoughness.metallicRoughnessTexture.index > -1) {
            auto& mrTexRef = model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index];
            if (mrTexRef.source >= 0 && mrTexRef.source < (int)cm->textures.size()) {
                cm->metallicRoughnessTextures[i]  = cm->textures[mrTexRef.source];
                props.hasMetallicRoughnessMap     = true;
                printf("[PBR] Mesh %d: metallic-roughness map source=%d metallic=%.3f roughness=%.3f\n",
                       i, mrTexRef.source, props.metallicFactor, props.roughnessFactor);
            }
        }

        // Emissive map
        if (material.emissiveTexture.index > -1) {
            auto& emTexRef = model.textures[material.emissiveTexture.index];
            if (emTexRef.source >= 0 && emTexRef.source < (int)cm->textures.size()) {
                cm->emissiveTextures[i]  = cm->textures[emTexRef.source];
                props.hasEmissiveMap     = true;
                printf("[PBR] Mesh %d: emissive map source=%d factor=(%.3f,%.3f,%.3f)\n",
                       i, emTexRef.source,
                       props.emissiveFactor.x, props.emissiveFactor.y, props.emissiveFactor.z);
            }
        }

        // Occlusion (AO) map
        if (material.occlusionTexture.index > -1) {
            auto& aoTexRef = model.textures[material.occlusionTexture.index];
            if (aoTexRef.source >= 0 && aoTexRef.source < (int)cm->textures.size()) {
                cm->occlusionTextures[i]     = cm->textures[aoTexRef.source];
                props.occlusionStrength      = static_cast<float>(material.occlusionTexture.strength);
                props.hasOcclusionMap        = true;
                printf("[PBR] Mesh %d: occlusion map source=%d strength=%.3f\n",
                       i, aoTexRef.source, props.occlusionStrength);
            }
        }
        // ---- end Blender-level PBR -----------------------------------------------
    }
    cm->skins = std::map<
        int,
        std::map<
            int, std::vector<float>
        >
    >();

    auto skinMaps = std::vector<std::map<int, std::vector<float>>>();
    int s = 0;
    // printf("Test point");
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
            cm->nodeNames[model.nodes[node].name] = node;
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

        if (i == -1) {
            auto skin = skinMaps[0];
            auto skinMap = std::map<int, std::vector<float>>();
            for (auto s : skin) {
                skinMap[s.first] = s.second;
            }
            cm->skins[0] = skinMap;
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
                // printf("%s (%d)->%s (%d)\n", model.nodes[child].name.c_str(), child, model.nodes[index].name.c_str(), index);
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

    float matrixData[16] = {
        modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2], modelMatrix[0][3],
        modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2], modelMatrix[1][3],
        modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2], modelMatrix[2][3],
        modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2], modelMatrix[3][3]
    };
    auto matrix = Matrix4(matrixData);

    cm->matrices.push_back(matrix);
    float identityData[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    cm->offsetMatrices.push_back(Matrix4(identityData));

    for (int count = 0; count < 4; count++) {
        cm->cameraParams.push_back(std::map<std::string, std::vector<float>>());
        cm->lightParams.push_back(std::map<std::string, std::vector<float>>());
        if (model.cameras.size() > 0) {
            auto camera = model.cameras[0];

            if (camera.type == "perspective") {
                cm->cameraParams[count]["fov"] = std::vector<float> { static_cast<float>(camera.perspective.yfov) };
                cm->cameraParams[count]["aspectRatio"] = std::vector<float> { static_cast<float>(camera.perspective.aspectRatio) };
                cm->cameraParams[count]["near"] = std::vector<float> { static_cast<float>(camera.perspective.znear) };
                cm->cameraParams[count]["far"] = std::vector<float> { static_cast<float>(camera.perspective.zfar) };
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
                    cm->cameraParams[count]["position"] = std::vector<float> { cameraPosition.x, cameraPosition.y, cameraPosition.z };
                    cm->cameraParams[count]["up"] = std::vector<float> { cameraUp.x, cameraUp.y, cameraUp.z };
                    cm->cameraParams[count]["target"] = std::vector<float> { cameraTarget.x, cameraTarget.y, cameraTarget.z };
                    break;
                }
            }
        } else {
            cm->cameraParams[count]["fov"] = std::vector<float> { 1920.0f / 1080.0f };
            cm->cameraParams[count]["aspectRatio"] = std::vector<float> { 1.33f };
            cm->cameraParams[count]["near"] = std::vector<float> { 0.01 };
            cm->cameraParams[count]["far"] = std::vector<float> { 1000.0f };

            glm::vec3 cameraPosition(0.0f);
            glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
            glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector

            glm::vec3 cameraTarget = cameraPosition + cameraDirection;
            cm->cameraParams[count]["position"] = std::vector<float> { cameraPosition.x, cameraPosition.y, cameraPosition.z };
            cm->cameraParams[count]["up"] = std::vector<float> { cameraUp.x, cameraUp.y, cameraUp.z };
            cm->cameraParams[count]["target"] = std::vector<float> { cameraTarget.x, cameraTarget.y, cameraTarget.z };
        }


        glm::vec3 lightPosition(0.0f);
        glm::vec3 lightDirection(0.0f, 0.0f, 1.0f); // Default direction

        cm->lightParams[count]["position"] = std::vector<float> { lightPosition.x, lightPosition.y, lightPosition.z };
        cm->lightParams[count]["color"] = std::vector<float> { 1.0f, 1.0f, 1.0f };
        cm->lightParams[count]["intensity"] = std::vector<float> { 1.0f };
        cm->lightParams[count]["direction"] = std::vector<float> { lightDirection.x, lightDirection.y, lightDirection.z };

        // Retrieve the light position
        for (auto node : model.nodes) {
            if (node.extensions.find("KHR_lights_punctual") != node.extensions.end()) {
                auto light = node.extensions["KHR_lights_punctual"];
                auto lightNode = model.lights[0];
                // glm::vec3 lightPosition(0.0f);
                if (!node.translation.empty()) {
                    lightPosition = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
                }
                cm->lightParams[count]["position"] = std::vector<float> { lightPosition.x, lightPosition.y, lightPosition.z };
                cm->lightParams[count]["color"] = std::vector<float> { static_cast<float>(lightNode.color[0]), static_cast<float>(lightNode.color[1]), static_cast<float>(lightNode.color[2]) };
                cm->lightParams[count]["intensity"] = std::vector<float> { static_cast<float>(lightNode.intensity/2500.0f) };

                if (lightNode.type == "directional") {
                    // glm::vec3 lightDirection(0.0f, -1.0f, 0.0f); // Default direction
                    if (!node.rotation.empty()) {
                        glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                        glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
                        lightDirection = glm::vec3(rotationMatrix * glm::vec4(lightDirection, 0.0f));
                    }
                    cm->lightParams[count]["direction"] = std::vector<float> { lightDirection.x, lightDirection.y, lightDirection.z };
                }
                break;
            }
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
    if (readyData == nullptr && cm->animations.empty()) {
        
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
            // printf("ANIM: %s", name.c_str());
            anims[name] = channels;
            anim++;
        }

        cm->animations = anims;

        for (const auto& animation : anims) {
            const std::string& animName = animation.first;
            if (cm->m_cachedAnimationDurations.find(animName) == cm->m_cachedAnimationDurations.end()) {
                float duration = cm->calculateAnimationDuration(animName);
                cm->m_cachedAnimationDurations[animName] = duration;
            }
        }

        // Write the animations to json file using rapidjson
        std::ofstream animFile("animations.json");
        rapidjson::Document animDoc;
        animDoc.SetObject();
        rapidjson::Document::AllocatorType& allocator = animDoc.GetAllocator();
        for (const auto& anim : anims) {
            rapidjson::Value animName(rapidjson::kStringType);
            animName.SetString(anim.first.c_str(), allocator);
            rapidjson::Value channels(rapidjson::kObjectType);
            for (const auto& channel : anim.second) {
                rapidjson::Value channelName(rapidjson::kStringType);
                channelName.SetString(channel.first.c_str(), allocator);
                rapidjson::Value nodes(rapidjson::kObjectType);
                for (const auto& node : channel.second) {
                    std::string nodeIndexStr = std::to_string(node.first);
                    rapidjson::Value nodeIndex(nodeIndexStr.c_str(), allocator);
                    rapidjson::Value keyframes(rapidjson::kArrayType);
                    for (const auto& keyframe : node.second) {
                        rapidjson::Value keyframeData(rapidjson::kObjectType);
                        keyframeData.AddMember("time", keyframe.first, allocator);
                        rapidjson::Value dataArray(rapidjson::kArrayType);
                        dataArray.PushBack(keyframe.second.x, allocator);
                        dataArray.PushBack(keyframe.second.y, allocator);
                        dataArray.PushBack(keyframe.second.z, allocator);
                        dataArray.PushBack(keyframe.second.w, allocator);
                        keyframeData.AddMember("data", dataArray, allocator); // Store full vec4
                        keyframes.PushBack(keyframeData, allocator);
                    }
                    nodes.AddMember(nodeIndex, keyframes, allocator);
                }
                channels.AddMember(channelName, nodes, allocator);
            }
            animDoc.AddMember(animName, channels, allocator);
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        animDoc.Accept(writer);
        animFile << buffer.GetString();
        
    } else {
        if (readyData != nullptr) {
            cm->animations = readyData->anims;
        }        
    }

    if (type == "triangles" && i >= 0) {
        auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_DYNAMIC;
        if (readyData != nullptr) {
            auto m = instance->newMesh(vf, readyData->prepD.data(), readyData->prepD.size() * sizeof(float), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
            printf("[DEBUG] Created mesh: %p with texture: %p\n", (void*)m, (void*)tex);
            printf("[MEMORY] Mesh created from readyData, current mesh count: %zu\n", cm->meshes.size() + 1);
            m->setTexture(tex);
            printf("[DEBUG] Texture set on mesh (readyData)\n");
                
            // GLuint vbo;
            // glGenBuffers(1, &vbo);
            
            // const void* vertexData = m->getVertexData();
            // size_t dataSize = m->getVertexCount() * m->getVertexStride();

            // glBindBuffer(GL_ARRAY_BUFFER, vbo);
            // glBufferData(GL_ARRAY_BUFFER, dataSize, vertexData, GL_STATIC_DRAW);
            
            // cm->cachedVBOs[i] = vbo;
            // cm->vboSizes[i] = dataSize;

            std::pair<gfx::Mesh*, chai_meshData*> p = std::pair<gfx::Mesh*, chai_meshData*>(m, readyData);
            return p;
        } else {
            auto m = instance->newMesh(vf, prepD.data(), prepD.size() * sizeof(float), gfx::PrimitiveType::PRIMITIVE_TRIANGLES, usage);
            printf("[DEBUG] Created mesh: %p with texture: %p\n", (void*)m, (void*)tex);
            printf("[MEMORY] Mesh created from new data, current mesh count: %zu\n", cm->meshes.size() + 1);
            m->setTexture(tex);
            printf("[DEBUG] Texture set on mesh (new data)\n");
            auto d = new chai_meshData(prepD, cm->animations);

            // GLuint vbo;
            // glGenBuffers(1, &vbo);
            
            // const void* vertexData = m->getVertexData();
            // size_t dataSize = m->getVertexCount() * m->getVertexStride();

            // glBindBuffer(GL_ARRAY_BUFFER, vbo);
            // glBufferData(GL_ARRAY_BUFFER, dataSize, vertexData, GL_STATIC_DRAW);

            // cm->cachedVBOs[i] = vbo;
            // cm->vboSizes[i] = dataSize;

            std::pair<gfx::Mesh*, chai_meshData*> p = std::pair<gfx::Mesh*, chai_meshData*>(m, d);
            return p;
        }
    }

    return std::pair<gfx::Mesh*, chai_meshData*>(nullptr, nullptr);
}

void chai_mesh::reloadMesh() {
    for (size_t i = 0; i < meshes.size(); i++) {
        auto mesh = meshes[i];
        auto data = mesh->getVertexData();
        size_t size = mesh->getVertexCount() * mesh->getVertexStride();
        auto usage = gfx::BufferDataUsage::BUFFERDATAUSAGE_DYNAMIC;
        auto type = gfx::PrimitiveType::PRIMITIVE_TRIANGLES;
        auto tex = mesh->getTexture();
        auto m = instance->newMesh(vf, (const void *)data, size, type, usage);
        m->setTexture(tex);
        meshes[i] = m;
    }
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

    delete img;
    img = NULL;

    Rect rect = Rect();
    rect.w = w;
    rect.h = h;

    auto tex = cm->instance->newTexture(settings, &slices);
    tex->replacePixels(copyOfPixelData, dataSize*4, 0, 0, rect, false);
    delete[] copyOfPixelData;  // Free allocated pixel buffer

    gfx::SamplerState sampler = gfx::SamplerState();

    sampler.wrapU = gfx::SamplerState::WrapMode::WRAP_REPEAT;
    sampler.wrapV = gfx::SamplerState::WrapMode::WRAP_REPEAT;
    sampler.wrapW = gfx::SamplerState::WrapMode::WRAP_REPEAT;

    tex->setSamplerState(sampler);

    cm->textures.emplace_back(tex);

    return true;
}

std::map<std::string, std::vector<float>> chai_mesh::getCameraParams(int index) {
    return this->cameraParams[index];
}

std::map<std::string, std::vector<float>> chai_mesh::getLightParams(int index) {
    return this->lightParams[index];
}

void chai_mesh::setLightParams(const std::map<std::string, std::vector<float>> &params, int index) {
    this->lightParams[index] = params;
}

std::vector<chai_meshData*> chai_mesh::loadMeshFromFile(const std::vector<chaiscript::Boxed_Value> &vertexFormat, const std::string *FileName, const std::string &type) {
    instance = Module::getInstance<gfx::Graphics>(Module::M_GRAPHICS);

    // ── Clear previously-loaded state ─────────────────────────────────────────
    // textures / meshes / subVisible are *appended to* in this function, so a
    // second call (e.g. scene reload) would otherwise double-accumulate data,
    // leak old GPU textures, and leave dangling pointers in PBR vectors.
    if (!textures.empty()) {
        for (auto t : textures) {
            if (t != nullptr) t->~Drawable();
        }
        textures.clear();
    }
    if (!meshes.empty()) {
        for (auto m : meshes) {
            if (m != nullptr) delete m;
        }
        meshes.clear();
    }
    subVisible.clear();
    normalTextures.clear();
    metallicRoughnessTextures.clear();
    emissiveTextures.clear();
    occlusionTextures.clear();
    materialPropsList.clear();
    // ─────────────────────────────────────────────────────────────────────────

    auto cl = ChaiLove::getInstance();
    auto f = cl->getFSModule();
    auto s = f.getSize(*FileName);
    auto file = f.readBuffer(*FileName, s);

    // Allocate a character array and copy the file data into it
    unsigned char* data = new unsigned char[s];
    std::memcpy(data, file, s);

    // Check for file with .anim extension
    std::string animFileName = *FileName + ".anim";
    if (f.exists(animFileName)) {
        auto animFileSize = f.getSize(animFileName);
        auto animFile = f.readBuffer(animFileName, animFileSize);
        // Load the animation data from the json file using rapidjson
        rapidjson::Document animDoc;
        animDoc.Parse(reinterpret_cast<const char*>(animFile), animFileSize);
        if (animDoc.HasParseError()) {
            std::cerr << "Error parsing animation file: " << animDoc.GetParseError() << std::endl;
        } else {
            // Process the animation data
            auto anims = std::map<std::string, std::map<std::string, std::map<int, std::vector<std::pair<float, glm::vec4>>>>>();
            if (animDoc.IsObject()) {
                for (auto animIt = animDoc.MemberBegin(); animIt != animDoc.MemberEnd(); ++animIt) {
                    std::string animName = animIt->name.GetString();
                    const rapidjson::Value& channelsObj = animIt->value;
                    std::map<std::string, std::map<int, std::vector<std::pair<float, glm::vec4>>>> channels;

                    for (auto chanIt = channelsObj.MemberBegin(); chanIt != channelsObj.MemberEnd(); ++chanIt) {
                        std::string channelName = chanIt->name.GetString();
                        const rapidjson::Value& nodesObj = chanIt->value;
                        std::map<int, std::vector<std::pair<float, glm::vec4>>> nodes;

                        for (auto nodeIt = nodesObj.MemberBegin(); nodeIt != nodesObj.MemberEnd(); ++nodeIt) {
                            int nodeIndex = std::stoi(nodeIt->name.GetString());
                            const rapidjson::Value& keyframesArr = nodeIt->value;
                            std::vector<std::pair<float, glm::vec4>> keyframes;

                            for (auto& keyframe : keyframesArr.GetArray()) {
                                float time = keyframe["time"].GetFloat();
                                glm::vec4 data(0.0f);
                                if (keyframe.HasMember("data") && keyframe["data"].IsArray() && keyframe["data"].Size() == 4) {
                                    data.x = keyframe["data"][0].GetFloat();
                                    data.y = keyframe["data"][1].GetFloat();
                                    data.z = keyframe["data"][2].GetFloat();
                                    data.w = keyframe["data"][3].GetFloat();
                                } else if (keyframe.HasMember("data") && keyframe["data"].IsNumber()) {
                                    // fallback for single float data
                                    data.x = keyframe["data"].GetFloat();
                                }
                                keyframes.emplace_back(time, data);
                            }
                            nodes[nodeIndex] = keyframes;
                        }
                        channels[channelName] = nodes;
                    }
                    anims[animName] = channels;
                }
            }
            this->animations = anims;
        }

    }

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // Set the LoadImageData callback
    loader.SetImageLoader(&LoadImageData, this);
    loader.LoadBinaryFromMemory(&model, &err, &warn, data, s, "", false);

    vf = vertexFormatLoader(vertexFormat);

    std::vector<chai_meshData*> meshData;

    specular = std::vector<bool>();

    for (size_t i = 0; i < model.meshes.size(); i++) {
        specular.push_back(false);
        if (this->data.size() > i) {
            auto d = loadMesh(i, model, instance, type, vf, this, this->data[i]);
            meshes.emplace_back(d.first);
            meshData.emplace_back(d.second);
        } else {
            auto d = loadMesh(i, model, instance, type, vf, this);
            meshes.emplace_back(d.first);
            meshData.emplace_back(d.second);
        } 
        subVisible.push_back(true);       
    }

    if (meshes.empty()) {
        auto d = loadMesh(-1, model, instance, type, vf, this);
        meshes.emplace_back(d.first);
        meshData.emplace_back(d.second);
        subVisible.push_back(true);
    }

    return meshData;
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

    delete img;
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
    delete[] copyOfPixelData;  // Free allocated pixel buffer
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

void chai_mesh::stopAnimations() {
    activeAnimations.clear();
}

bool chai_mesh::isAnimationPlaying(const std::string &name) {
    auto animation = activeAnimations.find(name);
    if (animation != activeAnimations.end()) {
        return true;
    }
    return false;
}

float chai_mesh::getAnimationPercent(const std::string &name) {
    auto animation = activeAnimations.find(name);
    if (animation == activeAnimations.end()) {
        return 0.0f;
    }
    
    float currentTime = animation->second.first;
    
    // Check if we have a cached duration for this animation
    auto cachedDuration = m_cachedAnimationDurations.find(name);
    float totalTime = 0.0f;
    
    if (cachedDuration != m_cachedAnimationDurations.end()) {
        // Use cached duration
        totalTime = cachedDuration->second;
    } else {
        // Calculate and cache the duration
        totalTime = calculateAnimationDuration(name);
        m_cachedAnimationDurations[name] = totalTime;
    }
    
    // Avoid division by zero
    if (totalTime <= 0.0f) {
        return 0.0f;
    }
    
    // Calculate percentage and clamp to [0, 1]
    float percent = currentTime / totalTime;
    return std::min(1.0f, std::max(0.0f, percent));
}

std::vector<int> getChildNodes(std::map<int, std::vector<int>> nodeChildren, int nodeChild, std::vector<int> nodes) {
    nodes.push_back(nodeChild);
    for (auto nc: nodeChildren[nodeChild]) {
        nodes = getChildNodes(nodeChildren, nc, nodes);
    }
    return nodes;
}

void chai_mesh::update(std::vector<float> position, std::vector<float> rotation, std::vector<float> scale, chai_debug *debug) {
    auto cc = ChaiLove::getInstance()->chai_collisions;
    scale = {1.0f, 1.0f, 1.0f};
    auto po = cc.getPhysicsObjects(id, scale);
    for (int i = 0; i < (int)po.size() && i < (int)offsetMatrices.size(); i++) {
        auto physicsObjectMatrix = po[i];
        if (debug) {
            debug->pushDebugMessagef("Replacing Object Matrix: %d\n", i);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", matrices[i].getColumn(0).x, matrices[i].getColumn(0).y, matrices[i].getColumn(0).z, matrices[i].getColumn(0).w);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", matrices[i].getColumn(1).x, matrices[i].getColumn(1).y, matrices[i].getColumn(1).z, matrices[i].getColumn(1).w);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", matrices[i].getColumn(2).x, matrices[i].getColumn(2).y, matrices[i].getColumn(2).z, matrices[i].getColumn(2).w);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", matrices[i].getColumn(3).x, matrices[i].getColumn(3).y, matrices[i].getColumn(3).z, matrices[i].getColumn(3).w);
        }

        // matrices[i] = matrices[i].inverse();
        glm::mat4 mat = glm::mat4(
            matrices[i].getColumn(0).x, matrices[i].getColumn(0).y, matrices[i].getColumn(0).z, matrices[i].getColumn(0).w,
            matrices[i].getColumn(1).x, matrices[i].getColumn(1).y, matrices[i].getColumn(1).z, matrices[i].getColumn(1).w,
            matrices[i].getColumn(2).x, matrices[i].getColumn(2).y, matrices[i].getColumn(2).z, matrices[i].getColumn(2).w,
            matrices[i].getColumn(3).x, matrices[i].getColumn(3).y, matrices[i].getColumn(3).z, matrices[i].getColumn(3).w
        );
        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(mat[0])); // length of first column
        scale.y = glm::length(glm::vec3(mat[1])); // length of second column
        scale.z = glm::length(glm::vec3(mat[2]));
        // glm::vec3 rot = glm::vec3(
        //     glm::degrees(atan2(matrix[1][2], matrix[2][2])), // Yaw
        //     glm::degrees(atan2(-matrix[0][2], sqrt(matrix[1][2] * matrix[1][2] + matrix[2][2] * matrix[2][2]))), // Pitch
        //     glm::degrees(atan2(matrix[0][1], matrix[0][0]))  // Roll
        // );
        // debug->pushDebugMessagef("Rotation: %f %f %f\n", rot[0], rot[1], rot[2]);
        // printf("Scale: %f %f %f\n", matrices[i].getColumn(0).w, matrices[i].getColumn(1).w, matrices[i].getColumn(2).w);

        // debug->pushDebugMessagef("Position: %f %f %f\n", position[0], position[1], position[2]);

        // Compose transformation: scale -> rotation -> translation


        // glm::mat3 rotation = glm::mat3(
        //     physicsObjectMatrix.getColumn(0).x, physicsObjectMatrix.getColumn(0).y, physicsObjectMatrix.getColumn(0).z,
        //     physicsObjectMatrix.getColumn(1).x, physicsObjectMatrix.getColumn(1).y, physicsObjectMatrix.getColumn(1).z,
        //     physicsObjectMatrix.getColumn(2).x, physicsObjectMatrix.getColumn(2).y, physicsObjectMatrix.getColumn(2).z
        // );

        // glm::mat3 matrix3 = glm::mat3(
        //     matrix[0][0], matrix[0][1], matrix[0][2],
        //     matrix[1][0], matrix[1][1], matrix[1][2],
        //     matrix[2][0], matrix[2][1], matrix[2][2]
        // );

        // rotation = rotation * matrix3; // Apply the transformation matrix to the physics matrix

        // glm::mat4 physMat;
        // physMat[0] = glm::vec4(
        //     rotation[0][0],
        //     rotation[0][1],
        //     rotation[0][2],
        //     physicsObjectMatrix.getColumn(0).w // Use the original scale from the physics object matrix
        // );
        // physMat[1] = glm::vec4(
        //     rotation[1][0],
        //     rotation[1][1],
        //     rotation[1][2],
        //     physicsObjectMatrix.getColumn(1).w // Use the original scale from the physics object matrix
        // );
        // physMat[2] = glm::vec4(
        //     rotation[2][0],
        //     rotation[2][1],
        //     rotation[2][2],
        //     physicsObjectMatrix.getColumn(2).w // Use the original scale from the physics object matrix
        // );
        // if (offsetMatrices[i].getColumn(0).x == 1.0f && offsetMatrices[i].getColumn(0).y == 0.0f && offsetMatrices[i].getColumn(0).z == 0.0f && offsetMatrices[i].getColumn(0).w == 0.0f &&
        //     offsetMatrices[i].getColumn(1).x == 0.0f && offsetMatrices[i].getColumn(1).y == 1.0f && offsetMatrices[i].getColumn(1).z == 0.0f && offsetMatrices[i].getColumn(1).w == 0.0f &&
        //     offsetMatrices[i].getColumn(2).x == 0.0f && offsetMatrices[i].getColumn(2).y == 0.0f && offsetMatrices[i].getColumn(2).z == 1.0f && offsetMatrices[i].getColumn(2).w == 0.0f &&
        //     offsetMatrices[i].getColumn(3).x == 0.0f && offsetMatrices[i].getColumn(3).y == 0.0f && offsetMatrices[i].getColumn(3).z == 0.0f && offsetMatrices[i].getColumn(3).w == 1.0f) {
            
            // matrices[i].setColumn(3, Vector4(physicsObjectMatrix.getColumn(3).x,
            //                                      physicsObjectMatrix.getColumn(3).y,
            //                                      physicsObjectMatrix.getColumn(3).z,
            //                                      1.0f));
        // }
        //     matrices[i].setColumn(3, Vector4(physicsObjectMatrix.getColumn(3).x,
        //                                          physicsObjectMatrix.getColumn(3).y,
        //                                          physicsObjectMatrix.getColumn(3).z,
        //                                          1.0f));
        // }
        // physMat[3] = glm::vec4(
        //     matrices[i].getColumn(3).x, // Scale the X position by 0.76 to match the original scale and add 1.0f for offset
        //     matrices[i].getColumn(3).y, // Scale the Y position by 0.4 to match the original scale
        //     matrices[i].getColumn(3).z, // Scale the Z position by 0.4 to match the original scale
        //     physicsObjectMatrix.getColumn(3).w
        // );

        // printf("physicsObjectMatrix: %f\n", physicsObjectMatrix.getColumn(3).y);
        // printf("matrices[i]: %f\n", matrices[i].getColumn(3).y);

        // Conversion matrix: swap Y and Z axes (Z-up <-> Y-up)
        // glm::mat4 convert = glm::mat4(
        //     1, 0, 0, 0,
        //     0, 0, 1, 0,
        //     0, 1, 0, 0,
        //     0, 0, 0, 1
        // );

        // physMat = glm::scale(physMat, glm::vec3(0.8f, 0.8f, 0.8f)); // Scale the physics matrix to match the original scale
        
        // Extract physics position
        glm::vec3 physicsPos(
            physicsObjectMatrix.getColumn(3).x,
            physicsObjectMatrix.getColumn(3).y,
            physicsObjectMatrix.getColumn(3).z
        );

        // Extract render/model position
        glm::vec3 renderPos(
            matrices[i].getColumn(3).x,
            matrices[i].getColumn(3).y,
            matrices[i].getColumn(3).z
        );
        // renderPos[0] -= renderPos[0] * 0.3f;
        // renderPos[1] += renderPos[1] * 0.4f;
        // renderPos[2] -= renderPos[2] * 0.3f;
        auto p = glm::vec3(position[0], position[1], position[2]);
        auto hp = glm::vec3(position[0]/2.0f, position[1]/2.0f, position[2]/2.0f);
        // Calculate offset
        glm::vec3 offset = physicsPos;

        auto physMat = glm::mat4(
            physicsObjectMatrix.getColumn(0).x, physicsObjectMatrix.getColumn(0).y, physicsObjectMatrix.getColumn(0).z, physicsObjectMatrix.getColumn(0).w,
            physicsObjectMatrix.getColumn(1).x, physicsObjectMatrix.getColumn(1).y, physicsObjectMatrix.getColumn(1).z, physicsObjectMatrix.getColumn(1).w,
            physicsObjectMatrix.getColumn(2).x, physicsObjectMatrix.getColumn(2).y, physicsObjectMatrix.getColumn(2).z, physicsObjectMatrix.getColumn(2).w,
            offset.x, offset.y, offset.z, physicsObjectMatrix.getColumn(3).w
        );

        physMat = glm::scale(physMat, scale);



        // physMat = matrix * physMat;
        // physMat = glm::scale(physMat, glm::vec3(0.8f, 0.8f, 0.8f));
       
          // To convert Bullet's Y-up to model's Z-up:
        // physMat = physMat * convert;

        // physMat = glm::rotate(physMat, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        // Get the current position and rotation from the physics object
        // glm::vec3 currentPosition = glm::vec3(physMat[3][0]/0.4f, physMat[3][2]/0.4f, physMat[3][1]/-0.4f); // Scale the position by 1.6 to match the original scale
        // glm::vec3 currentRotation = glm::vec3(
        //     glm::degrees(atan2(physMat[1][2], physMat[2][2])), // Yaw
        //     glm::degrees(atan2(-physMat[0][2], sqrt(physMat[1][2] * physMat[1][2] + physMat[2][2] * physMat[2][2]))), // Pitch
        //     glm::degrees(atan2(physMat[0][1], physMat[0][0]))  // Roll
        // );
        // debug->pushDebugMessagef("Phys Rotation: %f %f %f\n", currentRotation[0], currentRotation[1], currentRotation[2]);
        // auto temp = currentRotation[2];
        // currentRotation[1] = currentRotation[1] + 180.0f;
        // currentRotation[1] = temp + 180.0f;
        // currentRotation[0] = currentRotation[0] * -1.0f;
        // current position to mat4
        // glm::mat4 currentPositionMat = glm::translate(glm::mat4(1.0f), currentPosition);
        // debug->pushDebugMessagef("Phys Position: %f %f %f\n", currentPositionMat[3][0], currentPositionMat[3][1], currentPositionMat[3][2]);
        // current rotation to mat4
        // glm::quat currentQuat = glm::quat(glm::vec3(glm::radians(currentRotation[0]), glm::radians(currentRotation[2]), glm::radians(currentRotation[1])));
        // glm::mat4 currentRotationMat = glm::mat4_cast(currentQuat);
        // Rotate Y by 180 degrees
        // currentRotationMat = glm::rotate(currentRotationMat, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 1.0f));
        // current scale to mat4
        // glm::mat4 currentScaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(physMat[0][0], physMat[1][1], physMat[2][2]));
        // debug->pushDebugMessagef("Phys Scale: %f %f %f\n", currentScaleMat[0][0], currentScaleMat[1][1], currentScaleMat[2][2]);


        // Compose transformation: scale -> rotation -> translation
        // glm::mat4 scaleMat2 = glm::scale(glm::mat4(1.0f), glm::vec3(scale[0], scale[1], scale[2]));
        // glm::quat q2 = glm::quat(glm::vec3(rotation[0], rotation[1], rotation[2])); // Pitch, Yaw, Roll (X, Y, Z)
        // glm::mat4 rotMat2 = glm::mat4_cast(q2);
        // auto o0 = position[0];
        // auto o1 = position[1];
        // auto o2 = position[2];
        // auto p0 = currentPosition[0];
        // auto p1 = currentPosition[1];
        // auto p2 = currentPosition[2];
        // auto b0 = matrix[3][0];
        // auto b1 = matrix[3][1];
        // auto b2 = matrix[3][2];
        // debug->pushDebugMessagef("Calc: %f \n", p1-o1-b1);
        // glm::vec3 translate = glm::vec3((o0-p0), (o1-p1), (o2-p2));
        // debug->pushDebugMessagef("Translate: %f %f %f\n", translate[0], translate[1], translate[2]);
        // glm::mat4 modelMat = glm::translate(matrix, translate); // Adjust for Z-up to Y-up conversion
        // // Rotate the model matrix by the current rotation
        // modelMat = currentRotationMat * modelMat; // Apply the current rotation

        // Compose: physics * user translation * user rotation * user scale
        // glm::mat4 modelMat = transMat2 * matrix;

        // Matrix4 m = Matrix4(new float[16] {
        //     modelMat[0][0], modelMat[0][1], modelMat[0][2], modelMat[0][3],
        //     modelMat[1][0], modelMat[1][1], modelMat[1][2], modelMat[1][3],
        //     modelMat[2][0], modelMat[2][1], modelMat[2][2], modelMat[2][3],
        //     translate[0], translate[1], translate[2], modelMat[3][3]
        // });

        // physMat = glm::translate(physMat, glm::vec3(0.0f, 1.0f, 0.0f)); // Scale the physics matrix to match the original scale
 
        // matrix = glm::scale(matrix, glm::vec3(0.4f, 0.4f, 0.4f)); // Scale the model matrix to match the original scale

        auto diff = physMat;
        float diffData[16] = {
            diff[0][0], diff[0][1], diff[0][2], diff[0][3],
            diff[1][0], diff[1][1], diff[1][2], diff[1][3],
            diff[2][0], diff[2][1], diff[2][2], diff[2][3],
            diff[3][0], diff[3][1], diff[3][2], diff[3][3]
        };
        offsetMatrices[i] = Matrix4(diffData);

        if (debug) {
            debug->visualizeMatrix(physMat);

            debug->pushDebugMessagef("New Object Matrix: %d\n", i);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", offsetMatrices[i].getColumn(0).x, offsetMatrices[i].getColumn(0).y, offsetMatrices[i].getColumn(0).z, offsetMatrices[i].getColumn(0).w);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", offsetMatrices[i].getColumn(1).x, offsetMatrices[i].getColumn(1).y, offsetMatrices[i].getColumn(1).z, offsetMatrices[i].getColumn(1).w);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", offsetMatrices[i].getColumn(2).x, offsetMatrices[i].getColumn(2).y, offsetMatrices[i].getColumn(2).z, offsetMatrices[i].getColumn(2).w);
            debug->pushDebugMessagef("Matrix: %f %f %f %f\n", offsetMatrices[i].getColumn(3).x, offsetMatrices[i].getColumn(3).y, offsetMatrices[i].getColumn(3).z, offsetMatrices[i].getColumn(3).w);
            // debug->displayDebugMessages();
            // ChaiLove::getInstance()->script->debugbreak();
        }
    }

    // If there's only one physics object but multiple offsetMatrices (sub-meshes),
    // apply the physics transform to all sub-mesh offsets
    if (po.size() == 1 && offsetMatrices.size() > 1) {
        // printf("[DEBUG chai_mesh::update] Applying single physics transform to all %zu sub-meshes\n", offsetMatrices.size());
        const Matrix4& physicsTransform = offsetMatrices[0];
        for (size_t j = 1; j < offsetMatrices.size(); j++) {
            offsetMatrices[j] = physicsTransform;
        }
    }

}

// void chai_mesh::preloadAnimations() {
//     std::map<std::string, std::map<int, glm::mat4>> animationsMatrices;
//     for (const auto& animPair : animations) {
//         int i = 0;
//         const std::string& name = animPair.first;
//         // Calculate and cache the matrix data for the animation
//         if (jointList.size() > i) {                    
//             jointList[i] = std::vector<int>();
//             jointMatrix[i] = std::map<int, glm::mat4>();
//         } else {
//             jointList.push_back(std::vector<int>());
//             jointMatrix.push_back(std::map<int, glm::mat4>());
//         }
//         auto jointIBMatrix = std::map<int, glm::mat4>();

//         if (jointOrder.find(i) != jointOrder.end()) {
//             for (auto joint : jointOrder[i]) {
//                 auto matrix = skins[i][joint];
//                 jointIBMatrix[joint] = glm::mat4(
//                     matrix[0], matrix[1], matrix[2], matrix[3],
//                     matrix[4], matrix[5], matrix[6], matrix[7],
//                     matrix[8], matrix[9], matrix[10], matrix[11],
//                     matrix[12], matrix[13], matrix[14], matrix[15]
//                 );
//                 jointMatrix[i][joint] = glm::mat4(1.0);
//                 jointList[i].push_back(joint);

//                 nodeActiveMatrix[joint] = glm::inverse(jointIBMatrix[joint]);
//             }
//         }

//         auto animPlaying = -1;
//         int nodeIndex = -1;

//         for (const auto& channel : animPair.second) {           
//             auto chan = channel.first;
//             for (const auto& keyframe : channel.second) {
//                 nodeIndex = keyframe.first;
//                 auto animTime = keyframe.second.back().first;
                

//                 if (jointMatrix[i].find(nodeIndex) == jointMatrix[i].end()) {
//                     continue;
//                 }
//                 const auto& keyframes = keyframe.second;

//                 // Find the two keyframes to interpolate between
//                 auto it = std::lower_bound(keyframes.begin(), keyframes.end(), animTime,
//                     [](const std::pair<float, glm::vec4>& a, float b) {
//                         return a.first < b;
//                     });

//                 if (it == keyframes.end()) {
//                     // Use the last keyframe if the current time is beyond the last keyframe
//                     it = keyframes.end() - 1;
//                 }

//                 auto nextIt = it + 1;
//                 if (nextIt == keyframes.end()) {
//                     nextIt = it;
//                 }

//                 float t1 = it->first;
//                 float t2 = nextIt->first;

//                 const glm::vec4& v1 = it->second;
//                 const glm::vec4& v2 = nextIt->second;

//                 // Interpolate between the two keyframes
//                 float t = (t1 - animTime) / (t2 - t1);
//                 glm::vec4 interpolatedValue = glm::vec4(1.0);
//                 if (chan != "rotation") {
//                     interpolatedValue = glm::mix(v1, v2, t);
//                 }
                
//                 if (animPlaying == -1) {
//                     animPlaying = 0;
//                 }

//                 if (glm::all(glm::isnan(interpolatedValue)) == false) {
//                     auto nodes = std::vector<int>();
//                     nodes.push_back(nodeIndex);
//                     int parent = -1;
//                     int root = -1;
//                     int subroot = -1;
//                     auto offset = glm::mat4(1.0f);
//                     auto hierarchy = std::vector<int>();
//                     int np = nodeIndex;

//                     // Initialize the hierarchy vector with np
//                     hierarchy.push_back(np);

//                     // Traverse the nodeParent pairs to find the parent of the current node
//                     while (true) {
//                         bool found = false;
//                         for (const auto& pair : nodeParent) {
//                             if (pair.first == np) {   
//                                 subroot = np;                                         
//                                 np = pair.second;
//                                 hierarchy.push_back(np);
//                                 found = true;
//                                 break;
//                             }
//                         }
//                         if (!found) {
//                             root = np;
//                             // root = 61;
//                             // subroot = np; // Set subroot to the last node in hierarchy
//                             // root = 0; // Set root to the current nodeIndex
//                             break; // Reached the root node
//                         }
//                     }

//                     // subroot = 61;
                    
//                     for (auto node : nodeChildren) {
//                         for (auto n : node.second) {
//                             if (n == nodeIndex) {
//                                 parent = node.first;
//                                 break;
//                             }
                            
//                         }
//                         if (parent != -1) {
//                             break;
//                         }
                        
//                     }
//                     for (auto nodeChild : nodeChildren[nodeIndex]) {
//                         nodes = getChildNodes(nodeChildren, nodeChild, nodes);
//                     }
//                     for (auto node : nodes) {
//                         if (false && node == root) {
//                             glm::mat4 trs = jointMatrix[i][node];

//                             // Apply translation
//                             if (chan == "translation") {
//                                 animPlaying = 1;
//                                 printf("NodeIndex: %d\n", nodeIndex);
//                                 printf("AnimTime: %f\n", animTime);
//                                 printf("t1: %f, t2: %f, t: %f\n", t1, t2, t);
//                                 printf("v1: %f %f %f %f\n", v1.x, v1.y, v1.z, v1.w);
//                                 printf("v2: %f %f %f %f\n", v2.x, v2.y, v2.z, v2.w);
//                                 printf("Translate: %d %f %f %f\n", node, interpolatedValue.x, interpolatedValue.y, interpolatedValue.z);
//                                 jointMatrix[i][node] = glm::translate(glm::mat4(1.0f), glm::vec3(interpolatedValue.w, interpolatedValue.z, interpolatedValue.y)) * jointMatrix[i][node];
//                             }

//                             // Apply rotation
//                             if (chan == "rotation" && t <= 1.0f && t >= 0.0f) {
//                                 // glm::quat rotation = glm::slerp(glm::quat(v2.w, v2.x, v2.y, v2.z), glm::quat(v1.w, v1.x, v1.y, v1.z), t);
//                                 // trs *= glm::mat4_cast(rotation);
//                             }

//                             // Apply scale
//                             if (chan == "scale") {
//                                 // glm::vec3 v(interpolatedValue);
//                                 // trs = glm::scale(trs, v);
//                             }

//                             // Combine with inverse bind matrix if needed
//                             // jointMatrix[i][node] = trs;
//                         } else if (true) {
//                             if (chan == "translation") {

//                                 animPlaying = 1;
//                                 jointMatrix[i][node] = glm::translate(glm::mat4(1.0f), glm::vec3(interpolatedValue.x, interpolatedValue.z, interpolatedValue.y)) * jointMatrix[i][node];
//                                 // glm::vec3 v(interpolatedValue);
//                                 // jointMatrix[i][node] = glm::translate(jointMatrix[i][node], v) * jointMatrix[i][node];
//                                 // printf("Translate: %d %f %f %f\n", node, interpolatedValue.x, interpolatedValue.y, interpolatedValue.z);
//                             } else if (chan == "rotation" && t <= 1.0f && t >= 0.0f) {
//                                 animPlaying = 1;
//                                 glm::quat rotation = glm::quat(1.0f, 1.0f, 1.0f, 1.0f);
//                                 auto bindMatrix = glm::inverse(jointIBMatrix[node]);
//                                 if (node == nodes[0]) {
//                                     rotation = glm::slerp(glm::quat(v2.w, v2.x, v2.y, v2.z), glm::quat(v1.w, v1.x, v1.y, v1.z), t);
//                                     auto parentMatrix = glm::mat4(1.0f);
//                                     if (true || parent != root) {
//                                         parentMatrix = jointIBMatrix[root];
//                                     }
                                    
//                                     auto invMatrix = glm::inverse(nodeMatrix[node]);
                                    
//                                     auto hierarchyRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
//                                     auto hierarchyRotation2 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
//                                     for (auto h = hierarchy.rbegin(); h != hierarchy.rend(); ++h) {
//                                         if (node == subroot) {
//                                             hierarchyRotation = hierarchyRotation * glm::quat_cast(jointMatrix[i][*h]);
//                                             hierarchyRotation2 = hierarchyRotation2 * glm::quat_cast(glm::inverse(nodeMatrix[*h]));
//                                         } else {
//                                             if (*h == node) {
//                                                 hierarchyRotation = hierarchyRotation * glm::quat_cast(jointMatrix[i][*h]);
//                                                 hierarchyRotation2 = hierarchyRotation2 * glm::quat_cast(glm::inverse(nodeMatrix[*h]));
//                                             }
//                                         }
//                                     }

//                                     rotation = hierarchyRotation * rotation;
//                                     rotation = hierarchyRotation2 * rotation;
                                                                                    
//                                     rotation = glm::normalize(rotation);
                                    
//                                     jointMatrix[i][node] = bindMatrix * glm::mat4_cast(rotation) * jointIBMatrix[node] * jointMatrix[i][node];
//                                     parent = node;
//                                 } else {
//                                     // Child node
//                                     jointMatrix[i][node] = jointMatrix[i][parent] * jointMatrix[i][node];
//                                 }
//                             } else if (chan == "scale") {
//                                 animPlaying = 1;
//                                 glm::vec3 v(interpolatedValue);
//                                 jointMatrix[i][node] = glm::scale(jointMatrix[i][node], v) * jointMatrix[i][node];
//                             }

//                             nodeActiveMatrix[node] = jointMatrix[i][node] * glm::inverse(jointIBMatrix[node]); 
//                         }
//                         animationsMatrices[name][node] = nodeActiveMatrix[node];
//                     }
//                 }              
//             }            
//         }
//     }
//     std::printf("Preloaded Animations Matrices\n");
// }

void chai_mesh::draw(love::gfx::Graphics *gfx, const Matrix4 &m, chai_shader *shader, float dt, chai_shader *computeShader, bool shadows) {
    
    if (dt > 0.0f)
        frameCount++;
    
    int animRate = 6; // Adjust this value to change animation update frequency

    nodeActiveMatrix.clear();
    
    // Clear animation cache to prevent unbounded memory growth
    animationFrameMatrixCache.clear();

    if (m_modelMatrixCacheRaw.size() != 16) {
        m_modelMatrixCacheRaw.resize(16, 0.0f);
    }
    
    if (m_isSpecularCache0.empty()) {
        m_isSpecularCache0.push_back(0);
    }
    
    if (m_isSpecularCache1.empty()) {
        m_isSpecularCache1.push_back(1);
    }
    
    if (m_jointInfoCache.size() != 1) {
        m_jointInfoCache.resize(1);
    }

    if (mesh != nullptr) {
        mesh->draw(gfx, m);
    } else {
        // preloadAnimations();
        for (int i = 0; i < meshes.size(); i++) {
            // Bounds check: ensure i is valid for all parallel arrays
            if (i < 0 || i >= 1024) {
                fprintf(stderr, "[MESH] Invalid mesh index %d, skipping\n", i);
                continue;
            }
            
            auto msh = meshes[i];

            if (subVisible[i] == false && msh != nullptr) {
                continue;
            }

            // Bounds check to prevent crash
            // if (i >= meshToNode.size()) {
            //     continue;
            // }

            // auto node = meshToNode[i];
            auto jointIBMatrix = std::map<int, glm::mat4>();

            if (frameCount % animRate == 0 || frameCount < 3) {
                if (jointList.size() > i) {                    
                    jointList[i].clear();
                    jointMatrix[i].clear();
                } else {
                    jointList.push_back(std::vector<int>());
                    jointMatrix.push_back(std::map<int, glm::mat4>());
                }  
            
                if (jointOrder.find(i) != jointOrder.end()) {
                    for (auto joint : jointOrder[i]) {
                        // Safety check: ensure skin data exists and has correct size
                        if (skins.find(i) == skins.end() || skins[i].find(joint) == skins[i].end()) {
                            continue;
                        }
                        auto& matrix = skins[i][joint];
                        if (matrix.size() < 16) {
                            continue;
                        }
                        
                        jointIBMatrix[joint] = glm::mat4(
                            matrix[0], matrix[1], matrix[2], matrix[3],
                            matrix[4], matrix[5], matrix[6], matrix[7],
                            matrix[8], matrix[9], matrix[10], matrix[11],
                            matrix[12], matrix[13], matrix[14], matrix[15]
                        );
                        jointMatrix[i][joint] = glm::mat4(1.0);
                        jointList[i].push_back(joint);

                        nodeActiveMatrix[joint] = glm::inverse(jointIBMatrix[joint]);
                    }
                } else {
                    // static int noJointsCount = 0;
                    // if (++noJointsCount % 100 == 0) {
                    //     fprintf(stderr, "[JOINT DEBUG] Mesh %d has no jointOrder entry (no skeletal data) - called %d times\n", i, noJointsCount);
                    //     fflush(stderr);
                    // }
                }
            }

            // Wheels are rigged by armature to chassis - no physics override needed

            // Process animations every frame (removed currentTime != dt condition)
            // Since we clear jointMatrix every frame, we must process animations every frame too
            {
                size_t animLeakBefore = 0, animLeakAfter = 0;
                uint64_t animSizeBefore = 0, animSizeAfter = 0;
                
                // if (frameCount % 300 == 0) {
                //     __mem_leak_check(animLeakBefore, animSizeBefore, false, "", false);
                // }
                
                auto animTime = 0.1f;
                auto loop = false;
                std::string name = "";
                std::map<std::string, bool> eraseAnimations;

                if (frameCount % animRate == 0 || frameCount < 3) {
                    std::vector<float> packedData;
                    // if (msh == nullptr && jointList[i].size() > 0) {    
                    //     packedData.push_back((float)jointList[i].size()); // Placeholder for alignment
                    //     packedData.push_back(0.0f); // Placeholder for alignment
                    //     packedData.push_back(0.0f); // Placeholder for alignment
                    //     packedData.push_back(0.0f); // Placeholder for alignment
                    // }
                    for (auto activeAnimation : activeAnimations) {
                        name = activeAnimation.first;
                        animTime = activeAnimation.second.first;
                        // animTime = 2.5f;
                        // animTime = 0.1f;
                        loop = activeAnimation.second.second;

                        auto animPlaying = -1;

                        auto animation = animations[name];
                        int nodeIndex = -1;

                        if (false) {
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
                                                // root = 61;
                                                // subroot = np; // Set subroot to the last node in hierarchy
                                                // root = 0; // Set root to the current nodeIndex
                                                break; // Reached the root node
                                            }
                                        }

                                        // subroot = 61;
                                        
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
                                            animPlaying = 1;
                                            packedData[3] = 1.0f; // Indicate that there is animation data
                                            packedData.push_back(static_cast<float>(node));
                                            packedData.push_back(static_cast<float>(chan == "translation" ? 0 : (chan == "rotation" ? 1 : 2)));
                                            packedData.push_back(t1);
                                            packedData.push_back(t2);
                                            packedData.push_back(t);
                                            packedData.push_back(v1.x);
                                            packedData.push_back(v1.y);
                                            packedData.push_back(v1.z);
                                            packedData.push_back(v1.w);
                                            packedData.push_back(v2.x);
                                            packedData.push_back(v2.y);
                                            packedData.push_back(v2.z);
                                            packedData.push_back(v2.w);
                                            packedData.push_back(interpolatedValue.x);
                                            packedData.push_back(interpolatedValue.y);
                                            packedData.push_back(interpolatedValue.z);
                                            packedData.push_back(interpolatedValue.w);
                                            packedData.push_back(jointMatrix[i][nodeIndex][0][0]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][0][1]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][0][2]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][0][3]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][1][0]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][1][1]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][1][2]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][1][3]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][2][0]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][2][1]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][2][2]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][2][3]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][3][0]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][3][1]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][3][2]);
                                            packedData.push_back(jointMatrix[i][nodeIndex][3][3]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][0][0]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][0][1]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][0][2]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][0][3]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][1][0]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][1][1]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][1][2]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][1][3]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][2][0]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][2][1]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][2][2]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][2][3]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][3][0]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][3][1]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][3][2]);
                                            packedData.push_back(jointIBMatrix[nodeIndex][3][3]);
                                        }
                                    }
                                }
                            } 
                        }
                                           

                        
                        // auto info = computeShader->shader->getUniformInfo("InputBuffer");
                        // gfx::Buffer *buffer = gfx->getDefaultStorageBuffer();
                        // computeShader->shader->sendBuffers(info, &buffer, 1);

                        if (true) {
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

                                    // Animation caching completely disabled to prevent memory leak
                                    // The cache was growing unbounded with unique float keys every frame
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
                                                // root = 61;
                                                // subroot = np; // Set subroot to the last node in hierarchy
                                                // root = 0; // Set root to the current nodeIndex
                                                break; // Reached the root node
                                            }
                                        }

                                        // subroot = 61;
                                        
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
                                            if (false && node == root) {
                                                glm::mat4 trs = jointMatrix[i][node];

                                                // Apply translation
                                                if (chan == "translation") {
                                                    animPlaying = 1;
                                                    printf("NodeIndex: %d\n", nodeIndex);
                                                    printf("AnimTime: %f\n", animTime);
                                                    printf("t1: %f, t2: %f, t: %f\n", t1, t2, t);
                                                    printf("v1: %f %f %f %f\n", v1.x, v1.y, v1.z, v1.w);
                                                    printf("v2: %f %f %f %f\n", v2.x, v2.y, v2.z, v2.w);
                                                    printf("Translate: %d %f %f %f\n", node, interpolatedValue.x, interpolatedValue.y, interpolatedValue.z);
                                                    jointMatrix[i][node] = glm::translate(glm::mat4(1.0f), glm::vec3(interpolatedValue.w, interpolatedValue.z, interpolatedValue.y)) * jointMatrix[i][node];
                                                }

                                                // Apply rotation
                                                if (chan == "rotation" && t <= 1.0f && t >= 0.0f) {
                                                    // glm::quat rotation = glm::slerp(glm::quat(v2.w, v2.x, v2.y, v2.z), glm::quat(v1.w, v1.x, v1.y, v1.z), t);
                                                    // trs *= glm::mat4_cast(rotation);
                                                }

                                                // Apply scale
                                                if (chan == "scale") {
                                                    // glm::vec3 v(interpolatedValue);
                                                    // trs = glm::scale(trs, v);
                                                }

                                                // Combine with inverse bind matrix if needed
                                                // jointMatrix[i][node] = trs;
                                            } else if (true) {
                                                if (chan == "translation") {

                                                    animPlaying = 1;
                                                    jointMatrix[i][node] = glm::translate(glm::mat4(1.0f), glm::vec3(interpolatedValue.x, interpolatedValue.z, interpolatedValue.y)) * jointMatrix[i][node];
                                                    // glm::vec3 v(interpolatedValue);
                                                    // jointMatrix[i][node] = glm::translate(jointMatrix[i][node], v) * jointMatrix[i][node];
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

                                                nodeActiveMatrix[node] = jointMatrix[i][node] * glm::inverse(jointIBMatrix[node]);
                                                // Disable cache insertion to prevent memory leak
                                                // jointCache[node] = jointMatrix[i][node];
                                            }
                                        }
                                    } 
                                }
                            }
                        }   
                        if (animPlaying == 1) {
                            activeAnimations[name].first += dt * animRate; // Increment animation time
                        } else if (animPlaying == 0) {
                            if (loop) {
                                activeAnimations[name].first = 0.1f;
                            } else {
                                eraseAnimations[name] = true;
                            }
                        }
                    }
                    // if (msh == nullptr && jointList[i].size() > 0) {
                    //     auto offset = computeShader->shader->updateBuffer("InputBuffer", packedData.data(),
                    //                                     packedData.size() * sizeof(float), 0);
                    // }
                }

                // if (activeAnimations.size() == 0) {
                //     gfx->dispatchThreadgroups(computeShader->shader, 1, 1, 1);
                // }
                
                

                for (auto eraseAnimation : eraseAnimations) {
                    activeAnimations.erase(eraseAnimation.first);
                }
                
                // if (frameCount % 300 == 0) {
                //     __mem_leak_check(animLeakAfter, animSizeAfter, false, "", false);
                //     if (animLeakAfter > animLeakBefore) {
                //         printf("  [ANIMATION LOOP] Leaked in mesh %d: %zu objects, %llu bytes\n",
                //                i, animLeakAfter - animLeakBefore, animSizeAfter - animSizeBefore);
                //         printf("    activeAnimations size: %zu\n", activeAnimations.size());
                //     }
                // }
            }
            // End of animation processing block

            // auto tempMat = matrices[i] * m;
            // printf("Matrix: %f %f %f %f\n", tempMat.getColumn(0).x, tempMat.getColumn(0).y, tempMat.getColumn(0).z, tempMat.getColumn(0).w);
            // printf("Matrix: %f %f %f %f\n", tempMat.getColumn(1).x, tempMat.getColumn(1).y, tempMat.getColumn(1).z, tempMat.getColumn(1).w);
            // printf("Matrix: %f %f %f %f\n", tempMat.getColumn(2).x, tempMat.getColumn(2).y, tempMat.getColumn(2).z, tempMat.getColumn(2).w);
            // printf("Matrix: %f %f %f %f\n", tempMat.getColumn(3).x, tempMat.getColumn(3).y, tempMat.getColumn(3).z, tempMat.getColumn(3).w);
            
            // Check if offsetMatrix is identity
            bool isIdentity = (offsetMatrices[i].getColumn(0).x == 1.0f && offsetMatrices[i].getColumn(0).y == 0.0f && offsetMatrices[i].getColumn(0).z == 0.0f && offsetMatrices[i].getColumn(0).w == 0.0f &&
                offsetMatrices[i].getColumn(1).x == 0.0f && offsetMatrices[i].getColumn(1).y == 1.0f && offsetMatrices[i].getColumn(1).z == 0.0f && offsetMatrices[i].getColumn(1).w == 0.0f &&
                offsetMatrices[i].getColumn(2).x == 0.0f && offsetMatrices[i].getColumn(2).y == 0.0f && offsetMatrices[i].getColumn(2).z == 1.0f && offsetMatrices[i].getColumn(2).w == 0.0f &&
                offsetMatrices[i].getColumn(3).x == 0.0f && offsetMatrices[i].getColumn(3).y == 0.0f && offsetMatrices[i].getColumn(3).z == 0.0f && offsetMatrices[i].getColumn(3).w == 1.0f);
            
            auto mat = offsetMatrices[i];
            if (isIdentity) {
                // Identity offset - use static model matrix
                mat = matrices[i] * m;
            } else {
                // Physics-updated offset - compose with transform
                mat = offsetMatrices[i] * m;
            }

            // Add a small random offset to the translation z component (assuming mat is Matrix4)
            auto col = mat.getColumn(3);
            // col.z += std::rand() % 1000 * 0.001f * 0.01f;
            mat.setColumn(3, col);
            
            activeMatrix = mat;
            // printf("Matrix: %f %f %f %f\n", mat.getColumn(0).x, mat.getColumn(0).y, mat.getColumn(0).z, mat.getColumn(0).w);
            // printf("Matrix: %f %f %f %f\n", mat.getColumn(1).x, mat.getColumn(1).y, mat.getColumn(1).z, mat.getColumn(1).w);
            // printf("Matrix: %f %f %f %f\n", mat.getColumn(2).x, mat.getColumn(2).y, mat.getColumn(2).z, mat.getColumn(2).w);
            // printf("Matrix: %f %f %f %f\n", mat.getColumn(3).x, mat.getColumn(3).y, mat.getColumn(3).z, mat.getColumn(3).w);
            
            int idx = 0;
            for (int c = 0; c < 4; ++c) {
                m_modelMatrixCacheRaw[idx++] = mat.getColumn(c).x;
                m_modelMatrixCacheRaw[idx++] = mat.getColumn(c).y;
                m_modelMatrixCacheRaw[idx++] = mat.getColumn(c).z;
                m_modelMatrixCacheRaw[idx++] = mat.getColumn(c).w;
            }
            int modelIdx = -1;
            {
                // Build glm::mat4 from the raw float array
                glm::mat4 modelMat = glm::mat4(
                    mat.getColumn(0).x, mat.getColumn(0).y, mat.getColumn(0).z, mat.getColumn(0).w,
                    mat.getColumn(1).x, mat.getColumn(1).y, mat.getColumn(1).z, mat.getColumn(1).w,
                    mat.getColumn(2).x, mat.getColumn(2).y, mat.getColumn(2).z, mat.getColumn(2).w,
                    mat.getColumn(3).x, mat.getColumn(3).y, mat.getColumn(3).z, mat.getColumn(3).w
                );
                m_matrixCache.clear();
                m_matrixCache.push_back(modelMat);
                if (msh != nullptr) {
                    modelIdx = shader->send("modelMatrix", m_matrixCache);
                }
            }
            // if (specular.size() > i && specular[i]) {
            //     shader->send("isSpecular", m_isSpecularCache1);
            // } else {
            //     shader->send("isSpecular", m_isSpecularCache0);
            // }

            shader->sendConstant("miscInfo", {glm::vec4(0.0f, 1.0f, 1.0f, shadows ? 1.0f : 0.0f)});

            if (jointList[i].size() > 0) {
            //     // Create a large buffer containing ALL joint matrices
            //     gfx::Buffer::Settings bufferSettings(gfx::BUFFERUSAGEFLAG_SHADER_STORAGE, gfx::BUFFERDATAUSAGE_STATIC);
            //     // Pack all joint matrices into one buffer (column-major)
                // std::vector<float> allJointMatrices;
                // allJointMatrices.reserve(jointMatrix[i].size() * 16); // All joints * 16 floats per matrix
                
                // // Add ALL joint matrices to the buffer (not just the ones in jointList)
                // for (const auto& jointPair : jointMatrix[i]) {
                //     glm::mat4 mat = jointPair.second;
                //     // Add matrix elements in column-major order
                //     for (int col = 0; col < 4; col++) {
                //         for (int row = 0; row < 4; row++) {
                //             allJointMatrices.push_back(mat[col][row]);
                //         }
                //     }
                // }

            //     int v = jointList[i].size();
            //     int* data = &v;
                // auto offset =shader->shader->updateBuffer("JointMatrixBlock", allJointMatrices.data(),
                //                              allJointMatrices.size() * sizeof(float), 0);
            //     shader->shader->updateBuffer("JointCountBuffer", data,
            //                                  1 * sizeof(int));
                // size_t leakCountBefore, leakCountAfter;
                // uint64_t leakSizeBefore, leakSizeAfter;
                
                // __mem_leak_check(leakCountBefore, leakSizeBefore, false, "", false);
                // Bounds check: jointMatrix size must be reasonable (< 512 joints)
                if (jointMatrix[i].size() > 512) {
                    fprintf(stderr, "[MESH] Invalid jointMatrix size %zu for mesh %d, skipping\n", jointMatrix[i].size(), i);
                } else {
                    auto offset = shader->sendMap("jointMatrix", jointMatrix[i], jointList[i]);
                    
                    m_jointInfoCache[0] = glm::vec4((float)jointMatrix[i].size(), (float)offset, (float)modelIdx, 0.0f);
                }
                if (msh != nullptr) {
                    shader->sendConstant("jointInfo", m_jointInfoCache);
                }
            } else {
                m_jointInfoCache[0] = glm::vec4(0.0f, 0.0f, (float)modelIdx, 0.0f);
                shader->sendConstant("jointInfo", m_jointInfoCache);
            }
           
            if (msh != nullptr) {
                // Force opaque blending mode for mesh rendering
                // gfx::BlendState opaqueBlend;
                // opaqueBlend.enable = false;  // Disable blending - write directly
                // gfx->setBlendState(opaqueBlend);

                // Rebind the mesh's texture to the shader to ensure correct texture state.
                // Only invalidate the descriptor set when the texture pointer changes —
                // invalidating every draw was causing one allocation per mesh per frame.
                auto meshTexture = msh->getTexture();
                static gfx::Texture* lastBoundTexture = nullptr;
                if (meshTexture != lastBoundTexture) {
                    shader->invalidateDescriptorSets();
                    lastBoundTexture = meshTexture;
                }
                if (meshTexture != nullptr) {
                    auto mainTexInfo = shader->shader->getMainTextureInfo();
                    if (mainTexInfo != nullptr) {
                        shader->shader->sendTextures(mainTexInfo, &meshTexture, 1);
                    }
                }

                // ---- Blender-level PBR: bind additional texture maps ----------------
                // Helper: only bind when uniform exists AND is an active sampler in this shader.
                // If the mesh has no map for this slot, fall back to the mesh's base color
                // texture so the sampler is never left unbound (avoids Vulkan validation errors).
                auto sendPBRTex = [&](const char* uniformName, gfx::Texture* pbrTex) {
                    auto info = shader->shader->getUniformInfo(uniformName);
                    if (info == nullptr || !info->active || info->baseType != gfx::Shader::UNIFORM_SAMPLER)
                        return;
                    // Use pbrTex if available, otherwise fall back to the base-color texture
                    gfx::Texture* bindTex = (pbrTex != nullptr) ? pbrTex : meshTexture;
                    if (bindTex != nullptr)
                        shader->shader->sendTextures(info, &bindTex, 1);
                };

                gfx::Texture* pbrNormal  = (i < (int)normalTextures.size())            ? normalTextures[i]            : nullptr;
                gfx::Texture* pbrMR      = (i < (int)metallicRoughnessTextures.size())  ? metallicRoughnessTextures[i]  : nullptr;
                gfx::Texture* pbrEmit    = (i < (int)emissiveTextures.size())           ? emissiveTextures[i]           : nullptr;
                gfx::Texture* pbrAO      = (i < (int)occlusionTextures.size())          ? occlusionTextures[i]          : nullptr;

                sendPBRTex("normalMap",            pbrNormal);
                sendPBRTex("metallicRoughnessMap", pbrMR);
                sendPBRTex("emissiveMap",          pbrEmit);
                sendPBRTex("occlusionMap",         pbrAO);

                // PBR scalar material factors — only write when uniform has a valid data pointer
                auto sendVec4Uniform = [&](const char* name, float x, float y, float z, float w) {
                    auto info = shader->shader->getUniformInfo(name);
                    if (info == nullptr || info->data == nullptr ||
                        info->baseType == gfx::Shader::UNIFORM_SAMPLER ||
                        info->baseType == gfx::Shader::UNIFORM_STORAGETEXTURE)
                        return;
                    int slot = (modelIdx >= 0) ? modelIdx : 0;
                    info->floats[slot * 4 + 0] = x; info->floats[slot * 4 + 1] = y;
                    info->floats[slot * 4 + 2] = z; info->floats[slot * 4 + 3] = w;
                    shader->shader->updateUniform(info, slot + 1);
                };

                if (i >= 0 && i < (int)materialPropsList.size()) {
                    const PBRMaterialProps& props = materialPropsList[i];

                    sendVec4Uniform("metallicRoughnessFactors",
                        props.metallicFactor, props.roughnessFactor,
                        props.normalScale,    props.occlusionStrength);
                    sendVec4Uniform("emissiveFactor",
                        props.emissiveFactor.x, props.emissiveFactor.y,
                        props.emissiveFactor.z, props.alphaCutoff);
                    sendVec4Uniform("pbrFlags",
                        props.hasNormalMap            ? 1.0f : 0.0f,
                        props.hasMetallicRoughnessMap ? 1.0f : 0.0f,
                        props.hasEmissiveMap          ? 1.0f : 0.0f,
                        props.hasOcclusionMap         ? 1.0f : 0.0f);
                    sendVec4Uniform("alphaMode",
                        props.alphaBlend  ? 1.0f : 0.0f,
                        props.alphaMask   ? 1.0f : 0.0f,
                        props.doubleSided ? 1.0f : 0.0f, 0.0f);
                    
                } else {
                    // No PBR material props — legacy lighting path: write miscInfo[modelIdx] (y=0)
                    
                }
                // ---- end PBR --------------------------------------------------------
                
                msh->draw(gfx, mat);  // Use physics-updated matrix, not 'm'
            }
        }
        currentTime = dt;
    }
    
    static size_t lastLeakCount = 0;
    static uint64_t lastLeakSize = 0;
    size_t leakCountBefore, leakCountAfter;
    uint64_t leakSizeBefore, leakSizeAfter;
    
    // if (frameCount % 300 == 0) {
    //     __mem_leak_check(leakCountAfter, leakSizeAfter, true, "", false);
        
    //     size_t deltaCount = leakCountAfter - leakCountBefore;
    //     uint64_t deltaSize = leakSizeAfter - leakSizeBefore;
        
    //     printf("[MESH DRAW] Frame %d | Delta: %zu objects, %llu bytes | Total Delta since last: %zu objects, %llu bytes\n",
    //            frameCount, deltaCount, deltaSize,
    //            leakCountAfter - lastLeakCount, leakSizeAfter - lastLeakSize);
        
    //     printf("  nodeActiveMatrix size: %zu\n", nodeActiveMatrix.size());
    //     printf("  animationFrameMatrixCache size: %zu\n", animationFrameMatrixCache.size());
    //     printf("  jointMatrix size: %zu\n", jointMatrix.size());
    //     printf("  jointList size: %zu\n", jointList.size());
        
    //     lastLeakCount = leakCountAfter;
    //     lastLeakSize = leakSizeAfter;
    // }

    // __mem_leak_check(leakCountAfter, leakSizeAfter, true, "", false);
    
    // printf("Leak delta: %zu objects, %llu bytes\n", 
    //     leakCountAfter - leakCountBefore,
    //     leakSizeAfter - leakSizeBefore);
}

Matrix4 chai_mesh::getNodeMatrix(const std::string &node) {
    auto n = nodeNames[node];
    // for (auto &pair : nodeNames) {
    //     printf("Node: %s %d %d %d\n", pair.first.c_str(), pair.second, nodeNames.size(), id);
    // }
    glm::mat4 m = nodeActiveMatrix[n];
    // return activeMatrix;
    // printf("Node Matrix: %s\n", node.c_str());
    // printf("Matrix: %f %f %f %f\n", m[3][0], m[3][1], m[3][2], m[3][3]);
    float matrixData[16] = {
        m[0][0], m[0][1], m[0][2], m[0][3],
        m[1][0], m[1][1], m[1][2], m[1][3],
        m[2][0], m[2][1], m[2][2], m[2][3],
        m[3][0], m[3][1], m[3][2], m[3][3] 
    };
    return matrices[0] * activeMatrix * Matrix4(matrixData);
}

void chai_mesh::loadSpecular(std::string texture) {
    if (texture == "") {
        texture = "/cartridges/test/assets/particles/sunrise.jpg";
    }
            
    auto& cg = ChaiLove::getInstance()->chai_gfx;
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

    delete img;
    img = NULL;

    specularW = w;
    specularH = h;
    specData = copyOfPixelData;
}

void chai_mesh::setVisible(bool visible) {
    this->visible = visible;
}

std::pair<glm::vec3, glm::vec3> chai_mesh::getBoundingBox(const glm::mat4 &viewProjectionMatrix) {    
    auto cc = ChaiLove::getInstance()->chai_collisions;

    auto bb = cc.getBoundingBox(id);
    auto boundingBoxMin = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    auto boundingBoxMax = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    // printf("BB: %d\n", bb.size());
    // printf("Mesh: %d\n", meshes.size());  
    for (int x = 0; x < bb.size(); x++) {
        auto min = bb[x].first;
        auto max = bb[x].second;
        if (min.x < boundingBoxMin.x) {
            boundingBoxMin.x = min.x;
        }
        if (min.y < boundingBoxMin.y) {
            boundingBoxMin.y = min.y;
        }
        if (min.z < boundingBoxMin.z) {
            boundingBoxMin.z = min.z;
        }

        if (max.x > boundingBoxMax.x) {
            boundingBoxMax.x = max.x;
        }
        if (max.y > boundingBoxMax.y) {
            boundingBoxMax.y = max.y;
        }
        if (max.z > boundingBoxMax.z) {
            boundingBoxMax.z = max.z;
        }

        auto minBounds = glm::vec3(min.x, min.y, min.z);
        auto maxBounds = glm::vec3(max.x, max.y, max.z);

        // Extract frustum planes from the view-projection matrix
        glm::vec4 planes[6];
        planes[0] = glm::vec4(
            viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0],
            viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0],
            viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0],
            viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0]);
        planes[1] = glm::vec4(
            viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0],
            viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0],
            viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0],
            viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0]);
        planes[2] = glm::vec4(
            viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1],
            viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1],
            viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1],
            viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1]);
        planes[3] = glm::vec4(
            viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1],
            viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1],
            viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1],
            viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1]);
        planes[4] = glm::vec4(
            viewProjectionMatrix[0][3] + viewProjectionMatrix[0][2],
            viewProjectionMatrix[1][3] + viewProjectionMatrix[1][2],
            viewProjectionMatrix[2][3] + viewProjectionMatrix[2][2],
            viewProjectionMatrix[3][3] + viewProjectionMatrix[3][2]);
        planes[5] = glm::vec4(
            viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2],
            viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
            viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2],
            viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2]);

        // Normalize the planes
        // for (int i = 0; i < 6; i++) {
        //     float length = glm::length(glm::vec3(planes[i]));
        //     planes[i] /= length;
        // }

        // Check if the bounding box is outside any plane
        for (int i = 0; i < 6; i++) {
            glm::vec3 normal = glm::vec3(planes[i]);
            float distance = planes[i].w;

            // Find the farthest point in the direction of the plane normal
            glm::vec3 farPoint = glm::vec3(
                (normal.x > 0) ? maxBounds.x : minBounds.x,
                (normal.y > 0) ? maxBounds.y : minBounds.y,
                (normal.z > 0) ? maxBounds.z : minBounds.z);

            // If the farthest point is outside the plane, the box is outside the frustum
            if (glm::dot(normal, farPoint) + distance < 0) {
                // printf("SubMesh %d is outside the frustum\n", x);
                subVisible[x] = false; // Completely outside
            }
        }

        subVisible[x] = true; // Inside or intersecting
    }    
    
    // printf("Bounding Box: %f %f %f %f %f %f\n", boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z, boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z);
    return std::pair<glm::vec3, glm::vec3>(boundingBoxMin, boundingBoxMax);
}

std::vector<float> chai_mesh::getMeshBoundingBox() {
    auto cc = ChaiLove::getInstance()->chai_collisions;

    auto bb = cc.getBoundingBox(id);

    if (bb.size() == 0) {
        return std::vector<float>({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    }
    
    return std::vector<float>({bb[0].first.x, bb[0].first.y, bb[0].first.z, bb[0].second.x, bb[0].second.y, bb[0].second.z});
}

chai_mesh::~chai_mesh() {
    // Explicitly clear all Boxed_Value caches to ensure proper cleanup
    m_matrixCache.clear();
    m_isSpecularCache0.clear();
    m_isSpecularCache1.clear();
    m_jointInfoCache.clear();
    
    // Clear map containers to prevent leaks
    skins.clear();
    jointOrder.clear();
    nodeChildren.clear();
    
    // Clear PBR texture vectors (textures are reference-counted by the graphics system)
    normalTextures.clear();
    metallicRoughnessTextures.clear();
    emissiveTextures.clear();
    occlusionTextures.clear();
    materialPropsList.clear();

    // Free specular data buffer if allocated
    if (specData != nullptr) {
        delete[] specData;
        specData = nullptr;
    }
    
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
    std::random_device rd;
    id = rd();
    meshes = c.meshes;
    nodeMatrix = c.nodeMatrix;
    nodeParent = c.nodeParent;
    nodeChildren = c.nodeChildren;
    jointOrder = c.jointOrder;
    skins = c.skins;
    meshToNode = c.meshToNode;
    jointList = c.jointList;
    jointMatrix = c.jointMatrix;
    activeAnimations = c.activeAnimations;
    animations = c.animations;
    nodeNames = c.nodeNames;

    m_cachedAnimationDurations = c.m_cachedAnimationDurations;

    matrices = c.matrices;
    offsetMatrices = c.offsetMatrices;

    cameraParams = c.cameraParams;
    lightParams = c.lightParams;
    textures = c.textures;
    vf = c.vf;

    visible = c.visible;
    subVisible = c.subVisible;
    specular = c.specular;

    // PBR material data - share texture pointers (reference counted)
    normalTextures            = c.normalTextures;
    metallicRoughnessTextures = c.metallicRoughnessTextures;
    emissiveTextures          = c.emissiveTextures;
    occlusionTextures         = c.occlusionTextures;
    materialPropsList         = c.materialPropsList;

    currentTime = c.currentTime;
    
    mesh = c.mesh;
    instance = c.instance;

    tex = c.tex;
    // image = c.image;
    img = c.img;

    slices = c.slices;
    buf = c.buf;
    cloned = true;
}

chai_mesh *chai_mesh::clone() const
{
	return new chai_mesh(*this);
}

float chai_mesh::calculateAnimationDuration(const std::string &name) {
    auto animationIt = animations.find(name);
    if (animationIt == animations.end()) {
        return 0.0f;
    }
    
    float maxTime = 0.0f;
    
    // Iterate through all channels to find the maximum keyframe time
    for (const auto& channel : animationIt->second) {
        for (const auto& node : channel.second) {
            const auto& keyframes = node.second;
            if (!keyframes.empty()) {
                // Get the last keyframe time (keyframes should be sorted by time)
                float lastKeyframeTime = keyframes.back().first;
                maxTime = std::max(maxTime, lastKeyframeTime);
            }
        }
    }
    
    return maxTime;
}

}
