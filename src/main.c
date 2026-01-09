//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pd_api.h"
#include "SceneObject.h"
#include "Camera.h"
#include "WFObjLoader.h"

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

SceneObject *scene_object;
Camera *camera;
Vector* depth_buffer;

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);
		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

		WFObjLoader wf_obj_loader;
		wfobjloader_init(&wf_obj_loader);

		scene_object = (SceneObject*)malloc(sizeof(SceneObject));
		*scene_object = wfobjloader_create_scene_object_from_file(&wf_obj_loader, "icosahedron.obj", pd);
		camera = (Camera*)malloc(sizeof(Camera));
		vec3 pos = {0.0, 0.0, 0.0};
		scene_object_set_position(scene_object, pos);
		vector_setup(depth_buffer, SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(float));
		// Note: If you set an update callback in the kEventInit handler, the system assumes the game is pure C and doesn't run any Lua code in the game
		pd->system->setUpdateCallback(update, pd);
	}
	
	return 0;
}


#define TEXT_WIDTH 86
#define TEXT_HEIGHT 16

int x = (400-TEXT_WIDTH)/2;
int y = (240-TEXT_HEIGHT)/2;
int dx = 1;
int dy = 2;


int vector_fill(Vector* vector, void* value)
{
    if (!vector || !vector->data)
        return VECTOR_ERROR;

    for (size_t i = 0; i < vector->size; ++i)
    {
        void* dest = (char*)vector->data + i * vector->element_size;
        memcpy(dest, value, vector->element_size);
    }

    return VECTOR_SUCCESS;
}

static int update(void* userdata)
{
	float negative_infinity = -INFINITY;

	vector_fill(depth_buffer, &negative_infinity);

	PlaydateAPI* pd = userdata;
	
	pd->graphics->clear(kColorWhite);

	vec3 y_axis = {0.0f, 1.0f, 0.0f};
	vec3 x_axis = {1.0f, 0.0f, 0.0f};
	scene_object_rotate(scene_object, 1.0f, y_axis);
	scene_object_rotate(scene_object, 1.0f, x_axis);
	scene_object_draw(scene_object, camera, pd, depth_buffer);

        
	pd->system->drawFPS(0,0);

	return 1;
}

