#include "../ChaiLove.h"
#include <cmath> // For M_PI
#include <GL/gl.h> // For OpenGL functions like glOrtho
#ifndef TINY_GLTF_H_
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_FS
#define TINYGLTF_NO_STB_IMAGE
// #define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "../../vendor/tinygltf/tiny_gltf.h"
#endif

namespace love
{
chai_collisions::chai_collisions()
{
}
chai_collisions::~chai_collisions()
{
}
std::vector<int> chai_collisions::addRigidMesh(std::string meshPath, int meshRef, bool makeConvex) 
{
    auto cl = ChaiLove::getInstance();
    auto f = cl->getFSModule();
    auto s = f.getSize(meshPath);
    auto file = f.readBuffer(meshPath, s);    

    // Allocate a character array and copy the file data into it
    unsigned char* data = new unsigned char[s];
    std::memcpy(data, file, s);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    loader.LoadBinaryFromMemory(&model, &err, &warn, data, s, "", false);
    std::vector<int> refs;
    auto count = rigidMeshes.size();  
    
    // if (model.cameras.size() > 0) {
    //     auto camera = model.cameras[0];

    //     if (camera.type == "perspective") {
    //         debugDrawer->fov = camera.perspective.yfov;
    //         debugDrawer->aspectRatio = camera.perspective.aspectRatio;
    //         debugDrawer->nearPlane = camera.perspective.znear;
    //         debugDrawer->farPlane = camera.perspective.zfar;
    //     }

    //     // Retrieve the camera position
    //     for (auto node : model.nodes) {
    //         if (node.camera == 0) {
    //             glm::vec3 cameraPosition(0.0f);
    //             glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
    //             glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector
    //             if (!node.translation.empty()) {
    //                 cameraPosition = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
    //             }
    //             if (!node.rotation.empty()) {
    //                 glm::quat rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
    //                 glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
    //                 cameraUp = glm::vec3(rotationMatrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    //                 cameraDirection = glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
    //             }
    //             glm::vec3 cameraTarget = cameraPosition + cameraDirection;
    //             debugDrawer->cameraPosition = cameraPosition;
    //             debugDrawer->upVector = cameraUp;
    //             debugDrawer->cameraTarget = cameraTarget;
    //             break;
    //         }
    //     }
    // } else {
    //     debugDrawer->fov = 1920.0f / 1080.0f;
    //     debugDrawer->aspectRatio = 1.33f;
    //     debugDrawer->nearPlane = 0.01;
    //     debugDrawer->farPlane = 1000.0f;

    //     glm::vec3 cameraPosition(0.0f);
    //     glm::vec3 cameraUp(0.0f, 1.0f, 0.0f); // Default up vector
    //     glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f); // Default target vector

    //     glm::vec3 cameraTarget = cameraPosition + cameraDirection;
    //     debugDrawer->cameraPosition = cameraPosition;
    //     debugDrawer->upVector = cameraUp;
    //     debugDrawer->cameraTarget = cameraTarget;
    // }

    // Set OpenGL to wireframe mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    for (size_t i = 0; i < model.meshes.size(); i++) {
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

        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2], modelMatrix[0][3]);
        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2], modelMatrix[1][3]);
        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2], modelMatrix[2][3]);
        // printf("Model Matrix: %f, %f, %f, %f\n", modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2], modelMatrix[3][3]);

        btTriangleMesh *mesh = new btTriangleMesh();
        btConvexHullShape *convexShape = new btConvexHullShape();            
    
        auto primitive = model.meshes[i].primitives[0];
        auto positionAccessor = model.accessors[primitive.attributes["POSITION"]];
        auto positionBufferView = model.bufferViews[positionAccessor.bufferView];
        auto positionBuffer = model.buffers[positionBufferView.buffer];
        const float* positions = reinterpret_cast<const float*>(&positionBuffer.data[positionAccessor.byteOffset + positionBufferView.byteOffset]);

        auto indiceAccessor = model.accessors[primitive.indices];
        auto indiceBufferView = model.bufferViews[indiceAccessor.bufferView];
        auto indiceBuffer = model.buffers[indiceBufferView.buffer];

        if (indiceAccessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const unsigned int* indices = reinterpret_cast<const unsigned int*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            std::vector<float> vertices;
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
                
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[indices[i] * 3 + j]);
                }
                // if (isnan(vertices[0]) || isnan(vertices[1]) || isnan(vertices[2])) {
                //     printf("nan: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[3]) || isnan(vertices[4]) || isnan(vertices[5])) {
                //     printf("nan: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[6]) || isnan(vertices[7]) || isnan(vertices[8])) {
                //     printf("nan: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                if (vertices.size() == 9) {
                    auto m = modelMatrix * glm::vec4(vertices[0], vertices[1], vertices[2], 1.0f);
                    // vertices[0] = m.x;
                    // vertices[1] = m.y;
                    // vertices[2] = m.z;
                    m = modelMatrix * glm::vec4(vertices[3], vertices[4], vertices[5], 1.0f);
                    // vertices[3] = m.x;
                    // vertices[4] = m.y;
                    // vertices[5] = m.z;
                    m = modelMatrix * glm::vec4(vertices[6], vertices[7], vertices[8], 1.0f);
                    // vertices[6] = m.x;
                    // vertices[7] = m.y;
                    // vertices[8] = m.z;
                    if (false) {
                        // Define the vertices of the triangle
                        glBegin(GL_TRIANGLES);
                        glVertex3f(vertices[0], vertices[1], vertices[2]);
                        glVertex3f(vertices[3], vertices[4], vertices[5]);
                        glVertex3f(vertices[6], vertices[7], vertices[8]);
                        glEnd();    
                    }
                    // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                    // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                    // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                    if (makeConvex) {
                        if (!isfinite(vertices[0]) || !isfinite(vertices[1]) || !isfinite(vertices[2]) ||
                            !isfinite(vertices[3]) || !isfinite(vertices[4]) || !isfinite(vertices[5]) ||
                            !isfinite(vertices[6]) || !isfinite(vertices[7]) || !isfinite(vertices[8])) {
                            // Skip adding vertices if they are not finite
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                            
                        } else {
                            // Add the vertices to the convex shape
                            convexShape->addPoint(btVector3(vertices[0], vertices[1], vertices[2]));
                            convexShape->addPoint(btVector3(vertices[3], vertices[4], vertices[5]));
                            convexShape->addPoint(btVector3(vertices[6], vertices[7], vertices[8]));
                        }
                    } else {
                        mesh->addTriangle(btVector3(vertices[0], vertices[1], vertices[2]), btVector3(vertices[3], vertices[4], vertices[5]), btVector3(vertices[6], vertices[7], vertices[8]));
                    }
                    vertices.clear();
                }
                // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
            }
        } else {
            const unsigned short* indices = reinterpret_cast<const unsigned short*>(&indiceBuffer.data[indiceAccessor.byteOffset + indiceBufferView.byteOffset]);
            std::vector<float> vertices;
            
            for (size_t i = 0; i < indiceAccessor.count; ++i) {
               
                // Extract and push positions
                for (size_t j = 0; j < 3; ++j) {
                    vertices.push_back(positions[indices[i] * 3 + j]);
                }
                // if (isnan(vertices[0]) || isnan(vertices[1]) || isnan(vertices[2])) {
                //     printf("nan: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[3]) || isnan(vertices[4]) || isnan(vertices[5])) {
                //     printf("nan: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                // if (isnan(vertices[6]) || isnan(vertices[7]) || isnan(vertices[8])) {
                //     printf("nan: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                //     printf("indices: %d\n", indices[i]);
                //     printf("indices: %d\n", i);
                //     continue;
                // }
                if (vertices.size() == 9) {
                    auto m = modelMatrix * glm::vec4(vertices[0], vertices[1], vertices[2], 1.0f);
                    // vertices[0] = m.x;
                    // vertices[1] = m.y;
                    // vertices[2] = m.z;
                    m = modelMatrix * glm::vec4(vertices[3], vertices[4], vertices[5], 1.0f);
                    // vertices[3] = m.x;
                    // vertices[4] = m.y;
                    // vertices[5] = m.z;
                    m = modelMatrix * glm::vec4(vertices[6], vertices[7], vertices[8], 1.0f);
                    // vertices[6] = m.x;
                    // vertices[7] = m.y;
                    // vertices[8] = m.z;
                    if (false) {
                        // Define the vertices of the triangle
                        glBegin(GL_TRIANGLES);
                        glVertex3f(vertices[0], vertices[1], vertices[2]);
                        glVertex3f(vertices[3], vertices[4], vertices[5]);
                        glVertex3f(vertices[6], vertices[7], vertices[8]);
                        glEnd();    
                    }                 
                    // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                    // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                    // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                    if (makeConvex) {
                        if (!isfinite(vertices[0]) || !isfinite(vertices[1]) || !isfinite(vertices[2]) ||
                            !isfinite(vertices[3]) || !isfinite(vertices[4]) || !isfinite(vertices[5]) ||
                            !isfinite(vertices[6]) || !isfinite(vertices[7]) || !isfinite(vertices[8])) {
                            // Skip adding vertices if they are not finite
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                            printf("Skipping non-finite vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
                            
                        } else {
                            // Add the vertices to the convex shape
                            convexShape->addPoint(btVector3(vertices[0], vertices[1], vertices[2]));
                            convexShape->addPoint(btVector3(vertices[3], vertices[4], vertices[5]));
                            convexShape->addPoint(btVector3(vertices[6], vertices[7], vertices[8]));
                        }
                    } else {
                        mesh->addTriangle(btVector3(vertices[0], vertices[1], vertices[2]), btVector3(vertices[3], vertices[4], vertices[5]), btVector3(vertices[6], vertices[7], vertices[8]));
                    }
                    vertices.clear();
                }
                // printf("vertices: %f, %f, %f\n", vertices[0], vertices[1], vertices[2]);
                // printf("vertices: %f, %f, %f\n", vertices[3], vertices[4], vertices[5]);
                // printf("vertices: %f, %f, %f\n", vertices[6], vertices[7], vertices[8]);
            }
        }

        if (makeConvex) {
            btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
            btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(0, motionState, convexShape, btVector3(0, 0, 0));    
            btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
            rigidBody->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2])));
            rigidMeshes.emplace_back(new RigidMesh(mesh, rigidBody, meshRef));  
        } else {
            btBvhTriangleMeshShape *shape = new btBvhTriangleMeshShape(mesh, true);
            btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
            btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(0, motionState, shape, btVector3(0, 0, 0));    
            btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
            rigidBody->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2])));
            rigidMeshes.emplace_back(new RigidMesh(mesh, rigidBody, meshRef));   
        }  
        refs.push_back(count);
        count++;
    }
    // Reset OpenGL to fill mode (optional)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    return refs;
}
void chai_collisions::setRigidMeshPosition(std::vector<int> rigidMeshIndex, float x, float y, float z, std::vector<int> group = {0})
{
    int idx = 0;
    btTransform transform;
    for (auto i : rigidMeshIndex) {        
        auto r = rigidMeshes[i]->rigidBody;
        for (auto g : group) {
            if (worlds->worlds.find(g) == worlds->worlds.end()) {
                init(g);
            }
            rigidMeshes[i]->group.push_back(g);
            worlds->worlds[g]->dynamicsWorld->addRigidBody(r, btBroadphaseProxy::StaticFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
        }
        rigidMeshes[i]->rigidBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);

        if (true || idx == 0) {
            transform = rigidMeshes[i]->rigidBody->getWorldTransform();
        }
        printf("x,y,z: %f,%f,%f\n", transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ());
        rigidMeshes[i]->rigidBody->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), (btVector3(x, y, z) + btVector3(transform.getOrigin().getX(), transform.getOrigin().getY(), transform.getOrigin().getZ()))));
        idx++;
    }              
}
void chai_collisions::togglePhysics(std::vector<int> rigidMeshIndex, bool enable)
{
    for (auto i : rigidMeshIndex) {
        auto r = rigidMeshes[i]->rigidBody;
        if (enable) {
            worlds->worlds[0]->dynamicsWorld->removeRigidBody(r);
            
            r->setCollisionFlags(btCollisionObject::CF_DYNAMIC_OBJECT);
            r->setActivationState(DISABLE_DEACTIVATION); // Disable deactivation to keep the rigid body active
            btVector3 inertia;
            btScalar mass = 1000.0f; // Set mass to 1.0f for the rigid body
            r->getCollisionShape()->calculateLocalInertia(mass, inertia); 
            r->setMassProps(mass, inertia);
            // r->setCollisionFlags(r->getCollisionFlags() & ~btCollisionObject::CF_STATIC_OBJECT);
            // r->setLinearFactor(btVector3(0.1, 0.1, 0.1)); // Enable movement in all directions
            r->setGravity(btVector3(0, -9.81f, 0));
            r->setLinearVelocity(btVector3(0, 0, 0)); // Reset linear velocity
            r->setAngularVelocity(btVector3(0, 0, 0)); // Reset angular velocity
            r->setFriction(0.1f); // Set friction to a reasonable value
            r->setRestitution(0.0f); // Set restitution to a reasonable value
            r->setRollingFriction(0.1f); // Set rolling friction to a
            r->setSpinningFriction(0.1f); // Set spinning friction to a reasonable value
            r->setDamping(0.1f, 0.1f);       

            // applyForceToRigidMesh(i, 0, 5, 0); // Reset any previous forces applied to the rigid body
           
            worlds->worlds[0]->dynamicsWorld->addRigidBody(r); 
            // Set the collision flags to dynamic

        } else {
            // Turn off gravity and set the rigid body to static
            r->setCollisionFlags(r->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            r->setGravity(btVector3(0, 0, 0));
        }
    }
}
int chai_collisions::addCharacterController(int index, int meshRef, std::string charId)
{
    if (characterControllers.size() > 0) {
        for (int i = 0; i < characterControllers.size(); i++) {
            if (characterControllers[i]->charId == charId) {
                characterControllers[i]->addMesh(meshRef);
                return -1;
            }
        }
    }
    btPairCachingGhostObject *ghostObject = new btPairCachingGhostObject();
    btConvexShape *capsule = new btCapsuleShape(1.2f, 2.0f);
    ghostObject->setCollisionShape(capsule);
    ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    btScalar stepHeight = btScalar(0.35);
    btKinematicCharacterController *character = new btKinematicCharacterController(ghostObject, capsule, stepHeight, btVector3(0, 1, 0));
    character->setGravity(btVector3(0, -0.01, 0));    
    
    // btTransform startTransform;
    // startTransform.setIdentity();
    // startTransform.setOrigin(btVector3(0, 0, 0));

    // ghostObject->setWorldTransform(startTransform);
    // character->setLinearVelocity(btVector3(0, 0, 0));
    // character->setAngularVelocity(btVector3(0, 0, 0));

    ghostObject->setUserPointer(character); // Set the user pointer to the character controller

    ghostObject->setUserIndex(index); // Set the user pointer to the character controller
    characterControllers.emplace_back(new CharacterController(ghostObject, character, meshRef, charId));
    return characterControllers.size() - 1;
}
void chai_collisions::setCharacterControllerPosition(int characterIndex, float x, float y, float z, std::vector<int> group = {0})
{
    btPairCachingGhostObject *ghostObject = characterControllers[characterIndex]->ghostObject;
    
    for (auto i : group) {
        if (worlds->worlds.find(i) == worlds->worlds.end()) {
            init(i);
        }
        characterControllers[characterIndex]->group.push_back(i);
        worlds->worlds[i]->dynamicsWorld->addCollisionObject(characterControllers[characterIndex]->ghostObject, btBroadphaseProxy::CharacterFilter, btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter);
        worlds->worlds[i]->dynamicsWorld->addCharacter(characterControllers[characterIndex]->character);
    }
    
    ghostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
    ghostObject->setWorldTransform(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
}
void chai_collisions::applyForceToCharacter(int characterIndex, float x, float y, float z)
{
    characterControllers[characterIndex]->character->setWalkDirection(btVector3(x, y, z));
}
void chai_collisions::applyForceToRigidMesh(int rigidMeshIndex, float x, float y, float z)
{
    rigidMeshes[rigidMeshIndex]->rigidBody->applyCentralForce(btVector3(x, y, z));
}
std::vector<float> chai_collisions::getCharacterController(int ref)
{
    return std::vector<float>{
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getX(),
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getY(),
        characterControllers[ref]->ghostObject->getWorldTransform().getOrigin().getZ()
    };
}
std::vector<float> chai_collisions::getRigidMesh(int ref)
{
    return std::vector<float>{
        rigidMeshes[ref]->rigidBody->getWorldTransform().getOrigin().getX(),
        rigidMeshes[ref]->rigidBody->getWorldTransform().getOrigin().getY(),
        rigidMeshes[ref]->rigidBody->getWorldTransform().getOrigin().getZ()
    };
}
int chai_collisions::addBox(float x, float y, float z, float width, float height, float depth, std::vector<int> group = {0}, int index = 0)
{
    btTriangleMesh *mesh = new btTriangleMesh();

    // Define the vertices of an inner and outer wall box
    btVector3 vertices[8] = {
        btVector3(-width / 2, -height / 2, -depth / 2),
        btVector3(width / 2, -height / 2, -depth / 2),
        btVector3(width / 2, height / 2, -depth / 2),
        btVector3(-width / 2, height / 2, -depth / 2),
        btVector3(-width / 2, -height / 2, depth / 2),
        btVector3(width / 2, -height / 2, depth / 2),
        btVector3(width / 2, height / 2, depth / 2),
        btVector3(-width / 2, height / 2, depth / 2)
    };

    // Define the triangles of the box (6 faces, 2 triangles per face)
    int indices[12][3] = {
        {0, 1, 2}, {0, 2, 3}, // Front face
        {4, 5, 6}, {4, 6, 7}, // Back face
        {0, 1, 5}, {0, 5, 4}, // Bottom face
        {2, 3, 7}, {2, 7, 6}, // Top face
        {0, 3, 7}, {0, 7, 4}, // Left face
        {1, 2, 6}, {1, 6, 5}  // Right face
    };

    // Add the triangles to the mesh
    for (int i = 0; i < 12; ++i) {
        mesh->addTriangle(vertices[indices[i][0]], vertices[indices[i][1]], vertices[indices[i][2]]);
    }

    // Create the convex triangle mesh shape
    btCompoundShape *shape = new btCompoundShape();
    shape->addChildShape(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)), new btBvhTriangleMeshShape(mesh, true));

    // Set a non-zero mass to make the box dynamic
    // btScalar mass = 1.0f; // Adjust mass as needed
    // btVector3 inertia(0, 0, 0);
    // shape->calculateLocalInertia(mass, inertia);
    btDefaultMotionState *motionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(x, y, z)));
    btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(1.0f, motionState, shape, btVector3(0, 0, 0));
    btRigidBody *rigidBody = new btRigidBody(rigidBodyCI);
    rigidBody->setGravity(btVector3(0, 0, 0));
    // rigidBody->setLinearVelocity(btVector3(0, 0, 0));
    // rigidBody->setAngularVelocity(btVector3(0, 0, 0));
    // rigidBody->setLinearFactor(btVector3(1.9f, 0, 0));
    // Set friction and damping properties suitable for 1:1 character control
    // rigidBody->setFriction(0.45f); // Set a small positive friction value
    // rigidBody->setDamping(0.45f, 0.01f); // Set small positive damping values for stability
    // rigidBody->setRestitution(1.0f); // Set a small positive restitution value

    rigidBody->setUserPointer(rigidBody); // Set the user pointer to the character controller  
    rigidBody->setUserIndex(index);
    rigidBody->setUserIndex2(1);
    // Disable deactivation to keep the object active
    rigidBody->setActivationState(DISABLE_DEACTIVATION);
    rigidBody->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    cameraBox.emplace_back(rigidBody);
    rigidMeshes.emplace_back(new RigidMesh(shape, rigidBody));   
    for (auto i : group) {
        if (worlds->worlds.find(i) == worlds->worlds.end()) {
            init(i);
        }
        rigidMeshes.back()->group.push_back(i);
        worlds->worlds[i]->dynamicsWorld->addRigidBody(rigidBody, btBroadphaseProxy::DefaultFilter, btBroadphaseProxy::DefaultFilter | btBroadphaseProxy::CharacterFilter);
    }
    return rigidMeshes.size() - 1;
}

