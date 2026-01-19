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
#include "ScreenGlobals.h"
#include "SBufferRenderer.h"

#if TARGET_PLAYDATE
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

#endif

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

SceneObject* submarineObj;
SceneObject* mapObj;
Camera *camera;
Vector* depth_buffer;
BayerMatrix *bayer_matrix;

LCDBitmap* frame_buffer;
uint8_t* fb_data;
int rowbytes;

Vector sbuffer[LCD_ROWS];

void initialize_depth_buffer() {
	int size = SCREEN_WIDTH * SCREEN_HEIGHT;
	for (int i = 0; i < size; i += 1) {
		float ni = -INFINITY;
		vector_push_back(depth_buffer, &ni);
	}
}

void initialize_sbuffer() {
	for (int i = 0; i < LCD_ROWS; i += 1) {
		vector_setup(&(sbuffer[i]), 10, sizeof(span_t));
	}
}

void clear_sbuffer() {
	for (int i = 0; i < LCD_ROWS; i += 1) {
		sbuffer[i].size = 0;
	}
}

#ifdef _WINDLL
__declspec(dllexport)
#endif

int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	//Runs at the start of the program
	if ( event == kEventInit )
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);
		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

		WFObjLoader wf_obj_loader;
		wfobjloader_init(&wf_obj_loader);

		submarineObj = (SceneObject*)malloc(sizeof(SceneObject));
		*submarineObj = wfobjloader_create_scene_object_from_file(&wf_obj_loader, "submarine.obj", pd);
		mapObj = (SceneObject*)malloc(sizeof(SceneObject));
		*mapObj = wfobjloader_create_scene_object_from_file(&wf_obj_loader, "map.obj", pd);
		camera = (Camera*)malloc(sizeof(Camera));
		camera_init(camera);


		vec3 pos = {0.0, 0.0, 0.0};
		scene_object_set_position(submarineObj, pos);

		vec3 pos2 = { 0.0f, 0.0f, 0.0f };
		scene_object_set_position(mapObj, pos2);

		depth_buffer = (Vector*)malloc(sizeof(Vector));
		vector_setup(depth_buffer, SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(float));
		initialize_depth_buffer();
		initialize_sbuffer();

		bayer_matrix = (BayerMatrix*)malloc(sizeof(BayerMatrix));
		*bayer_matrix = bayer_matrix_create(512);


		frame_buffer = pd->graphics->newBitmap(SCREEN_WIDTH, SCREEN_HEIGHT, kColorWhite);
		int width, height;
		pd->graphics->getBitmapData(frame_buffer, &width, &height, &rowbytes, NULL, &fb_data);

		pd->system->setPeripheralsEnabled(kAccelerometer);

		pd->display->setScale(PIXEL_SCALE);

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


static float smoothed_pitch = 0.0f;
static float smoothed_roll = 0.0f;

void calculate_input_versor(PlaydateAPI* pd, versor out_q, float pitch_scale, float roll_scale)
{
	float ax, ay, az;
	pd->system->getAccelerometer(&ax, &ay, &az);

	// Compute our pitch & roll from accelerometer
	float acc_pitch = atan2f(ay, az);
	float acc_roll = atan2f(-ax, sqrtf(ay * ay + az * az));


	/// LOWPASS FILTER FOR ACCELEROMETER SMOOTHING
	const float angle_coef = 0.12f; //smoothing coefficient

	smoothed_pitch = smoothed_pitch + angle_coef * (acc_pitch - smoothed_pitch); 
	smoothed_roll = smoothed_roll + angle_coef * (acc_roll - smoothed_roll);

	//values where our new pitch and roll will go
	float pitch = smoothed_pitch * pitch_scale;
	float roll = smoothed_roll * roll_scale;

	/* doesnt work w new calibration system
	float max_pitch = glm_rad(80.0f);   // or whatever feels good
	float max_roll = glm_rad(80.0f);

	if (pitch > max_pitch) pitch = max_pitch;
	if (pitch < -max_pitch) pitch = -max_pitch;

	if (roll > max_roll) roll = max_roll;
	if (roll < -max_roll) roll = -max_roll;
	*/

	// Build quaternions
	versor q_pitch, q_roll, q_tilt;

	glm_quatv(q_pitch, pitch, (vec3) { 1, 0, 0 });
	glm_quatv(q_roll, roll, (vec3) { 0, 0, 1 });
	glm_quat_mul(q_pitch, q_roll, q_tilt);

	// -- ACCELEROMETER USER CALIBRATION (storing a user set offset)
	static versor q_calibration = GLM_QUAT_IDENTITY_INIT;
	static bool is_calibrated = false;

	PDButtons current;
	pd->system->getButtonState(&current, NULL, NULL);

	if (current & kButtonDown) { //if the player hits down, the accelerometer will store its current value in q_calibration and use it as an offset
		glm_quat_copy(q_tilt, q_calibration);
		glm_quat_normalize(q_calibration);
		is_calibrated = true;
	}

	versor corrected_tilt;
	if (is_calibrated) {
		versor inverse;
		glm_quat_inv(q_calibration, inverse);
		glm_quat_mul(inverse, q_tilt, corrected_tilt);
	}
	else {
		glm_quat_copy(q_tilt, corrected_tilt);
	}

	versor q_yaw;
	glm_quatv(q_yaw, glm_rad(pd->system->getCrankAngle()), (vec3) { 0, 1, 0 });

	versor q_final;
	glm_quat_mul(q_yaw, corrected_tilt, q_final);

	glm_quat_normalize_to(q_final, out_q);

}

