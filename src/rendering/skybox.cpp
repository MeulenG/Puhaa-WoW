#include "rendering/skybox.hpp"
#include "rendering/shader.hpp"
#include "rendering/camera.hpp"
#include "core/logger.hpp"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

namespace wowee {
namespace rendering {

Skybox::Skybox() = default;

Skybox::~Skybox() {
    shutdown();
}

bool Skybox::initialize() {
    LOG_INFO("Initializing skybox");

    // Create sky shader
    skyShader = std::make_unique<Shader>();

    // Vertex shader - position-only skybox
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;

        uniform mat4 view;
        uniform mat4 projection;

        out vec3 WorldPos;
        out float Altitude;

        void main() {
            WorldPos = aPos;

            // Calculate altitude (0 at horizon, 1 at zenith)
            Altitude = normalize(aPos).z;

            // Remove translation from view matrix (keep rotation only)
            mat4 viewNoTranslation = mat4(mat3(view));

            gl_Position = projection * viewNoTranslation * vec4(aPos, 1.0);

            // Ensure skybox is always at far plane
            gl_Position = gl_Position.xyww;
        }
    )";

    // Fragment shader - gradient sky with time of day
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 WorldPos;
        in float Altitude;

        uniform vec3 horizonColor;
        uniform vec3 zenithColor;
        uniform float timeOfDay;

        out vec4 FragColor;

        void main() {
            // Smooth gradient from horizon to zenith
            float t = pow(max(Altitude, 0.0), 0.5);  // Curve for more interesting gradient

            vec3 skyColor = mix(horizonColor, zenithColor, t);

            // Add atmospheric scattering effect (more saturated near horizon)
            float scattering = 1.0 - t * 0.3;
            skyColor *= scattering;

            FragColor = vec4(skyColor, 1.0);
        }
    )";

    if (!skyShader->loadFromSource(vertexShaderSource, fragmentShaderSource)) {
        LOG_ERROR("Failed to create sky shader");
        return false;
    }

    // Create sky dome mesh
    createSkyDome();

    LOG_INFO("Skybox initialized");
    return true;
}

void Skybox::shutdown() {
    destroySkyDome();
    skyShader.reset();
}

void Skybox::render(const Camera& camera, float time) {
    if (!renderingEnabled || vao == 0 || !skyShader) {
        return;
    }

    // Render skybox first (before terrain), with depth test set to LEQUAL
    glDepthFunc(GL_LEQUAL);

    skyShader->use();

    // Set uniforms
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();

    skyShader->setUniform("view", view);
    skyShader->setUniform("projection", projection);
    skyShader->setUniform("timeOfDay", time);

    // Get colors based on time of day
    glm::vec3 horizon = getHorizonColor(time);
    glm::vec3 zenith = getZenithColor(time);

    skyShader->setUniform("horizonColor", horizon);
    skyShader->setUniform("zenithColor", zenith);

    // Render dome
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // Restore depth function
    glDepthFunc(GL_LESS);
}

void Skybox::update(float deltaTime) {
    if (timeProgressionEnabled) {
        timeOfDay += deltaTime * timeSpeed;

        // Wrap around 24 hours
        if (timeOfDay >= 24.0f) {
            timeOfDay -= 24.0f;
        }
    }
}

void Skybox::setTimeOfDay(float time) {
    // Clamp to 0-24 range
    while (time < 0.0f) time += 24.0f;
    while (time >= 24.0f) time -= 24.0f;

    timeOfDay = time;
}