std::vector<std::pair<glm::vec3, glm::vec3>> chai_collisions::getBoundingBox(int mesh)
{
    std::vector<std::pair<glm::vec3, glm::vec3>> bb;
    for (auto &m : rigidMeshes) {
        // printf("Mesh: %d, %d\n", m->meshRef, mesh);
        if (m->meshRef == mesh) {
            btVector3 minV, maxV;
            
            m->rigidBody->getAabb(minV, maxV);

            // minV = m->rigidBody->getWorldTransform().inverse() * minV;
            // maxV = m->rigidBody->getWorldTransform().inverse() * maxV;
            
            bb.push_back(std::make_pair(glm::vec3(minV.getX(), minV.getY(), minV.getZ()), glm::vec3(maxV.getX(), maxV.getY(), maxV.getZ())));
        }
    }
    for (auto &c : characterControllers) {
        for (auto &m : c->meshRef) {
            // printf("Mesh: %d, %d\n", m, mesh);
            if (m == mesh) {
                btVector3 minV, maxV;
                c->ghostObject->getCollisionShape()->getAabb(c->ghostObject->getWorldTransform(), minV, maxV);
                bb.push_back(std::make_pair(glm::vec3(minV.getX(), minV.getY(), minV.getZ()), glm::vec3(maxV.getX(), maxV.getY(), maxV.getZ())));
                break;
            }
            if (bb.size() > 0) {
                break;
            }
        }
    }
    return bb;
}

