#pragma once
#ifdef __cplusplus



extern "C" {
#endif
	void registerBuffer(unsigned int texId);
	void unregisterbuffer();
	void updateframe();
	void updatephysics();
	void copyparams();
	void addDensity(int r, int c);
	void addvelocity(int r, int c, float dmx, float dmy, float frametime);
	void initcuda();
	void freecuda();

#ifdef __cplusplus


}
#endif