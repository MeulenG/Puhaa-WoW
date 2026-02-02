#include "rendering/swim_effects.hpp"
#include "rendering/camera.hpp"
#include "rendering/camera_controller.hpp"
#include "rendering/water_renderer.hpp"
#include "rendering/shader.hpp"
#include "core/logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <cmath>

namespace wowee {
namespace rendering {

static std::mt19937& rng() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

static float randFloat(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng());
}

SwimEffects::SwimEffects() = default;
SwimEffects::~SwimEffects() { shutdown(); }

bool SwimEffects::initialize() {
    LOG_INFO("Initializing swim effects");

    // --- Ripple/splash shader (small white spray droplets) ---
    rippleShader = std::make_unique<Shader>();

    const char* rippleVS = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in float aSize;
        layout (location = 2) in float aAlpha;

        uniform mat4 uView;
        uniform mat4 uProjection;

        out float vAlpha;

        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
            gl_PointSize = aSize;
            vAlpha = aAlpha;
        }
    )";

    const char* rippleFS = R"(
        #version 330 core
        in float vAlpha;
        out vec4 FragColor;

        void main() {
            vec2 coord = gl_PointCoord - vec2(0.5);
            float dist = length(coord);
            if (dist > 0.5) discard;
            // Soft circular splash droplet
            float alpha = smoothstep(0.5, 0.2, dist) * vAlpha;
            FragColor = vec4(0.85, 0.92, 1.0, alpha);
        }
    )";

    if (!rippleShader->loadFromSource(rippleVS, rippleFS)) {
        LOG_ERROR("Failed to create ripple shader");
        return false;
    }

    // --- Bubble shader ---
    bubbleShader = std::make_unique<Shader>();

    const char* bubbleVS = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in float aSize;
        layout (location = 2) in float aAlpha;

        uniform mat4 uView;
        uniform mat4 uProjection;

        out float vAlpha;

        void main() {
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
            gl_PointSize = aSize;
            vAlpha = aAlpha;
        }
    )";

    const char* bubbleFS = R"(
        #version 330 core
        in float vAlpha;
        out vec4 FragColor;

        void main() {
            vec2 coord = gl_PointCoord - vec2(0.5);
            float dist = length(coord);
            if (dist > 0.5) discard;
            // Bubble with highlight
            float edge = smoothstep(0.5, 0.35, dist);
            float hollow = smoothstep(0.25, 0.35, dist);
            float bubble = edge * hollow;
            // Specular highlight near top-left
            float highlight = smoothstep(0.3, 0.0, length(coord - vec2(-0.12, -0.12)));
            float alpha = (bubble * 0.6 + highlight * 0.4) * vAlpha;
            vec3 color = vec3(0.7, 0.85, 1.0);
            FragColor = vec4(color, alpha);
        }
    )";

    if (!bubbleShader->loadFromSource(bubbleVS, bubbleFS)) {
        LOG_ERROR("Failed to create bubble shader");
        return false;
    }

    // --- Ripple VAO/VBO ---
    glGenVertexArrays(1, &rippleVAO);
    glGenBuffers(1, &rippleVBO);
    glBindVertexArray(rippleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rippleVBO);
    // layout: vec3 pos, float size, float alpha  (stride = 5 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // --- Bubble VAO/VBO ---
    glGenVertexArrays(1, &bubbleVAO);
    glGenBuffers(1, &bubbleVBO);
    glBindVertexArray(bubbleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bubbleVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    ripples.reserve(MAX_RIPPLE_PARTICLES);
    bubbles.reserve(MAX_BUBBLE_PARTICLES);
    rippleVertexData.reserve(MAX_RIPPLE_PARTICLES * 5);
    bubbleVertexData.reserve(MAX_BUBBLE_PARTICLES * 5);

    LOG_INFO("Swim effects initialized");
    return true;
}