std::vector<Matrix4> chai_collisions::getPhysicsObjects(int mesh)
{
    std::vector<Matrix4> objects;
    int idx = 0;
    for (auto &m : rigidMeshes) {
        if (m->meshRef == mesh) {
            btTransform transform = m->rigidBody->getWorldTransform();
            glm::mat4 modelMat(
                (float)transform.getBasis()[0][0], (float)transform.getBasis()[0][1], (float)transform.getBasis()[0][2], 0.0f,
                (float)transform.getBasis()[1][0], (float)transform.getBasis()[1][1], (float)transform.getBasis()[1][2], 0.0f,
                (float)transform.getBasis()[2][0], (float)transform.getBasis()[2][1], (float)transform.getBasis()[2][2], 0.0f,
                (float)transform.getOrigin().getX(), (float)transform.getOrigin().getY(), (float)transform.getOrigin().getZ(), 1.0f
            );

            glm::mat4 projection = glm::perspective(
                glm::radians(26.0f), 
                debugDrawer->aspectRatio, 
                debugDrawer->nearPlane, 
                debugDrawer->farPlane
            );
            glm::vec3 vec(1.0f,1.0f,1.0f);
            glm::vec3 up(0.0f,1.0f,0.0f);
            glm::mat4 view = glm::lookAt(
                vec, 
                vec, 
                up
            );

            glm::mat4 reflect = glm::mat4(
                1,  0,  0, 0,
                0, 1,  0, 0,
                0,  0,  1, 0,
                0,  0,  0, 1
            );

            // modelMat *= reflect;

            // modelMat = view * modelMat;
            // modelMat = 20.0f;

            Matrix4 mat = Matrix4(new float[16] {
                modelMat[0][0], modelMat[0][1], modelMat[0][2], modelMat[0][3],
                modelMat[1][0], modelMat[1][1], modelMat[1][2], modelMat[1][3],
                modelMat[2][0], modelMat[2][1], modelMat[2][2], modelMat[2][3],
                modelMat[3][0], modelMat[3][1], modelMat[3][2], modelMat[3][3],
            });
            // printf("Origin: %f, %f, %f\n", 
                // transform.getOrigin().getX(), 
                // transform.getOrigin().getY(), 
                // transform.getOrigin().getZ());
            objects.push_back(mat);
        }
        idx++;
    }
    return objects;
}

