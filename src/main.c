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
#include <time.h>
#include <windows.h>

#include "pd_api.h"
#include "SceneObject.h"
#include "Camera.h"
#include "WFObjLoader.h"

// make ARM linker shut up about things we aren't using (nosys lib issues):
void _close(void)
{
}
void _lseek(void)
{

}
void _read(void)
{
}
void _write(void)
{
}
void _fstat(void)
{
}
void _getpid(void)
{
}
void _isatty(void)
{
}
void _kill(void)
{
}
void abort(void) {}
void _exit(void) {}
void _fini(void) {}
// end ARM linker warning hack

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

SceneObject *scene_object;
Camera *camera;
Vector* depth_buffer;
BayerMatrix *bayer_matrix;

void initialize_depth_buffer() {
	int size = SCREEN_WIDTH * SCREEN_HEIGHT;
	for (int i = 0; i < size; i += 1) {
		float ni = -INFINITY;
		vector_push_back(depth_buffer, &ni);
	}
}

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
		camera_init(camera);
		vec3 pos = {0.0, 0.0, 0.0};
		scene_object_set_position(scene_object, pos);
		depth_buffer = (Vector*)malloc(sizeof(Vector));
		vector_setup(depth_buffer, SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(float));
		initialize_depth_buffer();

		bayer_matrix = (BayerMatrix*)malloc(sizeof(BayerMatrix));
		*bayer_matrix = bayer_matrix_create(512);

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


void reset_depth_buffer()
{
	float* data = (float*)depth_buffer->data;
    for (int i = 0; i < depth_buffer->size; i += 1)
    {
        data[i] = -INFINITY;
    }
}

double total = 0;
double count = 0;

static int update(void* userdata)
{
	double cpu_time_used;
	LARGE_INTEGER frequency, start, end;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);
	reset_depth_buffer();
	PlaydateAPI* pd = userdata;
	// pd->system->logToConsole("db val: %f", VECTOR_GET_AS(float, depth_buffer, 0));
	
	pd->graphics->clear(kColorWhite);

	vec3 y_axis = {0.0f, 1.0f, 0.0f};
	vec3 x_axis = {1.0f, 0.0f, 0.0f};
	scene_object_rotate(scene_object, 1.0f, y_axis);
	scene_object_rotate(scene_object, 1.0f, x_axis);
	scene_object_draw(scene_object, camera, pd, depth_buffer, bayer_matrix);
	pd->system->drawFPS(0,0);
	QueryPerformanceCounter(&end);
	double elapsed_time_us = (double)(end.QuadPart - start.QuadPart) * 1000000.0 / frequency.QuadPart;
	total += elapsed_time_us;
	count += 1;
	double real_elapsed = total / count;
	pd->system->logToConsole("Total CPU time used: %f microseconds\n", real_elapsed);

	return 1;
}
