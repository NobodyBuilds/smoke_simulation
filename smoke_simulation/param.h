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
	float sor = 1.1f;
	float bouyancy = 0.0f;
	float visc = 0.001f;
	float dissipation = 0.01f;
	float vorticity = 5.0f;
	float vscale = 10.0f;
	float dscale = 3.0f;
	int diverror = 0;
	float damp = 0.99f;
	float k = 0.5f;
	int fpsCount = 0;
	int w = 0, h = 0;
	int itters = 40;
};
extern param settings;