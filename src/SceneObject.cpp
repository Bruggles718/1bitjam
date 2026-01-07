#include "SceneObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

VertexData::VertexData(std::vector<float> i_vertex_buffer) {
    m_vertex_buffer = i_vertex_buffer;
}

VertexData::~VertexData() {
    
}

void VertexData::add_to_vertex_buffer(float i_f) {
    m_vertex_buffer.push_back(i_f);
}

void VertexData::draw(PlaydateAPI* pd, glm::mat4& model, glm::mat4& view, glm::mat4& projection) {
    pd->system->logToConsole("Drawing VertexData with %zu floats", m_vertex_buffer.size());
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
        glm::vec4 pos = glm::vec4(x1, y1, z1, 1.0f);
        glm::vec4 world_pos = model * pos;
        glm::vec4 view_pos = view * world_pos;
        glm::vec4 clip_pos1 = projection * view_pos;
        pos = glm::vec4(x2, y2, z2, 1.0f);
        world_pos = model * pos;
        view_pos = view * world_pos;
        glm::vec4 clip_pos2 = projection * view_pos;
        pos = glm::vec4(x3, y3, z3, 1.0f);
        world_pos = model * pos;
        view_pos = view * world_pos;
        glm::vec4 clip_pos3 = projection * view_pos;
        int w = pd->display->getWidth();
        int h = pd->display->getHeight();
        pd->graphics->fillTriangle(
            ((clip_pos1.x / clip_pos1.w) + 1) * 0.5f * w,
            ((-clip_pos1.y / clip_pos1.w) + 1) * 0.5f * h,
            ((clip_pos2.x / clip_pos2.w) + 1) * 0.5f * w,
            ((-clip_pos2.y / clip_pos2.w) + 1) * 0.5f * h,
            ((clip_pos3.x / clip_pos3.w) + 1) * 0.5f * w,
            ((-clip_pos3.y / clip_pos3.w) + 1) * 0.5f * h,
            kColorBlack
        );
        // pd->system->logToConsole(
        //     "Triangle: (%f, %f), (%f, %f), (%f, %f)",
        //     ((clip_pos1.x / clip_pos1.w) + 1) * 0.5f * 400,
        //     ((-clip_pos1.y / clip_pos1.w) + 1) * 0.5f * 240,
        //     ((clip_pos2.x / clip_pos2.w) + 1) * 0.5f * 400,
        //     ((-clip_pos2.y / clip_pos2.w) + 1) * 0.5f * 240,
        //     ((clip_pos3.x / clip_pos3.w) + 1) * 0.5f * 400,
        //     ((-clip_pos3.y / clip_pos3.w) + 1) * 0.5f * 240
        // );
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

void SceneObject::draw(const Camera& i_camera, int i_screen_width, int i_screen_height, PlaydateAPI* pd) {
    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), m_transform.m_position), m_transform.m_scale);
    glm::mat4 view = i_camera.GetViewMatrix();
    glm::mat4 perspective = glm::perspective(
        glm::radians(45.0f),
        (float)i_screen_width/(float)i_screen_height,
        0.1f,
        1000.0f);
    m_vertex_data->draw(pd, model, view, perspective);
}

void SceneObject::pre_draw(const Camera& i_camera, int i_screen_width, int i_screen_height) {
    glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), m_transform.m_position), m_transform.m_scale);
    glm::mat4 view = i_camera.GetViewMatrix();
    glm::mat4 perspective = glm::perspective(
        glm::radians(45.0f),
        (float)i_screen_width/(float)i_screen_height,
        0.1f,
        1000.0f);

    // send_uniform_to_gpu(glUniformMatrix4fv, &model[0][0], "u_model");
    // send_uniform_to_gpu(glUniformMatrix4fv, &view[0][0], "u_view");
    // send_uniform_to_gpu(glUniformMatrix4fv, &perspective[0][0], "u_projection");
    // send_uniform_to_gpu(glUniform3fv, glm::value_ptr(m_diffuse_color), "u_diffuseColor");
    // send_uniform_to_gpu(glUniform1fv, &m_specular_strength, "u_specularStrength");
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
void SceneObject::set_diffuse_color(glm::vec3 i_diffuse_color) {
    m_diffuse_color = i_diffuse_color;
}
void SceneObject::set_specular_strength(float i_specular_strength) {
    m_specular_strength = i_specular_strength;
}