#pragma once
#ifdef __cplusplus



extern "C" {
#endif
	void registerBuffer(unsigned int texId);
	void unregisterbuffer();
	void updateframe(int w,int h,float* data);
	void initcuda(int w, int h);
	void freecuda();

#ifdef __cplusplus


}
#endif