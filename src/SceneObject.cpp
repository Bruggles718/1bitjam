#include "SceneObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>

VertexData::VertexData(std::vector<float> i_vertex_buffer) {
    m_vertex_buffer = i_vertex_buffer;
}

VertexData::~VertexData() {
    
}

void VertexData::add_to_vertex_buffer(float i_f) {
    m_vertex_buffer.push_back(i_f);
}

static inline float edge(
    float ax, float ay,
    float bx, float by,
    float px, float py)
{
    return (px - ax) * (by - ay)
         - (py - ay) * (bx - ax);
}

void drawLineZ(PlaydateAPI* pd, 
               std::vector<float>& depth_buffer,
               int x0, int y0, float iz0,
               int x1, int y1, float iz1, 
               uint8_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    int steps = std::max(dx, dy);
    for (int i = 0; i <= steps; i++) {
        // Linear interpolation along the line
        float t = steps == 0 ? 0 : (float)i / steps;
        float iz = iz0 * (1.0f - t) + iz1 * t;

        int idx = y * SCREEN_WIDTH + x;
        if (iz > depth_buffer[idx]) {
            depth_buffer[idx] = iz;
            pd->graphics->setPixel(x, y, color);
        }

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

void drawLineZThick(PlaydateAPI* pd,
                    std::vector<float>& depth_buffer,
                    int x0, int y0, float iz0,
                    int x1, int y1, float iz1,
                    uint8_t color, int thickness)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int steps = std::max(dx, dy);
    for (int i = 0; i <= steps; i++) {
        float tline = steps == 0 ? 0 : (float)i / steps;
        float iz = iz0 * (1.0f - tline) + iz1 * tline;

        // Compute main pixel along the line
        int xi = x0;
        int yi = y0;

        // Draw thick pixels perpendicular
        for (int ox = -thickness; ox <= thickness; ox++) {
            for (int oy = -thickness; oy <= thickness; oy++) {
                int px = xi + ox;
                int py = yi + oy;
                if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT)
                    continue;

                int idx = py * SCREEN_WIDTH + px;
                if (iz > depth_buffer[idx]) {
                    depth_buffer[idx] = iz;
                    pd->graphics->setPixel(px, py, color);
                }
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}


std::vector<std::vector<float>> bayerMatrix(int n) {
    assert((n & (n - 1)) == 0 && "Size must be a power of 2");

    std::vector<std::vector<int>> B = {{0, 2}, {3, 1}};
    int size = 2;

    while (size < n) {
        std::vector<std::vector<int>> newB(size * 2, std::vector<int>(size * 2));
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int v = B[y][x];
                newB[y][x] = 4 * v;
                newB[y][x + size] = 4 * v + 2;
                newB[y + size][x] = 4 * v + 3;
                newB[y + size][x + size] = 4 * v + 1;
            }
        }
        B = std::move(newB);
        size *= 2;
    }

    // Normalize to [0,1)
    std::vector<std::vector<float>> normalized(n, std::vector<float>(n));
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            normalized[y][x] = B[y][x] / float(n * n);
        }
    }
    return normalized;
}

std::vector<std::vector<int>> bayerDither(float brightness, int width, int height) {
    // Find next power-of-2 size >= max(width, height)
    int n = 1;
    while (n < std::max(width, height)) n *= 2;

    auto T = bayerMatrix(n);

    std::vector<std::vector<int>> pattern(height, std::vector<int>(width));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pattern[y][x] = (brightness > T[y % n][x % n]) ? kColorWhite : kColorBlack;
        }
    }

    return pattern;
}

