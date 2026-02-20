#pragma once

#include <vector>
#include <memory>

namespace pwow {
namespace rendering {

class Mesh;

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    void addMesh(std::shared_ptr<Mesh> mesh);
    void removeMesh(std::shared_ptr<Mesh> mesh);
    void clear();

    const std::vector<std::shared_ptr<Mesh>>& getMeshes() const { return meshes; }

private:
    std::vector<std::shared_ptr<Mesh>> meshes;
};

} // namespace rendering
} // namespace pwow
