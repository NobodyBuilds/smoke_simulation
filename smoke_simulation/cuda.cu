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
#include<vector>
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = (call); \
        if (err != cudaSuccess) \
            printf("[CUDA ERROR] %s:%d — %s: %s\n", __FILE__, __LINE__, #call, cudaGetErrorString(err)); \
    } while(0)
#define BLOCKS(n) ((n + 255) / 256)
#define THREADS 256
//TODO :
//- use uchar for solid edges storing in 1 and 0
//-merge possible kernels to reduce global memory access



//data storage for grid and its values
float* density = nullptr;
float* vx = nullptr;
float* vy = nullptr;
float* pressure = nullptr;
//float* temppressure = nullptr;
float* tempd = nullptr;
float* tempvx = nullptr;
float* tempvy = nullptr;
float* vorticity = nullptr;
float* divergence = nullptr; //divergence

extern "C" void initcuda() {

	int s = settings.w * settings.h;

	cudaMalloc(&density, s * sizeof(float));
	cudaMalloc(&vx, s * sizeof(float));
	cudaMalloc(&vy, s * sizeof(float));
	cudaMalloc(&pressure, s * sizeof(float));
	cudaMalloc(&tempd, s * sizeof(float));
	cudaMalloc(&tempvx, s * sizeof(float));
	cudaMalloc(&tempvy, s * sizeof(float));
	cudaMalloc(&vorticity, s * sizeof(float));
	cudaMalloc(&divergence, s * sizeof(float));
	//cudaMalloc(&temppressure,  s* sizeof(float));

	cudaMemset(density, 0, s * sizeof(float));
	cudaMemset(vx, 0, s * sizeof(float));
	cudaMemset(vy, 0, s * sizeof(float));
	cudaMemset(pressure, 0, s * sizeof(float));
	//cudaMemset(temppressure, 0, s * sizeof(float));
	cudaMemset(tempd, 0, s * sizeof(float));
	cudaMemset(tempvx, 0, s * sizeof(float));
	cudaMemset(tempvy, 0, s * sizeof(float));
	cudaMemset(vorticity, 0, s * sizeof(float));
	cudaMemset(divergence, 0, s * sizeof(float));
	cudaError_t a = cudaGetLastError();
	if (a) printf("memory allocation error : %s\n", cudaGetErrorString(a));
}
extern "C" void freecuda() {
	cudaFree(density);
	cudaFree(vx);
	cudaFree(vy);
	cudaFree(pressure);
	cudaFree(tempd);
	cudaFree(tempvx);
	cudaFree(tempvy);
	cudaFree(vorticity);
	cudaFree(divergence);
	//cudaFree(temppressure);
	density = nullptr;
	vx = nullptr;
	vy = nullptr;
	pressure = nullptr;
	tempd = nullptr;
	tempvx = nullptr;
	tempvy = nullptr;
	vorticity = nullptr;
	divergence = nullptr;
	//temppressure = nullptr;
	cudaError_t a = cudaGetLastError();
	if (a) printf("memory free error : %s \n", cudaGetErrorString(a));
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

extern "C" void updateframe() {


	//cudaMemcpy(d_data, h_data, w * h * sizeof(float), cudaMemcpyHostToDevice);
	/*cudaError_t att =cudaGetLastError();
	if (att) {
		printf("cudaMemcpy error: %s\n", cudaGetErrorString(att));
	}*/
	cudaGraphicsMapResources(1, &d_tex);

	cudaArray_t arr;
	cudaGraphicsSubResourceGetMappedArray(&arr, d_tex, 0, 0);

	cudaResourceDesc desc{};
	desc.resType = cudaResourceTypeArray;
	desc.res.array.array = arr;

	cudaSurfaceObject_t surf;
	cudaCreateSurfaceObject(&surf, &desc);

	dim3 block(16, 16);
	dim3 grid((settings.w + 15) / 16, (settings.h + 15) / 16);
	fillKernel << <grid, block >> > (surf, settings.w, settings.h,density);

	cudaDestroySurfaceObject(surf);
	cudaGraphicsUnmapResources(1, &d_tex);
	
}
////
//physics
// constant data for simulation parameters, copied from host to device at the start of the simulation
struct data {
	float density=0.0f;
	float tilesize=0.0f;
	float sor = 0.0f;
	float visc = 0.0f;
	float dissipation = 0.0f;
	float bouyancy = 0.0f;
	float vorticity = 0.0f;
	float vscale = 0.0f;
	float dscale = 0.0f;
	float dt = 0.0f;
	int w = 0;
	int h = 0;
	int itterations = 0;
};
__constant__ data params;
data h_params;
extern "C" void copyparams() {


	h_params.density = settings.density;
	h_params.tilesize = settings.tilesize;
	h_params.sor = settings.sor;
	h_params.visc = settings.visc;
	h_params.dissipation = settings.dissipation;
	h_params.bouyancy = settings.bouyancy;
	h_params.vorticity = settings.vorticity;
	h_params.vscale = settings.vscale;
	h_params.dscale = settings.dscale;
	h_params.dt = settings.fdt;
	h_params.w = settings.w;
	h_params.h = settings.h;
	h_params.itterations = settings.itters;

	cudaMemcpyToSymbol(params, &h_params, sizeof(data));
	cudaError_t a = cudaGetLastError();
	if (a)printf("copy parameters arros: %s\n ", cudaGetErrorString(a));
	else printf("params copied\n");
	
}
__device__ int issolid(int x, int y) {
	if(x<=0 || x>=params.w-1 || y<=0 || y>=params.h-1) return 1;
	return 0;
}

__global__ void dataSwap(float* data, float* data2) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int w = params.w;
	int i = y * w + x;

	float temp = data[i];
	data[i] = data2[i];
	data2[i] = temp;
}
//no race condition kernels
__global__ void divergenceKernel(float* vx,float* vy,float* pressure, float* div) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int w = params.w;
	int i = y * w + x;
	
		int h = params.h;
		float vtop = (y > 0 && !issolid(x, y - 1)) ? vy[i - w] : 0.0f;
		float vbottom = (y < h - 1 && !issolid(x, y + 1)) ? vy[i + w] : 0.0f;
		float vleft = (x > 0 && !issolid(x - 1, y)) ? vx[i - 1] : 0.0f;
		float vright = (x < w - 1 && !issolid(x + 1,y)) ? vx[i + 1] : 0.0f;

		float gradx = (vright - vleft) *0.5f;
		float grady = (vbottom - vtop) * 0.5f;

		div[i] = gradx + grady;
		pressure[i] = 0.f; // reset pressure
	
	
}
__global__ void projectkernel(float* vx,float* vy,float* pressure) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;

	int i = y * params.w + x;
	float k = params.dt / (params.density * 2.0f);
	

	if(issolid(x,y)){
		vx[i] = 0.f;
		vy[i] = 0.f;
		return;
	}
	if(issolid(x-1,y)|| issolid(x+1,y))
		vx[i] = 0.f;
	else
	{
		vx[i] += -k * (pressure[i + 1] - pressure[i - 1]);
	}

	if(issolid(x,y-1)||issolid(x,y+1))
		vy[i] = 0.f;
	else {
		vy[i] -= k * (pressure[i + params.w] - pressure[i - params.w]);
		vy[i] += params.bouyancy * params.dt;
	}
}
 __device__ float clamp(float val, float min,float max) {
	if (val < min) val = min;
	if (val > max) val = max;
	return val;

 }
