#pragma once

#include <glm/glm.hpp>

struct PointLight {
    glm::vec4 m_light_pos{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 m_light_color{1.0f, 1.0f, 1.0f, 1.0f};
    float m_ambientIntensity = 0.25f;
    float m_attenuation_constant = 1.0f;
    float m_attenuation_linear = 0.09f;
    float m_attenuation_quadratic = 0.032f;
};