void SwimEffects::shutdown() {
    if (rippleVAO) { glDeleteVertexArrays(1, &rippleVAO); rippleVAO = 0; }
    if (rippleVBO) { glDeleteBuffers(1, &rippleVBO); rippleVBO = 0; }
    if (bubbleVAO) { glDeleteVertexArrays(1, &bubbleVAO); bubbleVAO = 0; }
    if (bubbleVBO) { glDeleteBuffers(1, &bubbleVBO); bubbleVBO = 0; }
    rippleShader.reset();
    bubbleShader.reset();
    ripples.clear();
    bubbles.clear();
}

void SwimEffects::spawnRipple(const glm::vec3& pos, const glm::vec3& moveDir, float waterH) {
    if (static_cast<int>(ripples.size()) >= MAX_RIPPLE_PARTICLES) return;

    Particle p;
    // Scatter splash droplets around the character at the water surface
    float ox = randFloat(-1.5f, 1.5f);
    float oy = randFloat(-1.5f, 1.5f);
    p.position = glm::vec3(pos.x + ox, pos.y + oy, waterH + 0.3f);

    // Spray outward + upward from movement direction
    float spread = randFloat(-1.0f, 1.0f);
    glm::vec3 perp(-moveDir.y, moveDir.x, 0.0f);
    glm::vec3 outDir = -moveDir + perp * spread;
    float speed = randFloat(1.5f, 4.0f);
    p.velocity = glm::vec3(outDir.x * speed, outDir.y * speed, randFloat(1.0f, 3.0f));

    p.lifetime = 0.0f;
    p.maxLifetime = randFloat(0.5f, 1.0f);
    p.size = randFloat(3.0f, 7.0f);
    p.alpha = randFloat(0.5f, 0.8f);

    ripples.push_back(p);
}

void SwimEffects::spawnBubble(const glm::vec3& pos, float /*waterH*/) {
    if (static_cast<int>(bubbles.size()) >= MAX_BUBBLE_PARTICLES) return;

    Particle p;
    float ox = randFloat(-3.0f, 3.0f);
    float oy = randFloat(-3.0f, 3.0f);
    float oz = randFloat(-2.0f, 0.0f);
    p.position = glm::vec3(pos.x + ox, pos.y + oy, pos.z + oz);

    p.velocity = glm::vec3(randFloat(-0.3f, 0.3f), randFloat(-0.3f, 0.3f), randFloat(4.0f, 8.0f));
    p.lifetime = 0.0f;
    p.maxLifetime = randFloat(2.0f, 3.5f);
    p.size = randFloat(6.0f, 12.0f);
    p.alpha = 0.6f;

    bubbles.push_back(p);
}