__device__ float sample(float* data,float x,float y){
   x= clamp(x,0.5f,params.w-1.5f);
   y= clamp(y,0.5f,params.h-1.5f);
   int x0 = (int)x, y0 = (int)y;
   int x1=x0+1, y1=y0+1;
   float tx=x-x0, ty=y-y0;
   return (1-tx)*(1-ty)*data[y0*params.w+x0]+tx*(1-ty)*data[y0*params.w+x1]+(1-tx)*ty*data[y1*params.w+x0]+tx*ty* data[y1*params.w+x1];
}


__global__ void advectvelocity(float* vx,float* vy,float* tempvx,float* tempvy) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;

	int i = y * params.w + x;
	if(issolid(x,y) ){
		tempvx[i] = 0.f;
		tempvy[i] = 0.f;
		
		return;
	}
	float ux= clamp(vx[i],-1.0f/params.dt,1.0f/params.dt);
	float uy= clamp(vy[i],-1.0f/params.dt,1.0f/params.dt);

	float px=x-ux*params.dt;
	float py=y-uy*params.dt;
	tempvx[i] = sample(vx,px,py);
	tempvy[i] = sample(vy,px,py);



}
__global__ void advectdensity(float* vx,float* vy,float* density,float* tempd){

	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i = y * params.w + x;
	if(issolid(x,y) ){
		tempd[i] = 0.f;
		return;
	}
	float px=x- vx[i]*params.dt;
	float py=y- vy[i]*params.dt;
	tempd[i] = sample(density,px,py);
}

