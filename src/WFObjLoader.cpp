#include "WFObjLoader.hpp"
#include <fstream>
#include <iostream>

WFObjLoader::WFObjLoader() {}
WFObjLoader::~WFObjLoader() {}

SceneObject WFObjLoader::create_scene_object_from_file(
    std::string i_filepath, PlaydateAPI* pd
) {
    parse_obj_file(i_filepath, pd);
    // const int attribute_count = 3;
    // int stride = 9;
    // std::array<int, attribute_count> offsets{0, 3, 6};
    // std::shared_ptr<VertexData> vertex_data 
    //     = std::make_shared<SimpleVertexData<attribute_count>>(
    //         get_simple_vertex_buffer(), stride, offsets
    // );
    int stride = 6;
    std::shared_ptr<VertexData> vertex_data 
        = std::make_shared<IndexedVertexData>(
            get_ordered_vertex_data_buffer(), get_index_buffer(), 
            get_ordered_normal_buffer(), get_normal_index_buffer(), stride);
    //std::shared_ptr<SceneObject> scene_object = std::make_shared<SceneObject>(vertex_data);
    SceneObject scene_object{vertex_data};
    return scene_object;
}

void pd_getline(SDFile* file, std::string& line, PlaydateAPI* pd) {
    line.clear();
    char ch;
    while (pd->file->read(file, &ch, 1) == 1) {
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
}

void WFObjLoader::parse_obj_file(std::string i_filepath, PlaydateAPI* pd) {
    m_vertices.clear();
    m_vertex_normals.clear();
    m_faces.clear();

    SDFile* objFile;
    objFile = pd->file->open(i_filepath.c_str(), kFileRead);
    // if (objFile.is_open()) {
        std::cout << "Loading .obj file" << std::endl;
        while (true) {
            std::string line;
            pd_getline(objFile, line, pd);
            if (line.size() > 0) {
                // std::cout << line << std::endl;
                pd->system->logToConsole("OBJ Line: %s", line.c_str());
                parse_obj_file_line(line);
            } else {
                pd->system->logToConsole("End of OBJ file reached");
                break;
            }
        }
    // }
    // objFile.close();
    pd->file->close(objFile);
}

void WFObjLoader::parse_obj_file_line(std::string i_line) {
    std::vector<std::string> tokenized_line = tokenize(i_line, " ");
    std::string data_type = tokenized_line[0];
    if (data_type.compare("v") == 0) {
        add_vertex(tokenized_line);
    } else if (data_type.compare("vn") == 0) {
        add_vertex_normal(tokenized_line);
    } else if (data_type.compare("f") == 0) {
        add_face(tokenized_line);
    }
}

std::vector<std::string> tokenize(
    std::string stringToSplit,
    std::string delimeter
) {
    std::vector<std::string> result;

    std::string strippedString = stringToSplit;

    int newLinePos = stringToSplit.find("\n");
    if (newLinePos < stringToSplit.length()) {
        strippedString = stringToSplit.substr(0, newLinePos);
    }

    while (strippedString.length() > 0) {
        int delimPos = strippedString.find(delimeter);
        if (delimPos >= strippedString.length()) {
            delimPos = strippedString.length() - 1;
            result.push_back(strippedString);
            break;
        }
        std::string next_str = strippedString.substr(0, delimPos);
        if (next_str.compare("") != 0) {
            result.push_back(next_str);
        }
        strippedString = strippedString.substr(delimPos + 1, strippedString.length());
    }

    return result;
}

void WFObjLoader::add_vertex(std::vector<std::string> &i_tokenized_line) {
    glm::vec3 v = tokenized_line_to_vec3(i_tokenized_line);
    float w = 1.0f;
    if (i_tokenized_line.size() > 4) {
        w = std::stof(i_tokenized_line[4]);
    }
    m_vertices.emplace_back(v.x, v.y, v.z, w);
}

glm::vec3 WFObjLoader::tokenized_line_to_vec3(std::vector<std::string> &i_tokenized_line) {
    float x = std::stof(i_tokenized_line[1]);
    float y = std::stof(i_tokenized_line[2]);
    float z = std::stof(i_tokenized_line[3]);
    return glm::vec3{x, y, z};
}

void WFObjLoader::add_vertex_normal(std::vector<std::string> &i_tokenized_line) {
    glm::vec3 vn = tokenized_line_to_vec3(i_tokenized_line);
    m_vertex_normals.push_back(vn);
}

void WFObjLoader::add_face(std::vector<std::string> &i_tokenized_line) {
    m_faces.emplace_back(i_tokenized_line);
}

WFFace::WFFace(std::vector<std::string> &i_tokenized_line) {
    add_vertices(i_tokenized_line);
}

WFFace::~WFFace() {}

void WFFace::add_vertices(std::vector<std::string> &i_tokenized_line) {
    for (int i = 1; i < i_tokenized_line.size(); i++) {
        add_vertex(i_tokenized_line[i]);
    }
}

void WFFace::add_vertex(std::string &i_vert) {
    std::array<int, 3> vertex_to_add{-1, -1, -1};
    int num_of_indices = count_occurrences(i_vert, '/') + 1;
    std::vector<std::string> tokenized_vert = tokenize(i_vert, "/");
    if (num_of_indices >= 1) {
        vertex_to_add[0] = std::stoi(tokenized_vert[0]) - 1;
    }
    if (tokenized_vert.size() == 2 && num_of_indices == 3) {
        vertex_to_add[2] = std::stoi(tokenized_vert[1]) - 1;
    } else if (tokenized_vert.size() == 3 && num_of_indices == 3) {
        vertex_to_add[1] = std::stoi(tokenized_vert[1]) - 1;
        vertex_to_add[2] = std::stoi(tokenized_vert[2]) - 1;
    }
    m_vertices.push_back(vertex_to_add);
}

int count_occurrences(std::string stringToScan, char charToCount) {
    int count = 0;
    for (int i = 0; i < stringToScan.size(); i++) {
        if (stringToScan[i] == charToCount) {
            count++;
        }
    }
    return count;
}

std::vector<float> WFObjLoader::get_simple_vertex_buffer() {
    std::vector<float> result;
    for (auto face : m_faces) {
        auto face_as_simple_vb = face_to_simple_vertex_buffer(face);
        result = concat(result, face_as_simple_vb);
    }
    return result;
}

std::vector<float> WFObjLoader::get_ordered_vertex_data_buffer() {
    std::vector<float> result;
    // compute_and_store_normals();
    for (int i = 0; i < m_vertices.size(); i += 1) {
        glm::vec4 vert = m_vertices[i];
        glm::vec3 vert_norm = m_vertex_normals[i];
        result.push_back(vert.x);
        result.push_back(vert.y);
        result.push_back(vert.z);
        // result.push_back(vert_norm.x);
        // result.push_back(vert_norm.y);
        // result.push_back(vert_norm.z);
    }
    return result;
}

std::vector<int> WFObjLoader::get_index_buffer() {
    std::vector<int> result;
    for (auto face : m_faces) {
        auto face_as_idx_buffer = face_to_index_buffer(face);
        result = concat(result, face_as_idx_buffer);
    }
    return result;
}

std::vector<float> WFObjLoader::get_ordered_normal_buffer() {
    std::vector<float> result;
    // compute_and_store_normals();
    for (int i = 0; i < m_vertex_normals.size(); i += 1) {
        glm::vec3 vert_norm = m_vertex_normals[i];
        result.push_back(vert_norm.x);
        result.push_back(vert_norm.y);
        result.push_back(vert_norm.z);
    }
    return result;
}

std::vector<int> WFObjLoader::get_normal_index_buffer() {
    std::vector<int> result;
    for (auto face : m_faces) {
        auto face_as_idx_buffer = face_to_normal_index_buffer(face);
        result = concat(result, face_as_idx_buffer);
    }
    return result;
}

std::vector<int> WFObjLoader::face_to_index_buffer(WFFace &i_face) {
    std::vector<int> result;

    int vert_idx1 = i_face.get_vertex_idx(0);
    int vert_idx2 = i_face.get_vertex_idx(1);
    int vert_idx3 = i_face.get_vertex_idx(2);
    result.push_back(vert_idx1);
    result.push_back(vert_idx2);
    result.push_back(vert_idx3);
    return result;
}

std::vector<int> WFObjLoader::face_to_normal_index_buffer(WFFace &i_face) {
    std::vector<int> result;
    int vert_idx1 = i_face.get_vertex_normal(0);
    int vert_idx2 = i_face.get_vertex_normal(1);
    int vert_idx3 = i_face.get_vertex_normal(2);
    result.push_back(vert_idx1);
    result.push_back(vert_idx2);
    result.push_back(vert_idx3);
    return result;
}

void WFObjLoader::compute_and_store_normals() {
    m_vertex_normals.resize(m_vertices.size());
    for (auto face : m_faces) {
        compute_and_store_normals_for_face(face);
    }
}

void WFObjLoader::compute_and_store_normals_for_face(WFFace &i_face) {

    int vert_idx1 = i_face.get_vertex_idx(0);
    int vert_idx2 = i_face.get_vertex_idx(1);
    int vert_idx3 = i_face.get_vertex_idx(2);
    glm::vec4 vert1 = m_vertices.at(vert_idx1);
    glm::vec4 vert2 = m_vertices.at(vert_idx2);
    glm::vec4 vert3 = m_vertices.at(vert_idx3);

    float e1x = vert2.x - vert1.x;
    float e1y = vert2.y - vert1.y;
    float e1z = vert2.z - vert1.z;
    float e2x = vert3.x - vert1.x;
    float e2y = vert3.y - vert1.y;
    float e2z = vert3.z - vert1.z;

    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    for (int i = 0; i < i_face.vertex_count(); i++) {
        glm::vec3 vert_norm{nx, ny, nz};
        m_vertex_normals[vert_idx1] = vert_norm;
        m_vertex_normals[vert_idx2] = vert_norm;
        m_vertex_normals[vert_idx3] = vert_norm;
    }
}

std::vector<float> WFObjLoader::face_to_simple_vertex_buffer(WFFace &i_face) {
    std::vector<float> result;

    int vert_idx1 = i_face.get_vertex_idx(0);
    int vert_idx2 = i_face.get_vertex_idx(1);
    int vert_idx3 = i_face.get_vertex_idx(2);
    glm::vec4 vert1 = m_vertices.at(vert_idx1);
    glm::vec4 vert2 = m_vertices.at(vert_idx2);
    glm::vec4 vert3 = m_vertices.at(vert_idx3);

    float e1x = vert2.x - vert1.x;
    float e1y = vert2.y - vert1.y;
    float e1z = vert2.z - vert1.z;
    float e2x = vert3.x - vert1.x;
    float e2y = vert3.y - vert1.y;
    float e2z = vert3.z - vert1.z;

    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    for (int i = 0; i < i_face.vertex_count(); i++) {
        glm::vec4 vert = m_vertices[i_face.get_vertex_idx(i)];
        result.push_back(vert.x);
        result.push_back(vert.y);
        result.push_back(vert.z);
        // result.push_back(0.1f);
        // result.push_back(0.3f);
        // result.push_back(1.0f);
        if (i_face.vertex_has_normal(i)) {
            glm::vec3 vert_norm = m_vertex_normals[i_face.get_vertex_normal(i)];
            result.push_back(vert_norm.x);
            result.push_back(vert_norm.y);
            result.push_back(vert_norm.z);
        } else {
            result.push_back(nx);
            result.push_back(ny);
            result.push_back(nz);
        }
    }
    return result;
}

int WFFace::vertex_count() {
    return m_vertices.size();
}

int WFFace::get_vertex_idx(int idx) {
    return m_vertices[idx][0];
}

bool WFFace::vertex_has_normal(int idx) {
    return m_vertices[idx][2] != -1;
}

int WFFace::get_vertex_normal(int idx) {
    if (!vertex_has_normal(idx)) {
        std::cerr << "tried to access nonexistent vertex normal" << std::endl;
        exit(1);
    }
    return m_vertices[idx][2];
}

template <typename T>
std::vector<T> concat(std::vector<T> &a, std::vector<T> &b) {
    std::vector<T> result;
    result.reserve(a.size() + b.size());
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
}
