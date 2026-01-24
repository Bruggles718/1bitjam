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

#include "SceneObject.hpp"
#include "WFObjLoader.hpp"
#include "utils.hpp"
#include "ScreenGlobals.hpp"

static int update(void* userdata);
const char* fontpath = "/System/Fonts/Asheville-Sans-14-Bold.pft";
LCDFont* font = NULL;

SceneObject scene_object;
Camera camera;
std::vector<float> depth_buffer;
std::vector<std::vector<float>> bayer_matrix;

LCDBitmap* frame_buffer;
uint8_t* fb_data;
int rowbytes;

#ifdef _WINDLL
__declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
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

		scene_object = wf_obj_loader.create_scene_object_from_file(
			"icosahedron.obj", pd
		);

		scene_object.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
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


#define TEXT_WIDTH 86
#define TEXT_HEIGHT 16

int x = (400-TEXT_WIDTH)/2;
int y = (240-TEXT_HEIGHT)/2;
int dx = 1;
int dy = 2;

glm::quat rotation_quat;

static int update(void* userdata)
{
	std::fill(depth_buffer.begin(), depth_buffer.end(), -INFINITY);

	PlaydateAPI* pd = (PlaydateAPI*)userdata;

	pd->graphics->clearBitmap(frame_buffer, kColorWhite);

	float rot_speed = 1.0f;
	scene_object.rotate(rot_speed, glm::vec3(0.0f, 1.0f, 0.0f));
	scene_object.rotate(rot_speed, glm::vec3(1.0f, 0.0f, 0.0f));
	
	scene_object.draw(camera, pd, depth_buffer, bayer_matrix);
	pd->graphics->drawBitmap(frame_buffer, 0, 0, kBitmapUnflipped);
        
	pd->system->drawFPS(0,0);

	return 1;
}

