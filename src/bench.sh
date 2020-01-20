#!/bin/bash
set -m
patterns=('Assets/ball-crop/ball%04d.png' 'Assets/blender-spring1/%04d.png' 'Assets/ball/ball%04d.png' 'Assets/b/59.44/full/%05d.png')
startoffsets=(176 26 176 3)
sizes=(570x340 960x402 1280x720 1920x1080 )
for blocksize in 16 24 32; do
    for diamond in 0 1; do
        for ((i=0;i<${#patterns[@]};++i)); do
            pattern="${patterns[i]}"
            startoffset="${startoffsets[i]}"
            size="${sizes[i]}"
            if ((diamond != 0)); then
                diamondstr="Diamond"
            else
                diamondstr="Exhaustive"
            fi
            x=$(../build/MotionInterpolation $pattern $startoffset $blocksize $diamond 1 | grep --line-buffered -a FPS | tail -n 1 | awk '{print $4}')
            echo ${blocksize}x${blocksize} $diamondstr $size $x
        done
    done
done

# ../build/Assignment