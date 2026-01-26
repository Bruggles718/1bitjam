#pragma once

#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include "Camera.hpp"
#include "PointLight.hpp"
#include "pd_api.h"

class VertexData {
    public:
        VertexData(std::vector<float> i_vertex_buffer);
        ~VertexData();
        void add_to_vertex_buffer(float i_f);
        virtual void send_to_gpu() = 0;
        void draw(
            PlaydateAPI* pd, 
            glm::mat4& model, 
            glm::mat4& view, 
            glm::mat4& projection, 
            float* depth_buffer,
            std::vector<std::vector<int>>& bayer_matrix
        );
        void print_vertex_buffer();
    protected:
        std::vector<float> m_vertex_buffer;
};

template<int T>
class SimpleVertexData : public VertexData {
    public:
        SimpleVertexData(
            std::vector<float> i_vertex_buffer,
            int i_stride,
            std::array<int, T> i_offsets
        );
        ~SimpleVertexData();
        void send_to_gpu();

    private:
        void setup_for_send();
        void send_buffer();
        void send_attributes();
        int get_attribute_size(int i_idx);

        int m_stride;
        std::array<int, T> m_offsets;
};

struct Transform {
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Quaternion
};

class SceneObject {
    public:
        SceneObject();
        SceneObject(std::shared_ptr<VertexData> i_vertex_data);
        ~SceneObject();

        void draw(const Camera& i_camera, PlaydateAPI* pd, float* depth_buffer,
            std::vector<std::vector<int>>& bayer_matrix);
        void set_transform(Transform i_tf);
        void set_position(glm::vec3 i_position);
        void set_rotation(glm::quat i_rotation);
        void rotate(float i_angle_degrees, glm::vec3 i_axis);
        void set_scale(glm::vec3 i_scale);
        void set_diffuse_color(glm::vec3 i_diffuse_color);
        void set_specular_strength(float i_specular_strength);
        Transform get_transform();
    private:
        void send_vertex_data_to_gpu();
        void pre_draw(const Camera& i_camera, int i_screen_width, int i_screen_height);

        std::shared_ptr<VertexData> m_vertex_data;
        
        Transform m_transform;

        glm::vec3 m_diffuse_color{1.0f, 1.0f, 1.0f};
        float m_specular_strength{1.0f};
};

#include "SimpleVertexData_impl.hpp"