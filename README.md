## Noice

_Visualize 3D objects purely through white noise pixel movement._

Interactive sandbox app built around this graphics effect.
Supports loading custom 3D models in OBJ format via drag-and-drop.

Programmed in C++17 using OpenGL, GLFW, ImGui and various utility libraries.
Dependencies are fetched automatically on configure/build (see below).

The flowfield is generated based on UV coordinates. The final per pixel flow
vector depends on: sampled object tangent, angle/distance to perspective
camera, current object/camera movement, accumulated rest from previous frames.
This creates the illusion of noise pixels sticking to and moving smoothly along
the object surface.

Any 3D game that doesn't heavily rely on textures (or UI) could potentially use
this effect. I recommend precomputing flow maps as it's rather expensive to
generate. The effect itself is fairly cheap using only one vert/frag pass and two
compute shader passes. I would love to see games use this effect!

### Sandbox Modes (Object, Text, Paint)

![Image](https://github.com/user-attachments/assets/d7f135d9-f90f-4a22-8f8d-6588b6f2825c)

![Image](https://github.com/user-attachments/assets/226abf5c-5ad2-4300-8e32-39de81e68454)

![Image](https://github.com/user-attachments/assets/49703d33-9ed4-4995-a151-7c559ee8b356)

### Screenshot Feature

Collects and combines frames to visualize pixel movement (using e.g. sum of absolute differences).

<img width="1662" height="1080" alt="Image" src="https://github.com/user-attachments/assets/0b70b11d-faa2-4a7a-a6c0-cd2154758447" />

### Demo Video

https://github.com/user-attachments/assets/f3cc13a0-2750-4cd8-b10a-442c2d8176a5

### How to Build and Run

Tested on Linux and Windows (MSVC and MinGW).
Uses Ninja build system by default.

```
git clone https://github.com/erikrl2/Noice.git
cd Noice
cmake --preset release
cmake --build build/release
build/release/Noice
```
