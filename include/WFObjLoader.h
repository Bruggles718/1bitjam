#ifndef WFOBJLOADER_H
#define WFOBJLOADER_H

#include "ModelFileLoader.h"
#include "vector.h"
#include "pd_api.h"
#include <cglm/cglm.h>

/* ===== WFFace Struct ===== */

typedef struct WFFace {
    Vector m_vertices; /* Vector of int arrays [vertex_idx, texture_idx, normal_idx] */
                       /* Each element is a 3-element int array */
} WFFace;

/* Constructor */
void wfface_init(WFFace* face, Vector* tokenized_line);

/* Destructor */
void wfface_destroy(WFFace* face);

/* Public methods */
int wfface_vertex_count(const WFFace* face);
int wfface_get_vertex_idx(const WFFace* face, int face_idx);
bool wfface_vertex_has_texture(const WFFace* face, int idx);
bool wfface_vertex_has_normal(const WFFace* face, int idx);
int wfface_get_vertex_texture(const WFFace* face, int idx);
int wfface_get_vertex_normal(const WFFace* face, int idx);

/* Private helper methods */
void wfface_add_vertices(WFFace* face, Vector* tokenized_line);
void wfface_add_vertex(WFFace* face, const char* vert_str);


/* ===== WFObjLoader "Class" ===== */

typedef struct WFObjLoader {
    ModelFileLoader base; /* Inheritance - must be first member */
    
    Vector m_vertices;        /* Vector of vec4 */
    Vector m_vertex_normals;  /* Vector of vec3 */
    Vector m_faces;           /* Vector of WFFace */
} WFObjLoader;

/* Constructor */
void wfobjloader_init(WFObjLoader* loader);

/* Destructor */
void wfobjloader_destroy(WFObjLoader* loader);

/* Implementation of virtual function */
SceneObject wfobjloader_create_scene_object_from_file(
    WFObjLoader* loader,
    const char* filepath,
    PlaydateAPI* pd
);

/* Private helper methods */
void wfobjloader_get_simple_vertex_buffer(WFObjLoader* loader, Vector* out_buffer);
void wfobjloader_parse_obj_file(WFObjLoader* loader, const char* filepath, PlaydateAPI* pd);
void wfobjloader_parse_obj_file_line(WFObjLoader* loader, const char* line);
void wfobjloader_add_vertex(WFObjLoader* loader, Vector* tokenized_line);
void wfobjloader_add_vertex_normal(WFObjLoader* loader, Vector* tokenized_line);
void wfobjloader_tokenized_line_to_vec3(Vector* tokenized_line, vec3 out);
void wfobjloader_add_face(WFObjLoader* loader, Vector* tokenized_line);
void wfobjloader_face_to_simple_vertex_buffer(WFObjLoader* loader, WFFace* face, Vector* out_buffer);


/* ===== Utility Functions ===== */

/* Tokenize a string by delimiter, returns Vector of char* (strings) */
void tokenize(const char* string_to_split, const char* delimiter, Vector* out_tokens);

/* Count occurrences of a character in a string */
int count_occurrences(const char* string_to_scan, char char_to_count);

/* Concatenate two vectors, result stored in out_result */
void vector_concat_floats(Vector* a, Vector* b, Vector* out_result);

#endif /* WFOBJLOADER_H */
