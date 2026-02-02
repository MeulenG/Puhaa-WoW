#include "rendering/weather.hpp"
#include "rendering/camera.hpp"
#include "rendering/shader.hpp"
#include "core/logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <cmath>

namespace wowee {
namespace rendering {

Weather::Weather() {
}

Weather::~Weather() {
    cleanup();
}

bool Weather::initialize() {
    LOG_INFO("Initializing weather system");

    // Create shader
    shader = std::make_unique<Shader>();

    // Vertex shader - point sprites with instancing
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;

        uniform mat4 uView;
        uniform mat4 uProjection;
        uniform float uParticleSize;

        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
            gl_PointSize = uParticleSize;
        }
    )";

    // Fragment shader - simple particle with alpha
    const char* fragmentShaderSource = R"(
        #version 330 core

        uniform vec4 uParticleColor;

        out vec4 FragColor;

        void main() {
            // Circular particle shape
            vec2 coord = gl_PointCoord - vec2(0.5);
            float dist = length(coord);

            if (dist > 0.5) {
                discard;
            }

            // Soft edges
            float alpha = smoothstep(0.5, 0.3, dist) * uParticleColor.a;

            FragColor = vec4(uParticleColor.rgb, alpha);
        }
    )";

    if (!shader->loadFromSource(vertexShaderSource, fragmentShaderSource)) {
        LOG_ERROR("Failed to create weather shader");
        return false;
    }

    // Create VAO and VBO for particle positions
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Reserve space for particles
    particles.reserve(MAX_PARTICLES);
    particlePositions.reserve(MAX_PARTICLES);

    LOG_INFO("Weather system initialized");
    return true;
}

void Weather::update(const Camera& camera, float deltaTime) {
    if (!enabled || weatherType == Type::NONE) {
        return;
    }

    // Initialize particles if needed
    if (particles.empty()) {
        resetParticles(camera);
    }

    // Calculate active particle count based on intensity
    int targetParticleCount = static_cast<int>(MAX_PARTICLES * intensity);

    // Adjust particle count
    while (static_cast<int>(particles.size()) < targetParticleCount) {
        Particle p;
        p.position = getRandomPosition(camera.getPosition());
        p.position.y = camera.getPosition().y + SPAWN_HEIGHT;
        p.lifetime = 0.0f;

        if (weatherType == Type::RAIN) {
            p.velocity = glm::vec3(0.0f, -50.0f, 0.0f);  // Fast downward
            p.maxLifetime = 5.0f;
        } else {  // SNOW
            p.velocity = glm::vec3(0.0f, -5.0f, 0.0f);   // Slow downward
            p.maxLifetime = 10.0f;
        }

        particles.push_back(p);
    }

    while (static_cast<int>(particles.size()) > targetParticleCount) {
        particles.pop_back();
    }

    // Update each particle
    for (auto& particle : particles) {
        updateParticle(particle, camera, deltaTime);
    }

    // Update position buffer
    particlePositions.clear();
    for (const auto& particle : particles) {
        particlePositions.push_back(particle.position);
    }
}

void Weather::updateParticle(Particle& particle, const Camera& camera, float deltaTime) {
    // Update lifetime
    particle.lifetime += deltaTime;

    // Reset if lifetime exceeded or too far from camera
    glm::vec3 cameraPos = camera.getPosition();
    float distance = glm::length(particle.position - cameraPos);

    if (particle.lifetime >= particle.maxLifetime || distance > SPAWN_VOLUME_SIZE ||
        particle.position.y < cameraPos.y - 20.0f) {
        // Respawn at top
        particle.position = getRandomPosition(cameraPos);
        particle.position.y = cameraPos.y + SPAWN_HEIGHT;
        particle.lifetime = 0.0f;
    }

    // Add wind effect for snow
    if (weatherType == Type::SNOW) {
        float windX = std::sin(particle.lifetime * 0.5f) * 2.0f;
        float windZ = std::cos(particle.lifetime * 0.3f) * 2.0f;
        particle.velocity.x = windX;
        particle.velocity.z = windZ;
    }

    // Update position
    particle.position += particle.velocity * deltaTime;
}

void Weather::render(const Camera& camera) {
    if (!enabled || weatherType == Type::NONE || particlePositions.empty() || !shader) {
        return;
    }

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth write (particles are transparent)
    glDepthMask(GL_FALSE);

    // Enable point sprites
    glEnable(GL_PROGRAM_POINT_SIZE);

    shader->use();

    // Set matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();

    shader->setUniform("uView", view);
    shader->setUniform("uProjection", projection);

    // Set particle appearance based on weather type
    if (weatherType == Type::RAIN) {
        // Rain: white/blue streaks, small size
        shader->setUniform("uParticleColor", glm::vec4(0.7f, 0.8f, 0.9f, 0.6f));
        shader->setUniform("uParticleSize", 3.0f);
    } else {  // SNOW
        // Snow: white fluffy, larger size
        shader->setUniform("uParticleColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
        shader->setUniform("uParticleSize", 8.0f);
    }

    // Upload particle positions
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 particlePositions.size() * sizeof(glm::vec3),
                 particlePositions.data(),
                 GL_DYNAMIC_DRAW);

    // Render particles as points
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particlePositions.size()));

    glBindVertexArray(0);

    // Restore state
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void Weather::resetParticles(const Camera& camera) {
    particles.clear();

    int particleCount = static_cast<int>(MAX_PARTICLES * intensity);
    glm::vec3 cameraPos = camera.getPosition();

    for (int i = 0; i < particleCount; ++i) {
        Particle p;
        p.position = getRandomPosition(cameraPos);
        p.position.y = cameraPos.y + SPAWN_HEIGHT * (static_cast<float>(rand()) / RAND_MAX);
        p.lifetime = 0.0f;

        if (weatherType == Type::RAIN) {
            p.velocity = glm::vec3(0.0f, -50.0f, 0.0f);
            p.maxLifetime = 5.0f;
        } else {  // SNOW
            p.velocity = glm::vec3(0.0f, -5.0f, 0.0f);
            p.maxLifetime = 10.0f;
        }

        particles.push_back(p);
    }
}

glm::vec3 Weather::getRandomPosition(const glm::vec3& center) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    float x = center.x + dist(gen) * SPAWN_VOLUME_SIZE;
    float z = center.z + dist(gen) * SPAWN_VOLUME_SIZE;
    float y = center.y;

    return glm::vec3(x, y, z);
}

void Weather::setIntensity(float intensity) {
    this->intensity = glm::clamp(intensity, 0.0f, 1.0f);
}

int Weather::getParticleCount() const {
    return static_cast<int>(particles.size());
}

void Weather::cleanup() {
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
}

} // namespace rendering
} // namespace wowee