void VertexData::draw(PlaydateAPI* pd, glm::mat4& model, glm::mat4& view, glm::mat4& projection, std::vector<float>& depth_buffer) {
    pd->system->logToConsole("Drawing VertexData with %zu floats", m_vertex_buffer.size());
    glm::mat4 mv = view * model;
    for (int i = 0; i < m_vertex_buffer.size(); i += 9) {
        float x1 = m_vertex_buffer[i];
        float y1 = m_vertex_buffer[i + 1];
        float z1 = m_vertex_buffer[i + 2];
        float x2 = m_vertex_buffer[i + 3];
        float y2 = m_vertex_buffer[i + 4];
        float z2 = m_vertex_buffer[i + 5];
        float x3 = m_vertex_buffer[i + 6];
        float y3 = m_vertex_buffer[i + 7];
        float z3 = m_vertex_buffer[i + 8];
        glm::vec4 pos1 = glm::vec4(x1, y1, z1, 1.0f);
        glm::vec4 view_pos1 = mv * pos1;
        glm::vec4 pos2 = glm::vec4(x2, y2, z2, 1.0f);
        glm::vec4 view_pos2 = mv * pos2;
        glm::vec4 pos3 = glm::vec4(x3, y3, z3, 1.0f);
        glm::vec4 view_pos3 = mv * pos3;

        if (glm::cross(glm::vec3(view_pos2 - view_pos1), glm::vec3(view_pos3 - view_pos1)).z < 0) {
            // Backface cull
            continue;
        }

        glm::vec4 clip1_v4 = projection * view_pos1;
        glm::vec4 clip2_v4 = projection * view_pos2;
        glm::vec4 clip3_v4 = projection * view_pos3;
        glm::vec3 clip1_v3 = glm::vec3(clip1_v4) / clip1_v4.w;
        glm::vec3 clip2_v3 = glm::vec3(clip2_v4) / clip2_v4.w;
        glm::vec3 clip3_v3 = glm::vec3(clip3_v4) / clip3_v4.w;
        glm::vec2 clip1 = glm::vec2((clip1_v3.x + 1) * SCREEN_WIDTH, (1 - clip1_v3.y) * SCREEN_HEIGHT) * 0.5f;
        glm::vec2 clip2 = glm::vec2((clip2_v3.x + 1) * SCREEN_WIDTH, (1 - clip2_v3.y) * SCREEN_HEIGHT) * 0.5f;
        glm::vec2 clip3 = glm::vec2((clip3_v3.x + 1) * SCREEN_WIDTH, (1 - clip3_v3.y) * SCREEN_HEIGHT) * 0.5f;
        
        int minX = std::max(0, (int)std::floor(std::min({clip1.x, clip2.x, clip3.x})));
        int maxX = std::min(SCREEN_WIDTH - 1, (int)std::ceil(std::max({clip1.x, clip2.x, clip3.x})));
        int minY = std::max(0, (int)std::floor(std::min({clip1.y, clip2.y, clip3.y})));
        int maxY = std::min(SCREEN_HEIGHT - 1, (int)std::ceil(std::max({clip1.y, clip2.y, clip3.y})));
        float vz0 = -view_pos1.z;
        float vz1 = -view_pos2.z;
        float vz2 = -view_pos3.z;

        float iz0 = 1.0f / vz0;
        float iz1 = 1.0f / vz1;
        float iz2 = 1.0f / vz2;

        glm::vec3 normal = glm::cross(
            glm::vec3(view_pos2 - view_pos1),
            glm::vec3(view_pos3 - view_pos1)
        );

        float dot = glm::dot(normal, glm::vec3(0.0f, 1.0f, 0.0f));
        int width = maxX - minX;
        int height = maxY - minY;
        float brightness = (dot + 1.0f) * 0.5f;
        int n = 1;
        while (n < std::max(width, height)) n *= 2;

        auto T = bayerMatrix(n);

        int lineThickness = 1;

        for (int i = minX + lineThickness; i <= maxX - lineThickness; i++) {
            for (int j = minY + lineThickness; j <= maxY - lineThickness; j++) {
                float px = i + 0.5f;
                float py = j + 0.5f;

                float alpha =
                    ((clip2.y - clip3.y) * (px - clip3.x) +
                    (clip3.x - clip2.x) * (py - clip3.y)) /
                    ((clip2.y - clip3.y) * (clip1.x - clip3.x) +
                    (clip3.x - clip2.x) * (clip1.y - clip3.y));

                float beta =
                    ((clip3.y - clip1.y) * (px - clip3.x) +
                    (clip1.x - clip3.x) * (py - clip3.y)) /
                    ((clip3.y - clip1.y) * (clip2.x - clip3.x) +
                    (clip1.x - clip3.x) * (clip2.y - clip3.y));

                float gamma = 1.0f - alpha - beta;

                if (alpha < 0 || beta < 0 || gamma < 0)
                    continue;

                float iz = alpha * iz0 + beta * iz1 + gamma * iz2;

                int idx = j * SCREEN_WIDTH + i;

                // -------- DEPTH TEST --------
                if (iz > depth_buffer[idx]) {
                    depth_buffer[idx] = iz;
                    int y = j - minY;
                    int x = i - minX;
                    int color = (brightness > T[y % n][x % n]) ? kColorWhite : kColorBlack;
                    pd->graphics->setPixel(i, j, color);
                }
            }
        }
        drawLineZThick(pd, depth_buffer,
                  (int)clip1.x, (int)clip1.y, iz0,
                  (int)clip2.x, (int)clip2.y, iz1,
                  kColorBlack, lineThickness);
        drawLineZThick(pd, depth_buffer,
                  (int)clip2.x, (int)clip2.y, iz1,
                  (int)clip3.x, (int)clip3.y, iz2,
                  kColorBlack, lineThickness);
        drawLineZThick(pd, depth_buffer,
                  (int)clip3.x, (int)clip3.y, iz2,
                  (int)clip1.x, (int)clip1.y, iz0,
                  kColorBlack, lineThickness);
    }
    
}

