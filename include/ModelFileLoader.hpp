#pragma once

#include <string>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <memory>
#include "SceneObject.hpp"

class ModelFileLoader {
    public:
        virtual SceneObject create_scene_object_from_file(
            std::string i_filepath, PlaydateAPI* pd
        ) = 0;
};