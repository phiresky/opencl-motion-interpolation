Needs C++20 compiler (e.g. GCC9 or something)

Original OpenGL/OpenCL wrapper code based on [GPUC](https://cg.ivd.kit.edu/lehre/ws2020/index_2199.php) practical course.

```
mkdir build && cd build
cmake ../src
make
./MotionInterpolation

# interactive keys:

-   wasd: move around
-   q / e: zoom in / out
-   1: debug show mode
-   2: final output only show mode
-   m: enable march mode (load new images when interpolation done) (then double press m to reset frame to start)
-   f: enable fast mode (24fps not one fps)
-   p: print current orientation

```bash
make && ./MotionInterpolation 'Assets/b/59.44/%05d.png' 3 32 1 1
make && ./MotionInterpolation 'Assets/ball-crop/ball%04d.png' 176 16 0 2
make && ./MotionInterpolation 'Assets/blender-spring1/%04d.png' 26 32
```