void Skybox::createSkyDome() {
    // Create an extended dome that goes below horizon for better coverage
    const int rings = 16;      // Vertical resolution
    const int sectors = 32;    // Horizontal resolution
    const float radius = 2000.0f;  // Large enough to cover view without looking curved

    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    // Generate vertices - extend slightly below horizon
    const float minPhi = -M_PI / 12.0f;  // Start 15° below horizon
    const float maxPhi = M_PI / 2.0f;     // End at zenith
    for (int ring = 0; ring <= rings; ring++) {
        float phi = minPhi + (maxPhi - minPhi) * (static_cast<float>(ring) / rings);
        float y = radius * std::sin(phi);
        float ringRadius = radius * std::cos(phi);

        for (int sector = 0; sector <= sectors; sector++) {
            float theta = (2.0f * M_PI) * (static_cast<float>(sector) / sectors);
            float x = ringRadius * std::cos(theta);
            float z = ringRadius * std::sin(theta);

            // Position
            vertices.push_back(x);
            vertices.push_back(z);  // Z up in WoW coordinates
            vertices.push_back(y);
        }
    }

    // Generate indices
    for (int ring = 0; ring < rings; ring++) {
        for (int sector = 0; sector < sectors; sector++) {
            int current = ring * (sectors + 1) + sector;
            int next = current + sectors + 1;

            // Two triangles per quad
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    indexCount = static_cast<int>(indices.size());

    // Create OpenGL buffers
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Set vertex attributes (position only)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    LOG_DEBUG("Sky dome created: ", (rings + 1) * (sectors + 1), " vertices, ", indexCount / 3, " triangles");
}

void Skybox::destroySkyDome() {
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (ebo != 0) {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
}

glm::vec3 Skybox::getHorizonColor(float time) const {
    // Time-based horizon colors
    // 0-6: Night (dark blue)
    // 6-8: Dawn (orange/pink)
    // 8-16: Day (light blue)
    // 16-18: Dusk (orange/red)
    // 18-24: Night (dark blue)

    if (time < 5.0f || time >= 21.0f) {
        // Night - dark blue/purple horizon
        return glm::vec3(0.05f, 0.05f, 0.15f);
    }
    else if (time >= 5.0f && time < 7.0f) {
        // Dawn - blend from night to orange
        float t = (time - 5.0f) / 2.0f;
        glm::vec3 night = glm::vec3(0.05f, 0.05f, 0.15f);
        glm::vec3 dawn = glm::vec3(1.0f, 0.5f, 0.2f);
        return glm::mix(night, dawn, t);
    }
    else if (time >= 7.0f && time < 9.0f) {
        // Morning - blend from orange to blue
        float t = (time - 7.0f) / 2.0f;
        glm::vec3 dawn = glm::vec3(1.0f, 0.5f, 0.2f);
        glm::vec3 day = glm::vec3(0.6f, 0.7f, 0.9f);
        return glm::mix(dawn, day, t);
    }
    else if (time >= 9.0f && time < 17.0f) {
        // Day - light blue horizon
        return glm::vec3(0.6f, 0.7f, 0.9f);
    }
    else if (time >= 17.0f && time < 19.0f) {
        // Dusk - blend from blue to orange/red
        float t = (time - 17.0f) / 2.0f;
        glm::vec3 day = glm::vec3(0.6f, 0.7f, 0.9f);
        glm::vec3 dusk = glm::vec3(1.0f, 0.4f, 0.1f);
        return glm::mix(day, dusk, t);
    }
    else {
        // Evening - blend from orange to night
        float t = (time - 19.0f) / 2.0f;
        glm::vec3 dusk = glm::vec3(1.0f, 0.4f, 0.1f);
        glm::vec3 night = glm::vec3(0.05f, 0.05f, 0.15f);
        return glm::mix(dusk, night, t);
    }
}

glm::vec3 Skybox::getZenithColor(float time) const {
    // Zenith (top of sky) colors

    if (time < 5.0f || time >= 21.0f) {
        // Night - very dark blue, almost black
        return glm::vec3(0.01f, 0.01f, 0.05f);
    }
    else if (time >= 5.0f && time < 7.0f) {
        // Dawn - blend from night to light blue
        float t = (time - 5.0f) / 2.0f;
        glm::vec3 night = glm::vec3(0.01f, 0.01f, 0.05f);
        glm::vec3 dawn = glm::vec3(0.3f, 0.4f, 0.7f);
        return glm::mix(night, dawn, t);
    }
    else if (time >= 7.0f && time < 9.0f) {
        // Morning - blend to bright blue
        float t = (time - 7.0f) / 2.0f;
        glm::vec3 dawn = glm::vec3(0.3f, 0.4f, 0.7f);
        glm::vec3 day = glm::vec3(0.2f, 0.5f, 1.0f);
        return glm::mix(dawn, day, t);
    }
    else if (time >= 9.0f && time < 17.0f) {
        // Day - bright blue zenith
        return glm::vec3(0.2f, 0.5f, 1.0f);
    }
    else if (time >= 17.0f && time < 19.0f) {
        // Dusk - blend to darker blue
        float t = (time - 17.0f) / 2.0f;
        glm::vec3 day = glm::vec3(0.2f, 0.5f, 1.0f);
        glm::vec3 dusk = glm::vec3(0.1f, 0.2f, 0.4f);
        return glm::mix(day, dusk, t);
    }
    else {
        // Evening - blend to night
        float t = (time - 19.0f) / 2.0f;
        glm::vec3 dusk = glm::vec3(0.1f, 0.2f, 0.4f);
        glm::vec3 night = glm::vec3(0.01f, 0.01f, 0.05f);
        return glm::mix(dusk, night, t);
    }
}

glm::vec3 Skybox::getSkyColor(float altitude, float time) const {
    // Blend between horizon and zenith based on altitude
    glm::vec3 horizon = getHorizonColor(time);
    glm::vec3 zenith = getZenithColor(time);

    // Use power curve for more natural gradient
    float t = std::pow(std::max(altitude, 0.0f), 0.5f);

    return glm::mix(horizon, zenith, t);
}

} // namespace rendering
} // namespace wowee
