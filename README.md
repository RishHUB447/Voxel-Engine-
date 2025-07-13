# MineCraft: C++ Edition

> Tech Stack : C++, OpenGL, GLFW, ASSIMP, ImGUI, FastNoise

![Thumbnail](assets/MinecraftC++.png)
### Highly dynamic terrain generation
![1](assets/1.png)
### Multiple Biomes
![2](assets/2.png)
### Crazy naturally generated stuctures
![3](assets/3.png)
### Decorate your world !
![4](assets/4.png)

## Interactivity instructions
- Normal "W-A-S-D" for movement
- "W + LCTRL" for sprint
- "Space" for jump
- "LShift" for diving down fast when underwater
- "1-9" keys to toggle the "current block" that you can place
- "Esc" key to toggle "player-mouse-movement" 
- "V" to toggle FPP & TPP

## Build Instructions
### 🧰 Requirements
- CMake 3.15+
- Visual Studio 2022
- C++17
- MSYS2 

### 🧾 Build Steps (Release Mode)
- cd into the root folder
- Use MSYS2 MINGW64 terminal

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
- The exe will generate in the root folder, have fun !