__global__ void zerobounds(float* data) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;

	int i = y * params.w + x;
	if (x == 0 || x == params.w - 1 || y == 0 || y == params.h - 1) {
		data[i] = 0.f;
	}
}
 
__global__ void zerovelocitybounds(float* data1 ,float* data2) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;

	int i = y * params.w + x;
	if (x == 0 || x == params.w - 1 || y == 0 || y == params.h - 1) {
		data1[i] = 0.f;
		data2[i] = 0.f;
	}
}
 
__global__ void dissipateKernel(float* data) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i=y* params.w + x;
	data[i] *= 1.f - params.dissipation * params.dt;
}

__global__ void vorticityKernel1(float* vx,float* vy,float* vorticity) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;

	int i = y * params.w + x;
	if(issolid(x,y) ){
		vorticity[i] = 0.f;
		return;
	}
	float dx=(vy[i+1]-vy[i-1])*0.5f;
	float dy = (vx[i + params.w] - vx[i - params.w]) * 0.5f;

	vorticity[i] =dx-dy;
}


__global__ void vorticityKernel2(float* vx,float* vy,float* v) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;

	int i = y * params.w + x;
	if(issolid(x,y)){
		return;
	}
	float dx=(fabsf(v[i+1]) - fabsf(v[i-1]))*0.5f;
	float dy=(fabsf(v[i+params.w]) - fabsf(v[i-params.w]))*0.5f;
	float len=sqrtf(dx*dx+dy*dy)+1e-5f;
	dx/=len;
	dy/=len;

	float w=v[i];

	vx[i] += params.vorticity * (dy*w)*params.dt;
	vy[i] += params.vorticity * (-dx*w)*params.dt;
}

__global__ void diffuseKernel(float* density,float* tempd){
	float a= params.visc *params.dt;
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i= y * params.w + x;
	if(issolid(x,y)) {
		return;
	}
	tempd[i]=(density[i] +a *(density[i-1]+density[i+1]+density[i-params.w]+density[i+params.w]))/(1+4*a);
}

__global__ void solvepressure1(float* pressure,float* divergence){
	float scale= params.density / params.dt;
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i= y * params.w + x;
	if (i % 2 == 0)return;
	int flowtop=issolid(x,y+1)?0:1;
	int flowbottom=issolid(x,y-1)?0:1;
	int flowleft=issolid(x-1,y)?0:1;
	int flowright=issolid(x+1,y)?0:1;
	int neighbors=flowtop+flowbottom+flowleft+flowright;

	if(issolid(x,y)||neighbors==0) {
		pressure[i] = 0.f;
		return;
	}

	float newpressure=((pressure[i-1]*flowleft)+(pressure[i+1]*flowright)+(pressure[i-params.w]*flowbottom)+(pressure[i+params.w]*flowtop)-scale*divergence[i])/(neighbors);
	float oldpressure=pressure[i];
	pressure[i]=oldpressure+(newpressure-oldpressure)*params.sor;

}
__global__ void solvepressure2(float* pressure,float* divergence){
	float scale= params.density / params.dt;
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i= y * params.w + x;
	if (i % 2 != 0)return;
	int flowtop=issolid(x,y+1)?0:1;
	int flowbottom=issolid(x,y-1)?0:1;
	int flowleft=issolid(x-1,y)?0:1;
	int flowright=issolid(x+1,y)?0:1;
	int neighbors=flowtop+flowbottom+flowleft+flowright;

	if(issolid(x,y)||neighbors==0) {
		pressure[i] = 0.f;
		return;
	}

	float newpressure=((pressure[i-1]*flowleft)+(pressure[i+1]*flowright)+(pressure[i-params.w]*flowbottom)+(pressure[i+params.w]*flowtop)-scale*divergence[i])/(neighbors);
	float oldpressure=pressure[i];
	pressure[i]=oldpressure+(newpressure-oldpressure)*params.sor;

}
__global__ void adddensitykernel(int row,int col,float* density,float radiuscells) {
	bool test = true;
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i= y * params.w + x;
	float dx= x-col;
	float dy= y-row;
	float dist=sqrtf(dx*dx+dy*dy);
	if(dist<radiuscells){
		density[i] += params.dscale * params.dt;
	}
	
	

}