void SwimEffects::update(const Camera& camera, const CameraController& cc,
                         const WaterRenderer& water, float deltaTime) {
    glm::vec3 camPos = camera.getPosition();

    // Use character position for ripples in third-person mode
    glm::vec3 charPos = camPos;
    const glm::vec3* followTarget = cc.getFollowTarget();
    if (cc.isThirdPerson() && followTarget) {
        charPos = *followTarget;
    }

    // Check water at character position (for ripples) and camera position (for bubbles)
    auto charWaterH = water.getWaterHeightAt(charPos.x, charPos.y);
    auto camWaterH = water.getWaterHeightAt(camPos.x, camPos.y);

    bool swimming = cc.isSwimming();
    bool moving = cc.isMoving();

    // --- Ripple/splash spawning ---
    if (swimming && charWaterH) {
        float wh = *charWaterH;
        float spawnRate = moving ? 40.0f : 8.0f;
        rippleSpawnAccum += spawnRate * deltaTime;

        // Compute movement direction from camera yaw
        float yawRad = glm::radians(cc.getYaw());
        glm::vec3 moveDir(std::cos(yawRad), std::sin(yawRad), 0.0f);
        if (glm::length(glm::vec2(moveDir)) > 0.001f) {
            moveDir = glm::normalize(moveDir);
        }

        while (rippleSpawnAccum >= 1.0f) {
            spawnRipple(charPos, moveDir, wh);
            rippleSpawnAccum -= 1.0f;
        }
    } else {
        rippleSpawnAccum = 0.0f;
        ripples.clear();
    }

    // --- Bubble spawning ---
    bool underwater = camWaterH && camPos.z < *camWaterH;
    if (underwater) {
        float bubbleRate = 20.0f;
        bubbleSpawnAccum += bubbleRate * deltaTime;
        while (bubbleSpawnAccum >= 1.0f) {
            spawnBubble(camPos, *camWaterH);
            bubbleSpawnAccum -= 1.0f;
        }
    } else {
        bubbleSpawnAccum = 0.0f;
        bubbles.clear();
    }

    // --- Update ripples (splash droplets with gravity) ---
    for (int i = static_cast<int>(ripples.size()) - 1; i >= 0; --i) {
        auto& p = ripples[i];
        p.lifetime += deltaTime;
        if (p.lifetime >= p.maxLifetime) {
            ripples[i] = ripples.back();
            ripples.pop_back();
            continue;
        }
        // Apply gravity to splash droplets
        p.velocity.z -= 9.8f * deltaTime;
        p.position += p.velocity * deltaTime;

        // Kill if fallen back below water
        float surfaceZ = charWaterH ? *charWaterH : 0.0f;
        if (p.position.z < surfaceZ && p.lifetime > 0.1f) {
            ripples[i] = ripples.back();
            ripples.pop_back();
            continue;
        }

        float t = p.lifetime / p.maxLifetime;
        p.alpha = glm::mix(0.7f, 0.0f, t);
        p.size = glm::mix(5.0f, 2.0f, t);
    }

    // --- Update bubbles ---
    float bubbleCeilH = camWaterH ? *camWaterH : 0.0f;
    for (int i = static_cast<int>(bubbles.size()) - 1; i >= 0; --i) {
        auto& p = bubbles[i];
        p.lifetime += deltaTime;
        if (p.lifetime >= p.maxLifetime || p.position.z >= bubbleCeilH) {
            bubbles[i] = bubbles.back();
            bubbles.pop_back();
            continue;
        }
        // Wobble
        float wobbleX = std::sin(p.lifetime * 3.0f) * 0.5f;
        float wobbleY = std::cos(p.lifetime * 2.5f) * 0.5f;
        p.position += (p.velocity + glm::vec3(wobbleX, wobbleY, 0.0f)) * deltaTime;

        float t = p.lifetime / p.maxLifetime;
        if (t > 0.8f) {
            p.alpha = 0.6f * (1.0f - (t - 0.8f) / 0.2f);
        } else {
            p.alpha = 0.6f;
        }
    }

    // --- Build vertex data ---
    rippleVertexData.clear();
    for (const auto& p : ripples) {
        rippleVertexData.push_back(p.position.x);
        rippleVertexData.push_back(p.position.y);
        rippleVertexData.push_back(p.position.z);
        rippleVertexData.push_back(p.size);
        rippleVertexData.push_back(p.alpha);
    }

    bubbleVertexData.clear();
    for (const auto& p : bubbles) {
        bubbleVertexData.push_back(p.position.x);
        bubbleVertexData.push_back(p.position.y);
        bubbleVertexData.push_back(p.position.z);
        bubbleVertexData.push_back(p.size);
        bubbleVertexData.push_back(p.alpha);
    }
}

void SwimEffects::render(const Camera& camera) {
    if (rippleVertexData.empty() && bubbleVertexData.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();

    // --- Render ripples (splash droplets above water surface) ---
    if (!rippleVertexData.empty() && rippleShader) {
        rippleShader->use();
        rippleShader->setUniform("uView", view);
        rippleShader->setUniform("uProjection", projection);

        glBindVertexArray(rippleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, rippleVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     rippleVertexData.size() * sizeof(float),
                     rippleVertexData.data(),
                     GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(rippleVertexData.size() / 5));
        glBindVertexArray(0);
    }

    // --- Render bubbles ---
    if (!bubbleVertexData.empty() && bubbleShader) {
        bubbleShader->use();
        bubbleShader->setUniform("uView", view);
        bubbleShader->setUniform("uProjection", projection);

        glBindVertexArray(bubbleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, bubbleVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     bubbleVertexData.size() * sizeof(float),
                     bubbleVertexData.data(),
                     GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bubbleVertexData.size() / 5));
        glBindVertexArray(0);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

} // namespace rendering
} // namespace wowee
