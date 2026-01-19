#include "WFObjLoader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <assert.h>

/* ===== Utility Functions ===== */

void tokenize(const char* string_to_split, const char* delimiter, Vector* out_tokens) {
    if (!string_to_split || !delimiter || !out_tokens) return;
    
    /* Make a copy of the string to work with */
    char* str_copy = strdup(string_to_split);
    if (!str_copy) return;
    
    /* Strip newline if present */
    char* newline_pos = strchr(str_copy, '\n');
    if (newline_pos) {
        *newline_pos = '\0';
    }
    
    char* token = strtok(str_copy, delimiter);
    while (token != NULL) {
        if (strlen(token) > 0) {
            /* Allocate and copy token */
            char* token_copy = strdup(token);
            vector_push_back(out_tokens, &token_copy);
        }
        token = strtok(NULL, delimiter);
    }
    
    free(str_copy);
}

int count_occurrences(const char* string_to_scan, char char_to_count) {
    if (!string_to_scan) return 0;
    
    int count = 0;
    for (size_t i = 0; i < strlen(string_to_scan); i++) {
        if (string_to_scan[i] == char_to_count) {
            count++;
        }
    }
    return count;
}

void vector_concat_floats(Vector* a, Vector* b, Vector* out_result) {
    if (!a || !b || !out_result) return;
    
    /* Reserve space */
    vector_reserve(out_result, a->size + b->size);
    
    /* Copy from a */
    for (size_t i = 0; i < a->size; i++) {
        float val = VECTOR_GET_AS(float, a, i);
        vector_push_back(out_result, &val);
    }
    
    /* Copy from b */
    for (size_t i = 0; i < b->size; i++) {
        float val = VECTOR_GET_AS(float, b, i);
        vector_push_back(out_result, &val);
    }
}

/* ===== WFFace Implementation ===== */

void wfface_init(WFFace* face, Vector* tokenized_line) {
    if (!face || !tokenized_line) return;
    
    /* Initialize vector to hold vertex indices (each is an int[3]) */
    vector_setup(&face->m_vertices, 3, sizeof(ivec3s) * 3);
    
    wfface_add_vertices(face, tokenized_line);
}

void wfface_destroy(WFFace* face) {
    if (!face) return;
    vector_destroy(&face->m_vertices);
}

void wfface_add_vertices(WFFace* face, Vector* tokenized_line) {
    if (!face || !tokenized_line) return;
    
    /* Skip first token (it's "f") */
    for (size_t i = 1; i < tokenized_line->size; i++) {
        char** token_ptr = (char**)vector_get(tokenized_line, i);
        wfface_add_vertex(face, *token_ptr);
    }
}

void wfface_add_vertex(WFFace* face, const char* vert_str) {
    if (!face || !vert_str) return;
    
    ivec3s vertex_to_add = {-1, -1, -1};
    int num_of_indices = count_occurrences(vert_str, '/') + 1;
    
    Vector tokenized_vert;
    vector_setup(&tokenized_vert, 3, sizeof(char*));
    tokenize(vert_str, "/", &tokenized_vert);
    
    if (num_of_indices >= 1 && tokenized_vert.size >= 1) {
        char** token = (char**)vector_get(&tokenized_vert, 0);
        vertex_to_add.x = atoi(*token) - 1;
    }
    
    if (tokenized_vert.size == 2 && num_of_indices == 3) {
        char** token = (char**)vector_get(&tokenized_vert, 1);
        vertex_to_add.z = atoi(*token) - 1;
    } else if (tokenized_vert.size == 3 && num_of_indices == 3) {
        char** token1 = (char**)vector_get(&tokenized_vert, 1);
        char** token2 = (char**)vector_get(&tokenized_vert, 2);
        vertex_to_add.y = atoi(*token1) - 1;
        vertex_to_add.z = atoi(*token2) - 1;
    }
    
    vector_push_back(&face->m_vertices, &vertex_to_add);
    
    /* Free tokenized strings */
    for (size_t i = 0; i < tokenized_vert.size; i++) {
        char** token = (char**)vector_get(&tokenized_vert, i);
        free(*token);
    }
    vector_destroy(&tokenized_vert);
}

int wfface_vertex_count(const WFFace* face) {
    if (!face) return 0;
    return face->m_vertices.size;
}

int wfface_get_vertex_idx(const WFFace* face, int face_idx) {
    if (!face) return -1;
    ivec3s vertex = *(ivec3s*)vector_const_get(&face->m_vertices, face_idx);
    return vertex.x;
}

bool wfface_vertex_has_texture(const WFFace* face, int idx) {
    if (!face) return false;
    int* vertex = (int*)vector_const_get(&face->m_vertices, idx);
    return vertex[1] != -1;
}

bool wfface_vertex_has_normal(const WFFace* face, int idx) {
    if (!face) return false;
    ivec3s vertex = *(ivec3s*)vector_const_get(&face->m_vertices, idx);
    return vertex.z != -1;
}

int wfface_get_vertex_texture(const WFFace* face, int idx) {
    if (!face) return -1;
    ivec3s vertex = *(ivec3s*)vector_const_get(&face->m_vertices, idx);
    return vertex.y;
}

