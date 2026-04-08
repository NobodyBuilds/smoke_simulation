# Smoke Sim

A real-time 2D Eulerian fluid simulation built from scratch in C++/OpenGL, implementing the incompressible Navier-Stokes equations on a grid. Currently runs on CPU with CUDA acceleration planned.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![Status](https://img.shields.io/badge/status-active-brightgreen)
---
DEMO
<img width="985" height="881" alt="Screenshot 2026-04-08 163248" src="https://github.com/user-attachments/assets/0c99b09e-973b-4576-a0e6-0ffbbec5f8f9" />

---

## Features

- **Incompressible Euler fluid** — divergence-free velocity field via Gauss-Seidel pressure projection
- **Semi-Lagrangian advection** — unconditionally stable velocity and density advection
- **Vorticity confinement** — restores small-scale swirl lost to numerical dissipation
- **Implicit diffusion** — stable density diffusion via iterative solve
- **Buoyancy** — density-driven upward force for natural smoke rise
- **Interactive mouse input** — inject smoke and stir velocity with right/left mouse buttons
- **Adjustable brush radius** — scroll wheel to resize brush
- **ImGui panel** — live tuning of all simulation parameters at runtime
- **Window resize support** — grid recomputes on resize, no crashes
- **Divergence error display** — real-time incompressibility metric for debugging

---

## Physics

The simulation solves the incompressible Navier-Stokes equations on a 2D Eulerian grid each timestep at a fixed 120Hz:

```
1. Apply forces       (buoyancy, mouse injection)
2. Advect velocity    (semi-Lagrangian, self-advection)
3. Compute divergence
4. Solve pressure     (Gauss-Seidel with SOR)
5. Project velocity   (subtract pressure gradient → divergence-free)
6. Vorticity confinement
7. Advect density
8. Diffuse density    (implicit Gauss-Seidel)
9. Dissipate density  (exponential decay)
10. Damp velocity
```

---

## Dependencies

| Library | Purpose |
|---|---|
| OpenGL 3.3 | Rendering |
| GLFW | Window and input |
| GLAD | OpenGL loader |
| ImGui | Runtime UI |
| CUDA *(planned)* | GPU acceleration |

---

## Building

> Requires MSVC or GCC with C++17, OpenGL 3.3 capable GPU.

```bash
# Clone
git clone https://github.com/NobodyBuilds/smoke_sim
cd smoke_sim

# Build with your preferred system (CMake/Makefile/VS project)
```

---

## Controls

| Input | Action |
|---|---|
| Right Mouse | Inject smoke + velocity |
| Left Mouse | Stir velocity only |
| Scroll Wheel | Resize brush |
| K | Reset simulation |
| Escape | Quit |

---

## Tunable Parameters (ImGui)

| Parameter | Description |
|---|---|
| `tilesize` | Grid cell size in pixels — smaller = higher resolution |
| `viscosity` | Density diffusion rate |
| `dissipation` | How fast smoke fades |
| `buoyancy` | Upward force strength |
| `vorticity` | Vortex confinement intensity |
| `velDamping` | Velocity decay rate |
| `SOR` | Pressure solver relaxation factor |
| `iterations` | Pressure solve iterations — higher = more accurate |

---

## Architecture

```
Source.cpp       — main loop, input, sim orchestration
shader.h         — inline GLSL vertex + fragment shaders
cuda.h / .cu     — CUDA stubs (future GPU kernels)
param.h          — settings struct shared across files
ui.h             — ImGui panel
```

---

## About

Built from scratch without game engines or pre-built solvers. Math-first, correctness before performance. CUDA port in progress — the goal is to scale to high-resolution grids at real-time framerates on consumer hardware.

> GitHub: [NobodyBuilds](https://github.com/NobodyBuilds)