extern "C" void addDensity(int row,int col) {
	dim3 block(16, 16);
	dim3 grid((settings.w + 15) / 16, (settings.h + 15) / 16);
adddensitykernel << <grid, block >> > (row, col, density, settings.radiuscells);
cudaError_t a = cudaGetLastError();
if (a)printf("add density error: %s\n ", cudaGetErrorString(a));

}
__global__ void velocitykernel(int row,int col,float radius,float dmx,float dmy,float* vx,float* vy,float frametime) {
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if (x >= params.w || y >= params.h) return;
	int i= y * params.w + x;
	if (frametime < 1e-5f) return;
	float dx= x-col;
	float dy= y-row;
	float dist=sqrtf(dx*dx+dy*dy);
	if(dist<=radius){
	//	printf("vx BEFORE: %f\n", vx[i]);
		vx[i] += (float)(dmx /params.tilesize /frametime)*params.vscale *params.dt;
		vy[i] += (float)(dmy /params.tilesize /frametime)*params.vscale *params.dt;

		//printf("v %3f %3f \n", vx[i], vy[i]);
		//printf("dm %3f %3f \n", dmx, dmy);
		//printf("vscale %3f \n", params.vscale);
		//printf("tile %3f \n", params.tilesize);
		//printf("dt %3f \n", params.dt);
		//printf("frametime %3f \n", frametime);
	}
}
extern "C" void addvelocity(int row,int col,float dmx,float dmy,float frametime) {
	dim3 block(16, 16);
	dim3 grid((settings.w + 15) / 16, (settings.h + 15) / 16);
	velocitykernel<<<grid,block>>>(row,col,settings.radiuscells,dmx,dmy,vx,vy,frametime);
	cudaError_t a = cudaGetLastError();
	if (a)printf("add velocity error: %s\n ", cudaGetErrorString(a));

}


void inspectField(float* d_field, int n, const char* label) {
	std::vector<float> h(n);
	cudaMemcpy(h.data(), d_field, n * sizeof(float), cudaMemcpyDeviceToHost);
	float mn = *std::min_element(h.begin(), h.end());
	float mx = *std::max_element(h.begin(), h.end());
	int nans = 0, infs = 0;
	for (float v : h) {
		if (std::isnan(v)) nans++;
		if (std::isinf(v)) infs++;
	}
	printf("[DEBUG] %-20s  min=%.4f  max=%.4f  nans=%d  infs=%d\n", label, mn, mx, nans, infs);
}
__global__ void debugParams() {
	if (threadIdx.x == 0 && blockIdx.x == 0)
		printf("[PARAMS] w=%d h=%d dt=%.6f density=%.4f sor=%.4f visc=%.4f\n",
			params.w, params.h, params.dt, params.density, params.sor, params.visc);
}

extern "C" void updatephysics() {
	if (settings.debug) {
		debugParams << <1, 1 >> > ();
		cudaDeviceSynchronize();
		--settings.debug;
	}
	int n = settings.w * settings.h;
	dim3 block(16, 16);
	dim3 grid((settings.w + 15) / 16, (settings.h + 15) / 16);
	advectvelocity << <grid, block >> > (vx, vy, tempvx, tempvy);
	std::swap(vx, tempvx);
	std::swap(vy, tempvy);
	zerovelocitybounds << <grid, block >> > (vx, vy);
	
	divergenceKernel << <grid, block >> > (vx, vy, pressure, divergence);
	
	for (int i = 0; i < settings.itters; i++) {
		solvepressure1 << <grid, block >> > ( pressure,divergence);
		solvepressure2 << <grid, block >> > ( pressure,divergence);
		
	
	 } 
	 //dataSwap << <grid, block >> > (pressure, temppressure);
		//std::swap(pressure, temppressure);

	
	projectkernel << <grid, block >> > (vx, vy, pressure);
	
	vorticityKernel1 << <grid, block >> > (vx, vy, vorticity);

	vorticityKernel2 << <grid, block >> > (vx, vy, vorticity);
	
	advectdensity << <grid, block >> > (vx, vy, density, tempd);
	std::swap(density, tempd);
	
	zerobounds << <grid, block >> > (density);
	
	diffuseKernel << <grid, block >> > (density, tempd);
	std::swap(density, tempd);
	
	dissipateKernel << <grid, block >> > (density);
	

}

