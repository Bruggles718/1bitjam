#ifndef MODEL_FILE_LOADER_H
#define MODEL_FILE_LOADER_H

#include "SceneObject.h"
#include "pd_api.h"

/* Forward declaration */
typedef struct SceneObject SceneObject;

/* Base "interface" for model file loaders */
typedef struct ModelFileLoader ModelFileLoader;

/* Virtual function pointer type */
typedef SceneObject (*create_scene_object_from_file_fn)(
    ModelFileLoader* loader,
    const char* filepath,
    PlaydateAPI* pd
);

struct ModelFileLoader {
    /* Virtual function pointer */
    create_scene_object_from_file_fn create_scene_object_from_file;
    
    /* Derived classes will add their own data members here */
};

/* Base initializer (for use by derived "classes") */
void model_file_loader_init(ModelFileLoader* loader);

/* Base destructor */
void model_file_loader_destroy(ModelFileLoader* loader);

#endif /* MODEL_FILE_LOADER_H */