void chai_collisions::init(int group = 0)
{    
    btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld *dw = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    auto w = new World(broadphase, collisionConfiguration, dispatcher, solver, dw);
    if (worlds == nullptr) {
        worlds = new WorldMap();
    }
    worlds->worlds[group] = w;
    
    worlds->worlds[group]->dynamicsWorld->setGravity(btVector3(0, -9.81f, 0)); 
    
    worlds->worlds[group]->dynamicsWorld->setInternalTickCallback([](btDynamicsWorld *world, btScalar timeStep) {
        chai_collisions *self = static_cast<chai_collisions *>(world->getWorldUserInfo());
        btDispatcher *dispatcher = world->getDispatcher();
        const int numManifolds = dispatcher->getNumManifolds();
        btVector3 vel0 = btVector3(0, 0, 0);
        btVector3 vel1 = btVector3(0, 0, 0);
        std::map<int, bool> contact;
        std::map<int, btRigidBody*> body;
        for (int i = 0; i < self->cameraBox.size(); ++i) {
            contact[i] = false;
        }
        for (int m = 0; m < numManifolds; ++m) {
            auto *manifold = dispatcher->getManifoldByIndexInternal(m);
            const btRigidBody *body0 = static_cast<const btRigidBody *>(manifold->getBody0());
            const btRigidBody *body1 = static_cast<const btRigidBody *>(manifold->getBody1());
            
            auto numContacts = manifold->getNumContacts();
           
            if (body0->getUserIndex() == body1->getUserIndex() && numContacts > 0) {
                contact[body1->getUserIndex()] = true;
               
                btVector3 pos0 = body0->getWorldTransform().getOrigin();
                btVector3 pos1 = body1->getWorldTransform().getOrigin();

                // Calculate the collision direction
                btVector3 collisionDirection = pos1 - pos0;

                // Optional: Normalize the direction vector
                collisionDirection.normalize();

                if (body1->getUserIndex() != -1) {
                    auto b = self->cameraBox[body1->getUserIndex()];
                    b->setLinearVelocity(btVector3(-collisionDirection.getX() * 6.0f, 0.0f, 0.0f));
                    auto c = self->characterControllers[body1->getUserIndex()]->character;
                    // auto v = c->getLinearVelocity();
                    // c->getGhostObject()->setLinearVelocity(btVector3(v.getX(), v.getY(), 0.0f));
                    c->getGhostObject()->setUserIndex2(collisionDirection.getZ() > 0 ? 1 : collisionDirection.getZ() < 0 ? -1 : 0);       
                }         
            }
        }
        for (int i = 0; i < self->cameraBox.size(); ++i) { 
            self->cameraBox[i]->setGravity(btVector3(0, 0, 0));           
            if (!contact[i]) {
                auto b = self->cameraBox[i];                
                if (b != nullptr) {
                    auto v = b->getLinearVelocity();
                    b->setLinearVelocity(btVector3(v.getX()/1.5f, 0, 0));
                    auto c = self->characterControllers[i]->character->getGhostObject();
                    c->setUserIndex2(0);
                }
            }
        }
    }, this);

    // Initialize the debug drawer
    if (debugDrawer == nullptr) { 
        debugDrawer = new OpenGLDebugDrawer();
        debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
        worlds->worlds[group]->dynamicsWorld->setDebugDrawer(debugDrawer);
    }
    

}
void chai_collisions::destroy()
{
    for (auto &rm : rigidMeshes) {
        for (int i = 0; i < rm->group.size(); i++) {
            worlds->worlds[rm->group[i]]->dynamicsWorld->removeRigidBody(rm->rigidBody);
        }
        
        delete rm;
    }
    rigidMeshes.clear();
    rigidMeshes = std::vector<RigidMesh*>();
    for (auto &cc : characterControllers) {
        for (int i = 0; i < cc->group.size(); i++) {
            worlds->worlds[cc->group[i]]->dynamicsWorld->removeAction(cc->character);
            worlds->worlds[cc->group[i]]->dynamicsWorld->removeCollisionObject(cc->ghostObject);
        }
        delete cc;
    }
    characterControllers.clear();
    characterControllers = std::vector<CharacterController*>();

    cameraBox.clear();
    cameraBox = std::vector<btRigidBody*>();

    delete debugDrawer;
    debugDrawer = nullptr;
    delete worlds;
}

