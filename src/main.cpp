//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <memory>

#include "pd_api.h"
#include "pdcpp/pdnewlib.h"
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include "SceneObject.hpp"
#include "WFObjLoader.hpp"
#include "utils.hpp"
#include "ScreenGlobals.hpp"

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

SceneObject submarineObj;
SceneObject mapObj;
Camera camera;
std::vector<float> depth_buffer;
std::vector<std::vector<int>> bayer_matrix;

LCDBitmap* frame_buffer;
uint8_t* fb_data;
int rowbytes;

#ifdef __cplusplus
extern "C" {
#endif


#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
	eventHandler_pdnewlib(pd, event, arg);
	(void)arg; // arg is currently only used for event = kEventKeyPressed

	if ( event == kEventInit )
	{
		const char* err;
		font = pd->graphics->loadFont(fontpath, &err);
		
		if ( font == NULL )
			pd->system->error("%s:%i Couldn't load font %s: %s", __FILE__, __LINE__, fontpath, err);

		WFObjLoader wf_obj_loader;

		//scene_object = (SceneObject*)malloc(sizeof(SceneObject));

		submarineObj = wf_obj_loader.create_scene_object_from_file("submarine.obj", pd);
		mapObj = wf_obj_loader.create_scene_object_from_file("map.obj", pd);

		submarineObj.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
		mapObj.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
		depth_buffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT, -INFINITY);
		bayer_matrix = bayerMatrix(512);

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
#ifdef __cplusplus
}
#endif

void setPixel(PlaydateAPI* pd, int x, int y, int color) {
    if (x >= SCREEN_WIDTH || x < 0 || y > SCREEN_HEIGHT || y < 0) return;
    drawpixel(fb_data, x, y, rowbytes, color);
}


static float smoothed_pitch = 0.0f;
static float smoothed_roll = 0.0f;

glm::quat calculate_input_versor(PlaydateAPI* pd, float pitch_scale, float roll_scale)
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
	glm::quat q_pitch, q_roll, q_tilt;

	q_pitch = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	q_roll = glm::angleAxis(roll, glm::vec3(0.0f, 0.0f, 1.0f));
	q_tilt = q_pitch * q_roll;


	// -- ACCELEROMETER USER CALIBRATION (storing a user set offset)
	static glm::quat q_calibration = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	static bool is_calibrated = false;

	PDButtons current;
	pd->system->getButtonState(&current, NULL, NULL);

	if (current & kButtonDown) { //if the player hits down, the accelerometer will store its current value in q_calibration and use it as an offset
		q_calibration = q_tilt;
		q_calibration = glm::normalize(q_calibration);
		is_calibrated = true;
	}

	glm::quat corrected_tilt;
	if (is_calibrated) {
		glm::quat inverse = glm::inverse(q_calibration);
		corrected_tilt = inverse * q_tilt;
	}
	else {
		corrected_tilt = q_tilt;
	}

	glm::quat q_yaw = glm::angleAxis(glm::radians(pd->system->getCrankAngle()), glm::vec3(0.0f, 1.0f, 0.0f));

	glm::quat q_final= q_yaw * corrected_tilt;

	return glm::normalize(q_final);


}

static glm::vec4 sub_velocity = { 0.0f,0.0f,0.0f, 0.0f };
void control_object(PlaydateAPI* pd, SceneObject* obj) {
	//accelerometer and crank
	glm::quat tilt = calculate_input_versor(pd, 1.5f, 1.0f);
	obj->set_rotation(tilt);

	//calculate forward vector from tilt
	glm::vec4 forward = { 0.0f, 0.0f, -1.0f, 1.0f };
	glm::vec4 right = { -1.0f, 0.0f, 0.0f, 1.0f };
	glm::mat4 rot = glm::toMat4(tilt);
	forward = rot * forward;
	right = rot * right;
	forward = glm::normalize(forward);
	right = glm::normalize(right);

	//banking (roll effecting our forward vector)
	float bank_strength = 4;

	float bank = smoothed_roll * bank_strength;
	glm::vec4 side = right * bank;
	forward += side;
	forward = glm::normalize(forward);



	PDButtons current; pd->system->getButtonState(&current, NULL, NULL);

	//give her gas
	if (current & kButtonUp) {
		float thrust = 0.1f;
		glm::vec4 acceleration = forward * thrust;
		sub_velocity += acceleration;
	}

	//drag
	float drag = 0.92f;
	sub_velocity *= drag;



	glm::vec3 pos = obj->get_transform().m_position;
	pos += glm::vec3(sub_velocity);
	obj->set_position(pos);



	// -- CAMERA STUFF --

	float camera_distance = 10.0f;

	glm::vec4 backward = { 0.0f, 0.3f, 1.0f, 1.0f };

	rot = glm::toMat4(tilt);
	backward = rot * backward;
	backward = glm::normalize(backward);

	glm::vec3 cam_pos = obj->get_transform().m_position;

	glm::vec4 cam_offset = backward * camera_distance;

	cam_pos += glm::vec3(cam_offset);

	static glm::quat cam_rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	float rot_lerp = 0.15f;

	glm::quat blended = glm::slerp(cam_rot, tilt, rot_lerp);
	cam_rot = glm::normalize(blended);

	camera.SetRotation(cam_rot);
	camera.SetCameraEyePosition(cam_pos.x, cam_pos.y, cam_pos.z);


}

glm::quat rotation_quat;

static int update(void* userdata)
{
	std::fill(depth_buffer.begin(), depth_buffer.end(), -INFINITY);

	PlaydateAPI* pd = (PlaydateAPI*)userdata;

	pd->graphics->clearBitmap(frame_buffer, kColorWhite);

	control_object(pd, &submarineObj);
	float* depth_data = depth_buffer.data();
	submarineObj.draw(camera, pd, depth_data, bayer_matrix);
	mapObj.draw(camera, pd, depth_data, bayer_matrix);

	pd->graphics->drawBitmap(frame_buffer, 0, 0, kBitmapUnflipped);
        
	pd->system->drawFPS(0,0);

	return 1;
}