int wfface_get_vertex_normal(const WFFace* face, int idx) {
    if (!face) return -1;
    if (!wfface_vertex_has_normal(face, idx)) {
        fprintf(stderr, "tried to access nonexistent vertex normal\n");
        exit(1);
    }
    ivec3s vertex = *(ivec3s*)vector_const_get(&face->m_vertices, idx);
    return vertex.z;
}

/* ===== Helper for reading files ===== */

static void pd_getline(SDFile* file, char* line, size_t max_len, PlaydateAPI* pd) {
    size_t idx = 0;
    char ch;
    while (pd->file->read(file, &ch, 1) == 1 && idx < max_len - 1) {
        if (ch == '\n') {
            break;
        }
        line[idx++] = ch;
    }
    line[idx] = '\0';
}

/* ===== WFObjLoader Implementation ===== */

void wfobjloader_init(WFObjLoader* loader) {
    
    /* Initialize vectors */
    vector_setup(&loader->m_vertices, 10, sizeof(vec4));
    vector_setup(&loader->m_vertex_normals, 10, sizeof(vec3));
    vector_setup(&loader->m_faces, 10, sizeof(WFFace));
}

void wfobjloader_destroy(WFObjLoader* loader) {
    if (!loader) return;
    
    /* Destroy all faces */
    for (size_t i = 0; i < loader->m_faces.size; i++) {
        WFFace* face = (WFFace*)vector_get(&loader->m_faces, i);
        wfface_destroy(face);
    }
    
    vector_destroy(&loader->m_vertices);
    vector_destroy(&loader->m_vertex_normals);
    vector_destroy(&loader->m_faces);
    
    model_file_loader_destroy(&loader->base);
}

SceneObject wfobjloader_create_scene_object_from_file(
    WFObjLoader* obj_loader,
    const char* filepath,
    PlaydateAPI* pd
) {    
    wfobjloader_parse_obj_file(obj_loader, filepath, pd);
    
    Vector vertex_buffer;
    vector_setup(&vertex_buffer, 100, sizeof(float));
    assert(vector_is_initialized(&vertex_buffer));
    wfobjloader_get_simple_vertex_buffer(obj_loader, &vertex_buffer);
    
    /* Create SimpleVertexData */
    const int attribute_count = 3;
    int stride = 6;
    int* offsets = (int*)malloc(attribute_count * sizeof(int));
    offsets[0] = 0;
    offsets[1] = 3;
    offsets[2] = 6;
    
    SimpleVertexData* vertex_data = (SimpleVertexData*)malloc(sizeof(SimpleVertexData));
    simple_vertex_data_init(vertex_data, &vertex_buffer, stride, offsets, attribute_count);
    
    /* Create SceneObject */
    SceneObject scene_object;
    scene_object_init(&scene_object, (SimpleVertexData*)vertex_data);
    
    vector_destroy(&vertex_buffer);
    
    return scene_object;
}

void wfobjloader_parse_obj_file(WFObjLoader* loader, const char* filepath, PlaydateAPI* pd) {
    if (!loader || !filepath || !pd) return;
    
    /* Clear previous data */
    vector_clear(&loader->m_vertices);
    vector_clear(&loader->m_vertex_normals);
    vector_clear(&loader->m_faces);
    
    SDFile* obj_file = pd->file->open(filepath, kFileRead);
    if (!obj_file) {
        pd->system->logToConsole("Failed to open OBJ file: %s", filepath);
        return;
    }
    
    printf("Loading .obj file\n");
    
    char line[1024];
    while (true) {
        pd_getline(obj_file, line, sizeof(line), pd);
        if (strlen(line) > 0) {
            //pd->system->logToConsole("OBJ Line: %s", line);
            wfobjloader_parse_obj_file_line(loader, line);
        } else {
            pd->system->logToConsole("End of OBJ file reached");
            break;
        }
    }
    
    pd->file->close(obj_file);
}

void wfobjloader_parse_obj_file_line(WFObjLoader* loader, const char* line) {
    if (!loader || !line) return;
    
    Vector tokenized_line;
    vector_setup(&tokenized_line, 4, sizeof(char*));
    tokenize(line, " ", &tokenized_line);
    
    if (tokenized_line.size == 0) {
        vector_destroy(&tokenized_line);
        return;
    }
    
    char** data_type_ptr = (char**)vector_get(&tokenized_line, 0);
    const char* data_type = *data_type_ptr;
    
    if (strcmp(data_type, "v") == 0) {
        wfobjloader_add_vertex(loader, &tokenized_line);
    } else if (strcmp(data_type, "vn") == 0) {
        wfobjloader_add_vertex_normal(loader, &tokenized_line);
    } else if (strcmp(data_type, "f") == 0) {
        wfobjloader_add_face(loader, &tokenized_line);
    }
    
    /* Free tokenized strings */
    for (size_t i = 0; i < tokenized_line.size; i++) {
        char** token = (char**)vector_get(&tokenized_line, i);
        free(*token);
    }
    vector_destroy(&tokenized_line);
}