uint8_t* chai_collisions::processDebug(float deltaTime, std::vector<chaiscript::Boxed_Value> viewMatrix)
{
    auto width = 1920; // Set your desired width
    auto height = 1080; // Set your desired height
    if (!debugDrawer->debugTexture) {
        glGenTextures(1, &debugDrawer->debugTexture);
    }

    glBindTexture(GL_TEXTURE_2D, debugDrawer->debugTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (!debugDrawer->debugFBO) {
        glGenFramebuffers(1, &debugDrawer->debugFBO);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, debugDrawer->debugFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, debugDrawer->debugTexture, 0);

    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    

    for (auto &dw : worlds->worlds) {
        for (auto &cc : characterControllers) {
            auto stopZ = cc->ghostObject->getUserIndex2();
            auto v = cc->character->getLinearVelocity();
            if (stopZ > 0 && v.getZ() < 0) {                
                cc->character->setWalkDirection(btVector3(v.getX(), v.getY(), 0.0f));
            }
            if (stopZ < 0 && v.getZ() > 0) {                
                cc->character->setWalkDirection(btVector3(v.getX(), v.getY(), 0.0f));
            }
            cc->character->preStep(dw.second->dynamicsWorld);
            cc->character->playerStep(dw.second->dynamicsWorld, 1);
            
            cc->character->setWalkDirection(btVector3(0.0f, 0.0f, 0.0f));

            btVector3 pos = cc->ghostObject->getWorldTransform().getOrigin();
            
            // printf("ghost pos: %f, %f, %f\n", pos.getX(), pos.getY(), pos.getZ());
        }
        dw.second->dynamicsWorld->stepSimulation(deltaTime, 10);  
        if (debugDrawer) {
            std::vector<float> prepD;
            for (auto d : viewMatrix) {
                auto v = chaiscript::boxed_cast<float>(d);
                prepD.push_back(v);
            }

            btVector3 gravity = dw.second->dynamicsWorld->getGravity();
            // printf("Gravity: %f, %f, %f\n", gravity.getX(), gravity.getY(), gravity.getZ());   

            btVector3 min, max;
            min.setValue(FLT_MAX, FLT_MAX, FLT_MAX);
            max.setValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (int i = 0; i < dw.second->dynamicsWorld->getNumCollisionObjects(); i++) {
                btCollisionObject *obj = dw.second->dynamicsWorld->getCollisionObjectArray()[i];
                btVector3 aabbMin, aabbMax;
                obj->getCollisionShape()->getAabb(obj->getWorldTransform(), aabbMin, aabbMax);

                min.setMin(aabbMin);
                max.setMax(aabbMax);
            }

            // Calculate the center and size of the bounding box
            btVector3 center = (min + max) * 0.5;
            btVector3 size = max - min;

            // Set the camera target to the center of the geometry
            debugDrawer->cameraTarget = glm::vec3(-prepD[12], prepD[13]*0.4f, prepD[14]*2.0f);

            // Position the camera far enough to fit the geometry
            float maxDimension = std::max(size.getX(), std::max(size.getY(), size.getZ()));
            float horizontalFOV = 2.0f * atan(tan(glm::radians(debugDrawer->fov) / 2.0f) * debugDrawer->aspectRatio);
            float distance = maxDimension / (2.0f * tan(horizontalFOV / 2.0f)) / 2.0f; // Adjust the divisor to control the distance
            debugDrawer->cameraPosition = glm::vec3(-prepD[12], -prepD[13]*2.4f, -prepD[14]/0.4f);
            
            debugDrawer->fov = 7.0f; // Set the field of view

            // Ensure the up vector is correct
            debugDrawer->upVector = glm::vec3(0.0f, 1.0f, 0.0f);

            // Adjust the near and far planes
            debugDrawer->nearPlane = 0.1f;
            // debugDrawer->farPlane = distance + maxDimension * 2.0f;
            debugDrawer->farPlane = 100.0f;

            // Set the projection matrix
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
            glLoadMatrixf(glm::value_ptr(projection));

            // Set the view matrix
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
            glLoadMatrixf(glm::value_ptr(view));
            
            // printf("Camera Position: %f, %f, %f\n", debugDrawer->cameraPosition.x, debugDrawer->cameraPosition.y, debugDrawer->cameraPosition.z);
            // printf("Camera Target: %f, %f, %f\n", debugDrawer->cameraTarget.x, debugDrawer->cameraTarget.y, debugDrawer->cameraTarget.z);
            // printf("Up Vector: %f, %f, %f\n", debugDrawer->upVector.x, debugDrawer->upVector.y, debugDrawer->upVector.z);

            // printf("Bounding Box Min: %f, %f, %f\n", min.getX(), min.getY(), min.getZ());
            // printf("Bounding Box Max: %f, %f, %f\n", max.getX(), max.getY(), max.getZ());

            // Perform debug drawing
            dw.second->dynamicsWorld->debugDrawWorld();

            // test();
        }
    }    

    if (buffer == nullptr) {       
        buffer = new uint8_t[width * height * 4];
    }
    // Read the pixels from the framebuffer
    // glBindFramebuffer(GL_FRAMEBUFFER, debugDrawer->debugFBO);
    // glReadBuffer(ChaiLove::getInstance()->chai_gfx.COLORATTACH);
    
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

    // Flip framebuffer vertically
    for (int y = 0; y < height / 2; ++y) {
        int oppositeY = height - 1 - y;
        for (int x = 0; x < width * 4; ++x) {
            std::swap(buffer[y * width * 4 + x], buffer[oppositeY * width * 4 + x]);
        }
    }
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return buffer;
}

void chai_collisions::debugDraw()
{    
    printf("Debug Draw Texture: %d\n", debugDrawer->debugTexture);
    // Render texture
    // auto cg = ChaiLove::getInstance()->chai_gfx;
    // auto fb = cg.instance->hw_render.get_current_framebuffer();
    // glBindFramebuffer(GL_FRAMEBUFFER, 0); // Switch to window framebuffer
    glViewport(0, 0, 1920, 1080);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, debugDrawer->debugTexture);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1920, 0, 1080, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(1920.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(1920.0f, 1080.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1080.0f);
    glEnd();   
    glDisable(GL_TEXTURE_2D);
}

void chai_collisions::process(float deltaTime)
{    
    for (auto &dw : worlds->worlds) {
        for (auto &cc : characterControllers) {
            auto stopZ = cc->ghostObject->getUserIndex2();
            auto v = cc->character->getLinearVelocity();
            if (stopZ > 0 && v.getZ() < 0) {                
                cc->character->setWalkDirection(btVector3(v.getX(), v.getY(), 0.0f));
            }
            if (stopZ < 0 && v.getZ() > 0) {                
                cc->character->setWalkDirection(btVector3(v.getX(), v.getY(), 0.0f));
            }
            cc->character->preStep(dw.second->dynamicsWorld);
            cc->character->playerStep(dw.second->dynamicsWorld, 1);
            
            cc->character->setWalkDirection(btVector3(0.0f, 0.0f, 0.0f));

            btVector3 pos = cc->ghostObject->getWorldTransform().getOrigin();
            
            // printf("ghost pos: %f, %f, %f\n", pos.getX(), pos.getY(), pos.getZ());
        }
        dw.second->dynamicsWorld->stepSimulation(deltaTime, 10);  
        if (debugDrawer) {
            btVector3 gravity = dw.second->dynamicsWorld->getGravity();
            // printf("Gravity: %f, %f, %f\n", gravity.getX(), gravity.getY(), gravity.getZ());   

            btVector3 min, max;
            min.setValue(FLT_MAX, FLT_MAX, FLT_MAX);
            max.setValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (int i = 0; i < dw.second->dynamicsWorld->getNumCollisionObjects(); i++) {
                btCollisionObject *obj = dw.second->dynamicsWorld->getCollisionObjectArray()[i];
                btVector3 aabbMin, aabbMax;
                obj->getCollisionShape()->getAabb(obj->getWorldTransform(), aabbMin, aabbMax);

                min.setMin(aabbMin);
                max.setMax(aabbMax);
            }

            // Calculate the center and size of the bounding box
            btVector3 center = (min + max) * 0.5;
            btVector3 size = max - min;

            // Set the camera target to the center of the geometry
            debugDrawer->cameraTarget = glm::vec3(center.getX(), center.getY(), center.getZ());

            // Position the camera far enough to fit the geometry
            float maxDimension = std::max(size.getX(), std::max(size.getY(), size.getZ()));
            float horizontalFOV = 2.0f * atan(tan(glm::radians(debugDrawer->fov) / 2.0f) * debugDrawer->aspectRatio);
            float distance = maxDimension / (2.0f * tan(horizontalFOV / 2.0f)) / 2.0f; // Adjust the divisor to control the distance
            debugDrawer->cameraPosition = glm::vec3(center.getX(), center.getY(), center.getZ() + distance);

            // Ensure the up vector is correct
            debugDrawer->upVector = glm::vec3(0.0f, 1.0f, 0.0f);

            // Adjust the near and far planes
            debugDrawer->nearPlane = 0.1f;
            debugDrawer->farPlane = distance + maxDimension * 2.0f;

            // Set the projection matrix
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
            glLoadMatrixf(glm::value_ptr(projection));

            // Set the view matrix
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
            glLoadMatrixf(glm::value_ptr(view));
            
            // printf("Camera Position: %f, %f, %f\n", debugDrawer->cameraPosition.x, debugDrawer->cameraPosition.y, debugDrawer->cameraPosition.z);
            // printf("Camera Target: %f, %f, %f\n", debugDrawer->cameraTarget.x, debugDrawer->cameraTarget.y, debugDrawer->cameraTarget.z);
            // printf("Up Vector: %f, %f, %f\n", debugDrawer->upVector.x, debugDrawer->upVector.y, debugDrawer->upVector.z);

            // printf("Bounding Box Min: %f, %f, %f\n", min.getX(), min.getY(), min.getZ());
            // printf("Bounding Box Max: %f, %f, %f\n", max.getX(), max.getY(), max.getZ());

            // Perform debug drawing
            dw.second->dynamicsWorld->debugDrawWorld();

            // test();
        }
    }
}
void chai_collisions::test() {
    btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld *dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -10, 0));
    btCollisionShape *groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 1);
    btCollisionShape *fallShape = new btSphereShape(1);
    btDefaultMotionState *groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
    btRigidBody *groundRigidBody = new btRigidBody(groundRigidBodyCI);
    dynamicsWorld->addRigidBody(groundRigidBody);
    btDefaultMotionState *fallMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 50, 0)));
    btScalar mass = 1;
    btVector3 fallInertia(0, 0, 0);
    fallShape->calculateLocalInertia(mass, fallInertia);
    btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass, fallMotionState, fallShape, fallInertia);
    btRigidBody *fallRigidBody = new btRigidBody(fallRigidBodyCI);
    dynamicsWorld->addRigidBody(fallRigidBody);
    // Initialize the debug drawer
    // if (!debugDrawer) {
    //     debugDrawer = new OpenGLDebugDrawer();
    //     debugDrawer->setDebugMode(btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
    // }
    dynamicsWorld->setDebugDrawer(debugDrawer);
    // Set the projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glm::mat4 projection = glm::perspective(glm::radians(debugDrawer->fov), debugDrawer->aspectRatio, debugDrawer->nearPlane, debugDrawer->farPlane);
    glLoadMatrixf(glm::value_ptr(projection));

    // Set the view matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glm::mat4 view = glm::lookAt(debugDrawer->cameraPosition, debugDrawer->cameraTarget, debugDrawer->upVector);
    glLoadMatrixf(glm::value_ptr(view));
    for (int i = 0; i < 300; i++) {
        dynamicsWorld->stepSimulation(1 / 60.f, 10);

        // print positions of all objects
        for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--) {
            btCollisionObject *obj = dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody *body = btRigidBody::upcast(obj);
            btTransform trans;
            if (body && body->getMotionState()) {
                body->getMotionState()->getWorldTransform(trans);
            } else {
                trans = obj->getWorldTransform();
            }
            printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
            // Perform debug drawing
            dynamicsWorld->debugDrawWorld();
        }
        // break;
    }
    dynamicsWorld->removeRigidBody(fallRigidBody);
    delete fallRigidBody;
    dynamicsWorld->removeRigidBody(groundRigidBody);
    delete groundRigidBody;
    delete fallShape;
    delete groundShape;
    delete dynamicsWorld;
    delete solver;
    delete collisionConfiguration;
    delete dispatcher;
    delete broadphase;
    // delete debugDrawer;
}
} // namespace love