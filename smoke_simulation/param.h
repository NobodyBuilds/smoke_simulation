#pragma once
struct param {
	float fps = 0.0f;
	float fdt = 1.0f / 120.0f;
	float fpsTimer = 0.0f;
	float maxFps = 0.0f;
	float minFps = 0.0f;
	float avgFps = 0.0f;
	float fuc_ms = 0.0f;
	float tilesize = 5.0f;
	float radius = 50.0f;
	float radiuscells = 0.0f;
	float density = 1.0f;
	float sor = 1.93f;
	float bouyancy = 0.0f;
	float visc = 0.001f;
	float dissipation = 0.0f;
	float vorticity = 1.60f;
	float vscale = 2.0f;
	float dscale = 3.0f;
	float damp = 0.99f;

	float k = 0.5f;

	int diverror = 0;
	int fpsCount = 0;
	int w = 0, h = 0;
	int cells = w * h;
	int itters = 80;
	int debug = 5;
};
extern param settings;

struct MouseSample {
	int   row, col;
	float dmx, dmy;
	float frametime;
	int buttons;
};

struct ReplayBuffer {
	static constexpr int CAP = 2048;
	MouseSample samples[CAP];
	int  count = 0;
	int  head = 0;   // replay cursor
	bool recording = false;
	bool playing = false;
};
extern ReplayBuffer replay;