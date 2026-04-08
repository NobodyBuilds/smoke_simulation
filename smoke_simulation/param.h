#pragma once
struct param {
	float fps = 0.0f;
	float fpsTimer = 0.0f;
	float maxFps = 0.0f;
	float minFps = 0.0f;
	float avgFps = 0.0f;
	float fuc_ms = 0.0f;
	float tilesize = 10.0f;
	float radius = 50.0f;
	float radiuscells = 0.0f;
	float density = 1.0f;
	int fpsCount = 0;
	int w = 0, h = 0;
	int itters = 20;
};
extern param settings;