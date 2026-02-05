#pragma once

#include "ModelFileLoader.hpp"
#include "pd_api.h"

struct WFFace {
    public:
        WFFace(std::vector<std::string> &i_tokenized_line);
        ~WFFace();

        int vertex_count();
        int get_vertex_idx(int face_idx);
        bool vertex_has_texture(int idx);
        bool vertex_has_normal(int idx);
        int get_vertex_texture(int idx);
        int get_vertex_normal(int idx);

    private:
        void add_vertices(std::vector<std::string> &i_tokenized_line);
        void add_vertex(std::string &i_vert);

        std::vector<std::array<int, 3>> m_vertices;
};

class WFObjLoader : public ModelFileLoader {
    public:
        WFObjLoader();
        ~WFObjLoader();

        SceneObject create_scene_object_from_file(std::string i_filepath, PlaydateAPI* pd);

    private:
        std::vector<float> get_simple_vertex_buffer();
        void parse_obj_file(std::string i_filepath, PlaydateAPI* pd);
        void parse_obj_file_line(std::string i_line);
        void add_vertex(std::vector<std::string> &i_tokenized_line);
        void add_vertex_normal(std::vector<std::string> &i_tokenized_line);
        glm::vec3 tokenized_line_to_vec3(std::vector<std::string> &i_tokenized_line);
        void add_face(std::vector<std::string> &i_tokenized_line);
        std::vector<float> face_to_simple_vertex_buffer(WFFace &i_face);
        std::vector<int> face_to_index_buffer(WFFace &i_face);
        std::vector<int> face_to_normal_index_buffer(WFFace &i_face);
        std::vector<float> get_ordered_vertex_data_buffer();
        std::vector<int> get_index_buffer();
        std::vector<float> get_ordered_normal_buffer();
        std::vector<int> get_normal_index_buffer();
        void compute_and_store_normals();
        void compute_and_store_normals_for_face(WFFace &i_face);

        std::vector<glm::vec4> m_vertices;
        std::vector<glm::vec3> m_vertex_normals;
        std::vector<WFFace> m_faces;
};

std::vector<std::string> tokenize(
    std::string stringToSplit,
    std::string delimeter);

int count_occurrences(
    std::string stringToScan, 
    char charToCount);

template <typename T>
std::vector<T> concat(std::vector<T> &a, std::vector<T> &b);