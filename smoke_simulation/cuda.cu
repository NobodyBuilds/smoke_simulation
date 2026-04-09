#include <cuda_runtime.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cuda_gl_interop.h>

#include <cuda_runtime_api.h>
#include <device_launch_parameters.h>
#include <iostream>
#include <math_constants.h>
#include <math_functions.h>
#include "cuda.h"
#include"param.h"

#define BLOCKS(n) ((n + 255) / 256)
#define THREADS 256

float* d_data = nullptr;
float4* data1 = nullptr; // x= density, y= velocity_x, z= velocity_y, w= pressure
float4* data2 = nullptr;// x= divergence,y= tempdensity,z= tempvx, w= tempvy

extern "C" void initcuda(int w,int h) {


	cudaMalloc(&data1, w * h * sizeof(float4));
	cudaMalloc(&data2, w * h * sizeof(float4));
	cudaMalloc(&d_data, w * h * sizeof(float));
}
extern "C" void freecuda() {
	cudaFree(data1);
	cudaFree(data2);
	cudaFree(d_data);
	data1 = nullptr;
	data2 = nullptr;
	d_data = nullptr;
}

static cudaGraphicsResource* d_tex = nullptr;

extern "C" void registerBuffer(unsigned int texId) {

	cudaError_t err = cudaGraphicsGLRegisterImage(&d_tex, texId, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
	if (err != cudaSuccess) {
		std::cerr << "Failed to register OpenGL texture with CUDA: " << cudaGetErrorString(err) << std::endl;
	}
	
}

extern "C" void unregisterbuffer() {
	if (d_tex) {
		cudaGraphicsUnregisterResource(d_tex);
		d_tex = nullptr;
	}
}

__global__ void fillKernel(cudaSurfaceObject_t surf, int w, int h, float* data) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= w || y >= h) return;
    float val = data[y * w + x];
    surf2Dwrite(val, surf, x * sizeof(float), y);
}

extern "C" void updateframe(int w,int h,float* h_data) {


	cudaMemcpy(d_data, h_data, w * h * sizeof(float), cudaMemcpyHostToDevice);
	cudaError_t att =cudaGetLastError();
	if (att) {
		printf("cudaMemcpy error: %s\n", cudaGetErrorString(att));
	}
	cudaGraphicsMapResources(1, &d_tex);

	cudaArray_t arr;
	cudaGraphicsSubResourceGetMappedArray(&arr, d_tex, 0, 0);

	cudaResourceDesc desc{};
	desc.resType = cudaResourceTypeArray;
	desc.res.array.array = arr;

	cudaSurfaceObject_t surf;
	cudaCreateSurfaceObject(&surf, &desc);

	dim3 block(16, 16);
	dim3 grid((w + 15) / 16, (h + 15) / 16);
	fillKernel << <grid, block >> > (surf, w, h,d_data);

	cudaDestroySurfaceObject(surf);
	cudaGraphicsUnmapResources(1, &d_tex);
	
}
////
//physics
struct data {
	float density=0.0f;
	float tilesize=0.0f;
	float sor = 0.0f;
	float visc = 0.0f;
	float dissipation = 0.0f;
	float damp = 0.0f;
	int w = 0;
	int h = 0;
	int itterations = 0;
};
__constant__ data params;
extern "C" void copyparams() {
	data h_params;
	h_params.density = settings.density;
	h_params.tilesize = settings.tilesize;
	h_params.sor = settings.sor;
	h_params.visc = settings.visc;
	h_params.dissipation = settings.dissipation;
	h_params.damp = settings.damp;
	h_params.w = settings.w;
	h_params.h = settings.h;
	h_params.itterations = settings.itters;

	cudaMemcpy(&params, &h_params, sizeof(data),cudaMemcpyHostToDevice);
}
__device__ int issolid(int x, int y,int w,int h) {
	if(x<=0 || x>=w-1 || y<=0 || y>=h-1) return 1;
	return 0;
}

__global__ void divergenceKernel(float4* data1, float4* data2) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int w = params.w;
	int i = y * w + x;
	
		int h = params.h;
		float vtop = (y > 0 && !issolid(x, y - 1, w, h)) ? data1[i - w].z : 0.f;
		float vbottom = (y < h - 1 && !issolid(x, y + 1, w, h)) ? data1[i + w].z : 0.f;
		float vleft = (x > 0 && !issolid(x - 1, y, w, h)) ? data1[i - 1].y : 0.f;
		float vright = (x < w - 1 && !issolid(x + 1, y, w, h)) ? data1[i + 1].y : 0.f;

		float gradx = (vright - vleft) / params.tilesize;
		float grady = (vbottom - vtop) / params.tilesize;

		data2[i].x = gradx + grady;
	
	
}



extern "C" void updatephysics() {

	
}

