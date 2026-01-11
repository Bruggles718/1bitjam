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

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

SceneObject *scene_object;
SceneObject* scene_object2;
Camera *camera;
Vector* depth_buffer;
BayerMatrix *bayer_matrix;

LCDBitmap* frame_buffer;
uint8_t* fb_data;
int rowbytes;

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
		scene_object2 = (SceneObject*)malloc(sizeof(SceneObject));
		*scene_object2 = wfobjloader_create_scene_object_from_file(&wf_obj_loader, "cube.obj", pd);
		camera = (Camera*)malloc(sizeof(Camera));
		camera_init(camera);
		vec3 pos = {-2.0, 0.0, 0.0};
		scene_object_set_position(scene_object, pos);
		vec3 pos2 = { 2.0f, 0.0f, 0.0f };
		scene_object_set_position(scene_object2, pos2);
		depth_buffer = (Vector*)malloc(sizeof(Vector));
		vector_setup(depth_buffer, SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(float));
		initialize_depth_buffer();

		bayer_matrix = (BayerMatrix*)malloc(sizeof(BayerMatrix));
		*bayer_matrix = bayer_matrix_create(512);


		frame_buffer = pd->graphics->newBitmap(SCREEN_WIDTH, SCREEN_HEIGHT, kColorWhite);
		int width, height;
		pd->graphics->getBitmapData(frame_buffer, &width, &height, &rowbytes, NULL, &fb_data);

		pd->system->setPeripheralsEnabled(kAccelerometer);



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

float smoothed_ax = 0;
float smoothed_ay = 0; 
float smoothed_az = 0;

void get_orientation_from_input(PlaydateAPI* pd, versor out_q)
{
	float ax, ay, az;
	pd->system->getAccelerometer(&ax, &ay, &az);

	// TODO: lowpass filter accelerometer for smoothing
	/// LOWPASS FILTER FOR ACCELEROMETER SMOOTHING
	const float coef = 0.1f; //smoothing coefficient

	smoothed_ax = smoothed_ax + coef * (ax - smoothed_ax); 
	smoothed_ay = smoothed_ay + coef * (ay - smoothed_ay);
	smoothed_az = smoothed_az + coef * (az - smoothed_az);

	// Compute our pitch & roll from accelerometer
	float acc_pitch = 4 * atan2f(ay, az);
	float acc_roll = 0.25 * atan2f(-ax, sqrtf(ay * ay + az * az)); 

	//values where our new pitch and roll will go
	float pitch, roll;

	//TODO: Make behavior good
	pitch = acc_pitch;
	roll = acc_roll;


	// Crank → yaw
	float crank = pd->system->getCrankAngle();
	float yaw = glm_rad(crank);

	pd->system->logToConsole("PITCH:%d\nROLL:%d\nYAW%d\n", pitch, roll, yaw);

	// Build quaternions
	versor q_pitch, q_roll, q_yaw, temp, q_final;

	glm_quatv(q_pitch, pitch, (vec3) { 1, 0, 0 });
	glm_quatv(q_roll, roll, (vec3) { 0, 0, 1 });
	glm_quatv(q_yaw, yaw, (vec3) { 0, 1, 0 });

	// Combine yaw * pitch * roll
	glm_quat_mul(q_yaw, q_pitch, temp);
	glm_quat_mul(temp, q_roll, q_final);

	glm_quat_normalize_to(q_final, out_q);
}


static int update(void* userdata)
{
	double cpu_time_used;
	reset_depth_buffer();
	PlaydateAPI* pd = userdata;
	// pd->system->logToConsole("db val: %f", VECTOR_GET_AS(float, depth_buffer, 0));
	
	//pd->graphics->clear(kColorWhite);
	pd->graphics->clearBitmap(frame_buffer, kColorWhite);

	vec3 y_axis = {0.0f, 1.0f, 0.0f};
	vec3 x_axis = {1.0f, 0.0f, 0.0f};
	scene_object_rotate(scene_object, 1.0f, y_axis);
	scene_object_rotate(scene_object, 1.0f, x_axis);
	scene_object_rotate(scene_object2, 1.0f, y_axis);
	scene_object_rotate(scene_object2, 1.0f, x_axis);
	versor q;
	get_orientation_from_input(pd, q); 

	scene_object_set_rotation(scene_object, q);

	scene_object_draw(scene_object, camera, pd, depth_buffer, bayer_matrix);
	scene_object_draw(scene_object2, camera, pd, depth_buffer, bayer_matrix);
	//pd->graphics->drawScaledBitmap(frame_buffer, 0, 0, PIXEL_SCALE, PIXEL_SCALE);
	pd->graphics->drawBitmap(frame_buffer, 0, 0, 0);
	pd->graphics->drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, kColorBlack);
	pd->system->drawFPS(0,0);

	return 1;
}