void wfobjloader_add_vertex(WFObjLoader* loader, Vector* tokenized_line) {
    if (!loader || !tokenized_line) return;
    
    vec3 v = {0.0f, 0.0f, 0.0f};
    wfobjloader_tokenized_line_to_vec3(tokenized_line, &v);
    float w = 1.0f;
    
    if (tokenized_line->size > 4) {
        char** token = (char**)vector_get(tokenized_line, 4);
        w = atof(*token);
    }
    
    vec4 vertex = {v[0], v[1], v[2], w};
    vector_push_back(&loader->m_vertices, &vertex);
}

void wfobjloader_tokenized_line_to_vec3(Vector* tokenized_line, vec3 out) {

    if (!tokenized_line || tokenized_line->size < 4) return;
    
    char** token_x = (char**)vector_get(tokenized_line, 1);
    char** token_y = (char**)vector_get(tokenized_line, 2);
    char** token_z = (char**)vector_get(tokenized_line, 3);
    
    out[0] = atof(*token_x);
    out[1] = atof(*token_y);
    out[2] = atof(*token_z);
}

void wfobjloader_add_vertex_normal(WFObjLoader* loader, Vector* tokenized_line) {
    if (!loader || !tokenized_line) return;
    
    vec3 vn = {0.0f, 0.0f, 0.0f};
    wfobjloader_tokenized_line_to_vec3(tokenized_line, &vn);
    vector_push_back(&loader->m_vertex_normals, &vn);
}

void wfobjloader_add_face(WFObjLoader* loader, Vector* tokenized_line) {
    if (!loader || !tokenized_line) return;
    
    WFFace face;
    wfface_init(&face, tokenized_line);
    vector_push_back(&loader->m_faces, &face);
}

void wfobjloader_get_simple_vertex_buffer(WFObjLoader* loader, Vector* out_buffer) {
    if (!loader || !out_buffer) return;
    
    for (size_t i = 0; i < loader->m_faces.size; i++) {
        WFFace* face = (WFFace*)vector_get(&loader->m_faces, i);
        
        Vector face_buffer;
        vector_setup(&face_buffer, 50, sizeof(float));
        wfobjloader_face_to_simple_vertex_buffer(loader, face, &face_buffer);
        
        /* Append to output buffer */
        for (size_t j = 0; j < face_buffer.size; j++) {
            float* val = (float*)vector_get(&face_buffer, j);
            vector_push_back(out_buffer, val);
        }
        
        vector_destroy(&face_buffer);
    }
}

void wfobjloader_face_to_simple_vertex_buffer(WFObjLoader* loader, WFFace* face, Vector* out_buffer) {
    if (!loader || !face || !out_buffer) return;

    int vert_idx1 = wfface_get_vertex_idx(face, 0);
    int vert_idx2 = wfface_get_vertex_idx(face, 1);
    int vert_idx3 = wfface_get_vertex_idx(face, 2);
    vec4* vert1 = (vec4*)vector_get(&loader->m_vertices, vert_idx1);
    vec4* vert2 = (vec4*)vector_get(&loader->m_vertices, vert_idx2);
    vec4* vert3 = (vec4*)vector_get(&loader->m_vertices, vert_idx3);

    float e1x = (*vert2)[0] - (*vert1)[0];
    float e1y = (*vert2)[1] - (*vert1)[1];
    float e1z = (*vert2)[2] - (*vert1)[2];
    float e2x = (*vert3)[0] - (*vert1)[0];
    float e2y = (*vert3)[1] - (*vert1)[1];
    float e2z = (*vert3)[2] - (*vert1)[2];

    float nx = e1y * e2z - e1z * e2y;
    float ny = e1z * e2x - e1x * e2z;
    float nz = e1x * e2y - e1y * e2x;

    
    for (int i = 0; i < wfface_vertex_count(face); i++) {
        int vert_idx = wfface_get_vertex_idx(face, i);
        vec4* vert = (vec4*)vector_get(&loader->m_vertices, vert_idx);
        
        vector_push_back(out_buffer, &(*vert)[0]);
        vector_push_back(out_buffer, &(*vert)[1]);
        vector_push_back(out_buffer, &(*vert)[2]);
        if (wfface_vertex_has_normal(face, i)) {
            int n_idx = wfface_get_vertex_normal(face, i);
            vec3* v_normal = (vec3*)vector_get(&loader->m_vertex_normals, n_idx);
            vector_push_back(out_buffer, &(*v_normal)[0]);
            vector_push_back(out_buffer, &(*v_normal)[1]);
            vector_push_back(out_buffer, &(*v_normal)[2]);
        }
        else {
            vector_push_back(out_buffer, &nx);
            vector_push_back(out_buffer, &ny);
            vector_push_back(out_buffer, &nz);
        }
    }
}

/* ===== ModelFileLoader Base Implementation ===== */

void model_file_loader_init(ModelFileLoader* loader) {
    if (!loader) return;
    loader->create_scene_object_from_file = NULL;
}

void model_file_loader_destroy(ModelFileLoader* loader) {
    /* Nothing to clean up in base class */
    (void)loader;
}