void VertexData::print_vertex_buffer() {
    for (float f : m_vertex_buffer) {
        std::cout << f << std::endl;
    }
}

SceneObject::SceneObject(std::shared_ptr<VertexData> i_vertex_data) {
    m_vertex_data = i_vertex_data;
    
}

SceneObject::~SceneObject() {
    // Delete our Graphics pipeline
    
}

void SceneObject::send_vertex_data_to_gpu() {
    m_vertex_data->send_to_gpu();
}

void SceneObject::draw(const Camera& i_camera, PlaydateAPI* pd, std::vector<float>& depth_buffer) {
    // glm::mat4 model = glm::translate(glm::mat4(1.0f), m_transform.m_position);
    // model = model * glm::mat4_cast(m_transform.m_rotation);
    // model = glm::scale(model, m_transform.m_scale);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_transform.m_position)
                * glm::mat4_cast(m_transform.m_rotation)
                * glm::scale(glm::mat4(1.0f), m_transform.m_scale);

    glm::mat4 view = i_camera.GetViewMatrix();
    glm::mat4 perspective = glm::perspective(
        glm::radians(45.0f),
        (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
        0.1f,
        1000.0f);
    m_vertex_data->draw(pd, model, view, perspective, depth_buffer);
}

void SceneObject::set_transform(Transform i_tf) {
    m_transform = i_tf;
}
void SceneObject::set_position(glm::vec3 i_position) {
    m_transform.m_position = i_position;
}
void SceneObject::set_scale(glm::vec3 i_scale) {
    m_transform.m_scale = i_scale;
}
void SceneObject::set_rotation(glm::quat i_rotation) {
    m_transform.m_rotation = i_rotation;
}
void SceneObject::rotate(float i_angle_degrees, glm::vec3 i_axis) {
    glm::quat rotation_quat = glm::angleAxis(glm::radians(i_angle_degrees), glm::normalize(i_axis));
    m_transform.m_rotation = rotation_quat * m_transform.m_rotation;
}
void SceneObject::set_diffuse_color(glm::vec3 i_diffuse_color) {
    m_diffuse_color = i_diffuse_color;
}
void SceneObject::set_specular_strength(float i_specular_strength) {
    m_specular_strength = i_specular_strength;
}