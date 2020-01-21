---
history: true
width: 1280
height: 720
margin: 0.05
theme: white
#transition: cube
slideNumber: true
header-includes: |
    <style>
    img { max-height:600px !important; }
    video { max-height:600px !important; }
    .reveal section.nospace video { max-height:700px !important; }
    .reveal .slides > section.nospace, .reveal .slides > section > section.nospace {
        padding: 0 0;
    }
    /*.reveal h1 { font-size: 1.5em; }*/
    .reveal section.nospace h2 {
        font-size: 1.1em;
        margin: 0;
    }
    iframe {
        width: 1024px; height: 768px;
    }
    .reveal section img.noborder {
        border: none;
    }
    </style>
progress: "true, autoPlayMedia: true"
title: Motion Interpolation with OpenCL
---

## Example

<video data-src="media/ball-small.mp4" loop></video>

## Motivation / Prior Art

-   Why? 60 > 24. Panning in movies looks horrible
-   TV features like Motion Boost / TruMotion (controversial)
-   Smooth Video Project ([svp-team.com](svp-team.com))
-   Video Compression
-   Slow Motion in Smartphones, Adobe Premiere etc.

## Overview

1. Motion Estimation via Block Matching:

    Find most similar 16x16 pixel block close to current block

2. Render Frame

    Move by motion vectors and blend blocks

. . .

All videos are screen recordings (real time!)

## Motion Estimation

For every OpenCL local 2D group:

-   Load a 3x3 grid of 16x16 blocks of both images into local memory
-   Compute the SAD for moving the center block to every possible offset <small>(Sum of absolute differences)</small>
-   Bias for e.g. consistency with neighbourhood
-   Do everything a second time for backwards motion

##

![](media/1.svg){width=70% .noborder}

## {transition=none}

![](media/4.svg){width=70% .noborder}

## {transition=none}

![](media/2.svg){width=70% .noborder}

## Motion Estimation Example

:::::::::::::: {.columns}
::: {.column width="50%"}
![1 iteration](media/iter-1.png){width=90%}
:::
::: {.column width="50%" .fragment}
![5 iterations](media/iter-2.png){width=90%}
:::
::::::::::::::

## Motion Vectors

<video data-src="media/mvecs-small.mp4" loop></video>

## Diamond Block Search

Exhaustive search is slow. Lots of research on search patterns.

![<small>Zhu, Ma: A new-diamond search algorithm for fast block-matching motion estimation (2000)</small>](media/diamond.png){width=40%}

## Frame Rendering

1. Shift Motion vectors by themselves by some factor depending on time
2. For every output pixel, read input pixel shifted by negative motion vector \* deltatime
3. Do this four times (img1 forward, img1 backward, img2 forward, img2 backward)
4. Blend pixels depending on deltatime and visibility

## Shift Forward Motion Vectors

<video data-src="media/shifted-mvecs-small.mp4" loop></video>

## Rendered

<video data-src="media/mvecs-render-small.mp4" loop></video>

## Full Debug Output

<video data-src="media/2020-01-07-debug1-small.mp4" loop></video>

## Movie Example 1 (slowed 50%) {.nospace}

<video data-src="media/movie-example1-small.mp4" loop></video>

## Movie Example 2 (slowed 50%) {.nospace}

<video data-src="media/movie-example2-small.mp4" loop></video>

## Benchmark

![Measuring everything between `glFinish()` and `clFinish()`](media/by-method.svg){width=60%}

##

![](media/by-blocksize.svg){width=70%}

##

![](media/by-videosize.svg){width=70%}

## Link to Code

<https://github.com/phiresky/opencl-motion-interpolation>