static vec3 sub_velocity = { 0,0,0 };
void control_object(PlaydateAPI* pd, SceneObject* sub) {
	//accelerometer and crank
	versor tilt;
	calculate_input_versor(pd, tilt, 1.5f, 1);
	scene_object_set_rotation(sub, tilt);

	//calculate forward vector from tilt
	vec3 forward = { 0, 0, -1 };
	vec3 right = { -1, 0, 0 };
	mat4 rot;
	glm_quat_mat4(tilt, rot);
	glm_mat4_mulv3(rot, forward, 1.0f, forward);
	glm_mat4_mulv3(rot, right, 1.0f, right);
	glm_vec3_normalize(forward);
	glm_vec3_normalize(right);

	//banking (roll effecting our forward vector)
	float bank_strength = 4; 

	float bank = smoothed_roll * bank_strength;
	vec3 side;
	glm_vec3_scale(right, bank, side);
	glm_vec3_add(forward, side, forward);
	glm_normalize(forward);

	

	PDButtons current; pd->system->getButtonState(&current, NULL, NULL);

	//give her gas
	if (current & kButtonUp) {
		float thrust = 0.1f;
		vec3 acceleration;
		glm_vec3_scale(forward, thrust, acceleration);
		glm_vec3_add(sub_velocity, acceleration, sub_velocity);
	}

	//drag
	float drag = 0.92f;
	glm_vec3_scale(sub_velocity, drag, sub_velocity);



	vec3 pos;
	glm_vec3_copy(sub->m_transform.m_position, pos);
	glm_vec3_add(pos, sub_velocity, pos);
	scene_object_set_position(sub, pos);



	// -- CAMERA STUFF --

	float camera_distance = 6.0f;

	vec3 backward = { 0, 0.3f, 1 };

	glm_quat_mat4(tilt, rot);
	glm_mat4_mulv3(rot, backward, 1.0f, backward);
	glm_vec3_normalize(backward);

	vec3 cam_pos;
	glm_vec3_copy(sub->m_transform.m_position, cam_pos);

	vec3 cam_offset;
	glm_vec3_scale(backward, camera_distance, cam_offset);

	glm_vec3_add(cam_pos, cam_offset, cam_pos);

	static versor cam_rot = GLM_QUAT_IDENTITY_INIT;
	float rot_lerp = 0.15f;

	versor blended;
	glm_quat_slerp(cam_rot, tilt, rot_lerp, blended);
	glm_quat_normalize_to(blended, cam_rot);

	camera_set_rotation(camera, cam_rot);
	camera_set_eye_position(camera, cam_pos[0], cam_pos[1], cam_pos[2]);


}

void draw_sbuffer(PlaydateAPI *pd) {
	for (int i = 0; i < LCD_ROWS; i += 1) {
		draw_scanline(&(sbuffer[i]), i, pd, bayer_matrix);
	}
}



static int update(void* userdata)
{
	reset_depth_buffer();
	//clear_sbuffer();
	PlaydateAPI* pd = userdata;
	// pd->system->logToConsole("db val: %f", VECTOR_GET_AS(float, depth_buffer, 0));

	//pd->graphics->clear(kColorWhite);
	pd->graphics->clearBitmap(frame_buffer, kColorWhite);

	control_object(pd, submarineObj);


	

	scene_object_draw(submarineObj, camera, pd, depth_buffer, bayer_matrix);
	scene_object_draw(mapObj, camera, pd, depth_buffer, bayer_matrix);
	//pd->graphics->drawScaledBitmap(frame_buffer, 0, 0, PIXEL_SCALE, PIXEL_SCALE);
	//draw_sbuffer(pd);
	pd->graphics->drawBitmap(frame_buffer, 0, 0, 0);
	//pd->graphics->drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, kColorBlack);
	pd->system->drawFPS(0, 0);

	return 1;
}




