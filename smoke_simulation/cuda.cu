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

#define BLOCKS(n) ((n + 255) / 256)
#define THREADS 256


float* d_data = nullptr;

extern "C" void initcuda(int w,int h) {


	cudaMalloc(&d_data, w * h * sizeof(float));
}
extern "C" void freecuda() {
	if (d_data) {
		cudaFree(d_data);
		d_data = nullptr;
	}
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

__global__ void fillKernel(cudaSurfaceObject_t surf, int w, int h,float* data) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= w || y >= h) return;
	float val =data[y*w+x];
	
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