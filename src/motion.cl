#define LDSP1 ((int2)(-2, 0))
#define LDSP2 ((int2)(-1, -1))
#define LDSP3 ((int2)(0, -2))
#define LDSP4 ((int2)(1, -1))
#define LDSP5 ((int2)(2, 0))
#define LDSP6 ((int2)(1, 1))
#define LDSP7 ((int2)(0, 2))
#define LDSP8 ((int2)(-1, 1))

#define SDSP1 ((int2)(-1, 0))
#define SDSP2 ((int2)(0, -1))
#define SDSP3 ((int2)(1, 0))
#define SDSP4 ((int2)(0, 1))

//#define idx(arr, x, y) (arr)[(y)*width + (x)]
#define idx(arr, p) (arr)[(p).y * width + (p).x]

#define I2F(i) norm((i), -TILE_X, TILE_X)
#define F2I(f) unnorm((f), -TILE_X, TILE_X)

#define INF 1000000000  // less than int max
#define ONCE(ee)              \
  if (p.x == 0 && p.y == 0) { \
    ee                        \
  }

#define OOB(p) ((p).x < 0 || (p).y < 0 || (p).x >= width || (p).y >= height)

// assumes block1pos is valid index. block2pos may be out of bounds
int sad(local uchar block1[TILE_Y * 3][TILE_X * 3],
        local uchar block2[TILE_Y * 3][TILE_X * 3], int2 b1pos, int2 b2pos,
        int2 localpos) {
  // if (b1pos.x < 0 || b1pos.y < 0 || b1pos.x > 2 * TILE_X || b1pos.y > 2 *
  // TILE_Y)
  //    return INF;
  if (b2pos.x < 0 || b2pos.y < 0 || b2pos.x > 2 * TILE_X ||
      b2pos.y > 2 * TILE_Y)
    return INF;
  local int sum;
  if (localpos.x == 0 && localpos.y == 0) {
    sum = 0;
  }
  barrier(CLK_LOCAL_MEM_FENCE);
  int2 b1lpos = b1pos + localpos;
  int2 b2lpos = b2pos + localpos;
  atomic_add(&sum,
             abs(block1[b1lpos.y][b1lpos.x] - block2[b2lpos.y][b2lpos.x]));
  barrier(CLK_LOCAL_MEM_FENCE);
  return sum;
}

float norm(int val, int min, int max) {
  return (val - min) / ((float)max - min);
}

int unnorm(float oval, int min, int max) {
  return (int)(oval * (max - min) + min);
}

// void plotLine(global write_only float4 *img, uint width, uint height, int x0,
// int y0, int x1, int y1, float4 color0, float4 color1)

// c++ https://rosettacode.org/wiki/Xiaolin_Wu%27s_line_algorithm#C.2B.2B
void plotLineWu(global write_only float4 *img, uint width, uint height,
                float x0, float y0, float x1, float y1, float4 color,
                float4 color2) {
  float dist = distance((float2)(x0, y0), (float2)(x1, y1));
  float2 origorigin = (float2)(x0, y0);

#define swap(a, b) \
  {                \
    float tmp = a; \
    a = b;         \
    b = tmp;       \
  }
#define plot(x, y, br)                                                        \
  {                                                                           \
    int2 pos = (int2)(x, y);                                                  \
    float dist1 = distance(origorigin, (float2)(x, y));                       \
    idx(img, pos) = mix(idx(img, pos), mix(color, color2, dist1 / dist), br); \
  }
#define ipart(x) trunc(x)
#define fpart(x) ((x)-trunc(x))
#define rfpart(x) (1 - fpart(x))

  bool steep = fabs(y1 - y0) > fabs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }
  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  float dx = x1 - x0;
  float dy = y1 - y0;
  float gradient = (dx == 0) ? 1 : dy / dx;

  int xpx11;
  float intery;
  {
    const float xend = round(x0);
    const float yend = y0 + gradient * (xend - x0);
    const float xgap = rfpart(x0 + 0.5);
    xpx11 = (int)(xend);
    const int ypx11 = ipart(yend);
    if (steep) {
      plot(ypx11, xpx11, rfpart(yend) * xgap);
      plot(ypx11 + 1, xpx11, fpart(yend) * xgap);
    } else {
      plot(xpx11, ypx11, rfpart(yend) * xgap);
      plot(xpx11, ypx11 + 1, fpart(yend) * xgap);
    }
    intery = yend + gradient;
  }

  int xpx12;
  {
    const float xend = round(x1);
    const float yend = y1 + gradient * (xend - x1);
    const float xgap = rfpart(x1 + 0.5);
    xpx12 = (int)(xend);
    const int ypx12 = ipart(yend);
    if (steep) {
      plot(ypx12, xpx12, rfpart(yend) * xgap);
      plot(ypx12 + 1, xpx12, fpart(yend) * xgap);
    } else {
      plot(xpx12, ypx12, rfpart(yend) * xgap);
      plot(xpx12, ypx12 + 1, fpart(yend) * xgap);
    }
  }

  if (steep) {
    for (int x = xpx11 + 1; x < xpx12; x++) {
      plot(ipart(intery), x, rfpart(intery));
      plot(ipart(intery) + 1, x, fpart(intery));
      intery += gradient;
    }
  } else {
    for (int x = xpx11 + 1; x < xpx12; x++) {
      plot(x, ipart(intery), rfpart(intery));
      plot(x, ipart(intery) + 1, fpart(intery));
      intery += gradient;
    }
  }
}

void plotLine(global write_only float4 *img, uint width, uint height, int x0,
              int y0, int x1, int y1, float4 color0, float4 color1) {
  int zx0 = x0;
  int zy0 = y0;
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2, e2;

  for (;;) {
    if (x0 > 0 && y0 >= 0 && x0 < width && y0 < height) {
      int2 pos = (int2)(x0, y0);
      float m = y1 - y0 > x1 - x0 ? (((float)y0 - zy0) / (y1 - y0))
                                  : (((float)x0 - zx0) / (x1 - x0));
      idx(img, pos) = mix(color0, color1, m);
    }
    if (x0 == x1 && y0 == y1) break;
    e2 = err;
    if (e2 > -dx) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dy) {
      err += dx;
      y0 += sy;
    }
  }
}

// https://en.wikipedia.org/wiki/YUV#Y%E2%80%B2UV444_to_RGB888_conversion
float rgbToY(float4 rgb) {
  return clamp(0.299f * rgb.x + 0.587f * rgb.y + 0.114f * rgb.z, 0.0f, 1.0f);
}

#define SETIFBEST(loc) SETIFBESTBIASED(loc)

#define SETIFBESTUNBIASED(_loc)                                   \
  {                                                               \
    int2 loc = _loc;                                              \
    int val = sad(block1, block2, center, searchpos + (loc), lp); \
    if (val < bestval) {                                          \
      bestval = val;                                              \
      bestloc = searchpos + (loc);                                \
    }                                                             \
  }

#define SETIFBESTBIASED(_loc)                                                 \
  {                                                                           \
    int2 loc = _loc;                                                          \
    int2 tspos = searchpos + loc;                                             \
    int bias =                                                                \
        (int)(biasWeight * distance((float2)(tspos.x, tspos.y), biasCenter)); \
    int val = sad(block1, block2, center, tspos, lp) + bias;                  \
    if (val < bestval) {                                                      \
      bestval = val;                                                          \
      bestloc = tspos;                                                        \
    }                                                                         \
  }

int2 diamondSearch(local uchar block1[TILE_Y * 3][TILE_X * 3],
                   local uchar block2[TILE_Y * 3][TILE_X * 3], int2 center,
                   int2 lp, float2 biasCenter, float biasWeight) {
  int2 searchpos = center;  // (int2)(biasCenter.x, biasCenter.y); // center; //
                            // (int2)(biasCenter.x, biasCenter.y);
  // if (searchpos.x < 0 || searchpos.y < 0 || searchpos.x >= 2 * TILE_X ||
  // searchpos.y >= 2 * TILE_Y)
  //    searchpos = center;
  int i = 0;
  bool isSdsp = false;
  {
    int2 bestloc = searchpos;
    int bestval = INF;
    SETIFBEST((int2)(0, 0));
    SETIFBEST(4 * LDSP1)
    SETIFBEST(4 * LDSP2)
    SETIFBEST(4 * LDSP3)
    SETIFBEST(4 * LDSP4)
    SETIFBEST(4 * LDSP5)
    SETIFBEST(4 * LDSP6)
    SETIFBEST(4 * LDSP7)
    SETIFBEST(4 * LDSP8)
    // searchpos = bestloc;
    // SETIFBEST((int2)(0, 0));
    SETIFBEST(2 * LDSP1)
    SETIFBEST(2 * LDSP2)
    SETIFBEST(2 * LDSP3)
    SETIFBEST(2 * LDSP4)
    SETIFBEST(2 * LDSP5)
    SETIFBEST(2 * LDSP6)
    SETIFBEST(2 * LDSP7)
    SETIFBEST(2 * LDSP8)
    searchpos = bestloc;
  }
  while (true) {
    int2 bestloc = searchpos;
    int bestval = INF;

    SETIFBEST((int2)(0, 0));
    /*if (get_group_id(0) == 4 && get_group_id(1) == 6 && lp.x == 0 && lp.y ==
    0)
    {
        printf("Block %d", get_group_id(0));
        printf(",%d", get_group_id(1));
        printf(": searchpos=%d,%d", searchpos.x - ls.x, searchpos.y - ls.y);
        printf(" ssdp=%d, i=%d", isSdsp, i);
        printf(" bestsad=%d\n", bestval);
    }*/
    if (!isSdsp) {
      SETIFBEST(LDSP1)
      SETIFBEST(LDSP2)
      SETIFBEST(LDSP3)
      SETIFBEST(LDSP4)
      SETIFBEST(LDSP5)
      SETIFBEST(LDSP6)
      SETIFBEST(LDSP7)
      SETIFBEST(LDSP8)
      if (bestloc.x == searchpos.x && bestloc.y == searchpos.y) isSdsp = true;
      searchpos = bestloc;
    } else {
      SETIFBEST(SDSP1)
      SETIFBEST(SDSP2)
      SETIFBEST(SDSP3)
      SETIFBEST(SDSP4)
      /*if (bestloc.x == searchpos.x && bestloc.y == searchpos.y)
      {
      }*/
      searchpos = bestloc;
      break;
    }

    if (++i >= 1000) {
      if (lp.x == 0 && lp.y == 0) {
        printf("SHOUDL NOT APHEEN it=%i ", i);
        printf("%d", get_group_id(0));
        printf(",%d\n", get_group_id(1));
      }
      break;
    }
  }
  return searchpos;
}

int2 exhaustiveSearch(local uchar block1[TILE_Y * 3][TILE_X * 3],
                      local uchar block2[TILE_Y * 3][TILE_X * 3], int2 center,
                      int2 lp, float2 biasCenter, float biasWeight) {
  int2 searchpos = center;
  int2 bestloc = searchpos;
  int bestval = INF;
  for (int x = -TILE_X; x < TILE_X; x++) {
    for (int y = -TILE_Y; y < TILE_Y; y++) {
      int2 s = (int2)(x, y);
      SETIFBEST(s);
    }
  }
  return bestloc;
}

kernel __attribute__((reqd_work_group_size(TILE_X, TILE_Y, 1))) void
EstimateMotion(uint width, uint height, global read_only float4 *img_1,
               global read_only float4 *img_2,
               global write_only float4 *diffImg,
               global read_write float4 *mVecs, global float4 *unused1,
               global float4 *unused2, int iteration) {
  int2 p = (int2)(get_global_id(0), get_global_id(1));
  if (OOB(p)) return;
  // if (idx(img_1, p).x != 0.0f)
  //    printf("pixel is not null!!! %v2d\n", p);

  int2 ls = (int2)(get_local_size(0), get_local_size(1));
  int2 lp = (int2)(get_local_id(0), get_local_id(1));
  // load into local memory
  local uchar block1[TILE_Y * 3][TILE_X * 3];  // TILE_X = ls.x, TILE_Y = ls.y
  local uchar block2[TILE_Y * 3][TILE_X * 3];
  for (int ciy = 0; ciy < 3; ciy++) {
    for (int cix = 0; cix < 3; cix++) {
      int2 ci = (int2)(cix, ciy);
      int2 al = ci * ls + lp;
      int2 ag = (ci - 1) * ls + p;
      if (OOB(ag)) {
        block1[al.y][al.x] = 0;
        block2[al.y][al.x] = 0;
      } else {
        float luma1 = rgbToY(idx(img_1, ag));
        float luma2 = rgbToY(idx(img_2, ag));
        block1[al.y][al.x] = (uchar)(255 * luma1);
        block2[al.y][al.x] = (uchar)(255 * luma2);
      }
    }
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  int2 biasCenter = (int2)(0, 0);
  if (iteration > 0) {
    int2 T = (int2)(TILE_X, TILE_Y);

    int c = 0;
    float4 avg = 0;
#define MVIMAYB(delt)          \
  {                            \
    int2 pde = p + T * (delt); \
    if (!OOB(pde)) {           \
      avg += idx(mVecs, pde);  \
      c += 1;                  \
    }                          \
  }
    MVIMAYB((int2)(-1, -1))
    MVIMAYB((int2)(-1, 0))
    MVIMAYB((int2)(-1, 1))
    MVIMAYB((int2)(0, -1))
    MVIMAYB((int2)(0, 0))
    MVIMAYB((int2)(0, 1))
    MVIMAYB((int2)(+1, -1))
    MVIMAYB((int2)(+1, 0))
    MVIMAYB((int2)(+1, 1))
    avg /= c;
    biasCenter = (int2)(F2I(avg.x), F2I(avg.y));
    barrier(CLK_GLOBAL_MEM_FENCE);
  }
  int2 center = (int2)(ls.x, ls.y);  // center of nine tiles (=1x local size)
  float biasWeight = iteration == 0 ? 1.f : 10.f;
  int2 searchpos;
  if (USE_DIAMOND) {
    searchpos = diamondSearch(
        block1, block2, center, lp,
        ((float2)(center.x + biasCenter.x, center.y + biasCenter.y)),
        biasWeight);
  } else {
    searchpos = exhaustiveSearch(
        block1, block2, center, lp,
        ((float2)(center.x + biasCenter.x, center.y + biasCenter.y)),
        biasWeight);
  }
  int2 offset = searchpos - ls;
  idx(mVecs, p) = (float4)(I2F(offset.x), I2F(offset.y), 0, 1);

#define WRITEDEBUG 0
  if (WRITEDEBUG) {
    // diff img for debugging

    int val1 = block1[ls.y + lp.y][ls.x + lp.x];
    int val2 = block2[ls.y + lp.y][ls.x + lp.x];
    float diff =
        (val1 + val2) / 512.f;  // val1 / 256.f; //abs(val1 - val2) / 256.f;
    float4 di = (float4)(diff, diff, diff, 0.000000);

    if ((lp.x == 0 || lp.y == 0 || lp.x == TILE_X - 1 || lp.y == TILE_Y - 1))
      di = (float4)(0.2, 0.8, 0.2, 0);
    idx(diffImg, p) = di;

    // draw mvec line
    if (lp.x == TILE_X / 2 && lp.y == TILE_Y / 2) {
      int2 ofsd = (int2)(p.x + offset.x, p.y + offset.y);
      if (OOB(ofsd)) return;
      plotLineWu(diffImg, width, height, p.x, p.y, ofsd.x, ofsd.y,
                 (float4)(0.5, 0.5, 0.5, 1), (float4)(1, 0, 0, 1));
      idx(diffImg, ofsd) = (float4)(1, 0, 0, 1);
    }
  }
}

union hacky {
  int el[4];
  int4 i;
  float4 f;
};
kernel __attribute__((reqd_work_group_size(TILE_X, TILE_Y, 1))) void
ShiftVectors(uint width, uint height, global read_only float4 *img_1,
             global read_only float4 *img_2, global write_only float4 *diffImg,
             global read_only float4 *mVecs,
             global read_write float4 *_mVecsShifted, global float4 *_unused2,
             float t) {
  int2 p = (int2)(get_global_id(0), get_global_id(1));
  if (OOB(p)) return;
  global read_write union hacky *mVecsShifted = (global void *)_mVecsShifted;
  idx(mVecsShifted, p).i = (int4)0;
  barrier(CLK_GLOBAL_MEM_FENCE);
  float4 _mv = idx(mVecs, p);
  int2 mv = (int2)(F2I(_mv.x), F2I(_mv.y));
  int2 mvScaled = (int2)((int)round(t * mv.x), (int)round(t * mv.y));
  int2 outp = p + mvScaled;
  // idx(mVecsShifted, outp) = _mv;
  if (OOB(outp)) {
    return;
  }
#define AVG_SHIFTS 0
  if (AVG_SHIFTS) {
    atomic_add(&idx(mVecsShifted, outp).el[0], mv.x);
    atomic_add(&idx(mVecsShifted, outp).el[1], mv.y);
    atomic_add(&idx(mVecsShifted, outp).el[2], 1);  // weight
  } else {
    idx(mVecsShifted, outp).el[0] = mv.x;
    idx(mVecsShifted, outp).el[1] = mv.y;
    idx(mVecsShifted, outp).el[2] = 1;
  }
}

kernel __attribute__((reqd_work_group_size(TILE_X, TILE_Y, 1))) void
NormalizeMVecs(uint width, uint height, global read_only float4 *img_1,
               global read_only float4 *img_2,
               global write_only float4 *diffImg,
               global read_only float4 *mVecs,
               global read_write float4 *_mVecsShifted,
               global float4 *_unused2) {
  int2 p = (int2)(get_global_id(0), get_global_id(1));
  if (OOB(p)) return;
  barrier(CLK_GLOBAL_MEM_FENCE);
  global read_write union hacky *mVecsShifted =
      (global read_write void *)_mVecsShifted;

  int4 _mv = idx(mVecsShifted, p).i;

  float2 mv = (float2)(I2F(_mv.x), I2F(_mv.y));
  int scale = _mv.z;
  if (scale != 0) {
    idx(mVecsShifted, p).f = (float4)(mv, 0, 1) * (1.0f / scale);
  } else {
    idx(mVecsShifted, p).f = (float4)(0);
  }
}

kernel __attribute__((reqd_work_group_size(TILE_X, TILE_Y, 1))) void
RenderFrame(uint width, uint height, global read_only float4 *img_1,
            global read_only float4 *img_2,
            global write_only float4 *diffImg_unused,
            global write_only float4 *mVecs_unused,
            global read_only float4 *mVecsShifted,
            global write_only float4 *rendered, float t) {
  int2 p = (int2)(get_global_id(0), get_global_id(1));
  if (OOB(p)) return;
  // ONCE(printf("RENF\n");)
  float4 _mv = idx(mVecsShifted, p);
  if (_mv.w == 0) {
    // set error pixel as red
    idx(rendered, p) = (float4)(1, 0, 0, 1);
    return;
  }
  int2 mv = (int2)(F2I(_mv.x), F2I(_mv.y));
  int2 mvScaled = (int2)((int)round(t * mv.x), (int)round(t * mv.y));
  int2 mvScaled2 =
      (int2)((int)round((1 - t) * mv.x), (int)round((1 - t) * mv.y));

  int2 inp1 = p - mvScaled;
  int2 inp2 = p + mvScaled2;

  if (OOB(inp1)) {
    idx(rendered, p) = idx(img_2, inp2);
    return;
  } else if (OOB(inp2)) {
    idx(rendered, p) = idx(img_1, inp1);
    return;
  } else {
    idx(rendered, p) = mix(idx(img_1, inp1), idx(img_2, inp2), t);
  }
}

kernel __attribute__((reqd_work_group_size(TILE_X, TILE_Y, 1))) void
RenderFrameBothDirs(uint width, uint height, global read_only float4 *img_1,
                    global read_only float4 *img_2,
                    global read_only float4 *unused,
                    global read_only float4 *mVecsShifted,
                    global read_only float4 *mVecsRevShifted,
                    global write_only float4 *rendered, float t) {
  int2 p = (int2)(get_global_id(0), get_global_id(1));
  if (OOB(p)) return;
  // ONCE(printf("RENF\n");)
  float4 _mvFwd = idx(mVecsShifted, p);
  float4 _mvBwd = idx(mVecsRevShifted, p);
  if (_mvFwd.w == 0 && _mvBwd.w == 0) {
    // set error pixel as blended target pixel
    idx(rendered, p) =
        mix(idx(img_1, p), idx(img_2, p), t);  //(float4)(1, 0, 0, 1);
    return;
  }
  int2 mvFwd = (int2)(F2I(_mvFwd.x), F2I(_mvFwd.y));
  int2 mvBwd = (int2)(F2I(_mvBwd.x), F2I(_mvBwd.y));

  int2 mvFwdScaled = (int2)((int)round(t * mvFwd.x), (int)round(t * mvFwd.y));
  int2 mvFwdScaledBwd =
      (int2)((int)round((1 - t) * mvFwd.x), (int)round((1 - t) * mvFwd.y));

  int2 mvBwdScaledBwd =
      (int2)((int)round(t * mvBwd.x), (int)round(t * mvBwd.y));
  int2 mvBwdScaled =
      (int2)((int)round((1 - t) * mvBwd.x), (int)round((1 - t) * mvBwd.y));

  int2 inpFF = p - mvFwdScaled;
  int2 inpFB = p + mvFwdScaledBwd;
  int2 inpBF = p + mvBwdScaledBwd;
  int2 inpBB = p - mvBwdScaled;

  float4 tgtColor = (float4)0;
  float weight = 0;
#define OPPOSING 1

  if (_mvBwd.w != 0) {
    if (OPPOSING && !OOB(inpBF)) {
      tgtColor += idx(img_1, inpBF) * (1 - t);
      weight += (1 - t);
    }
    if (!OOB(inpBB)) {
      tgtColor += idx(img_2, inpBB) * t;
      weight += t;
    }
  }
  if (_mvFwd.w != 0) {
    if (!OOB(inpFF)) {
      tgtColor += idx(img_1, inpFF) * (1 - t);
      weight += (1 - t);
    }
    if (OPPOSING && !OOB(inpFB)) {
      tgtColor += idx(img_2, inpFB) * t;
      weight += t;
    }
  }
  if (weight == 0) {
    // fallback: blend pixels
    idx(rendered, p) = mix(idx(img_1, p), idx(img_2, p), t);
  } else {
    idx(rendered, p) = tgtColor / weight;
  }

  /*
  if (_mvBwd.w != 0 && !OOB(inpBF) && !OOB(inpBB))
  {
      // backward mvec only
      idx(rendered, p) = mix(idx(img_1, inpBF), idx(img_2, inpBB), t);
  }
  /*else if (!OOB(inpFF) && !OOB(inpFB))
  {
      // forward mvec only
      idx(rendered, p) = mix(idx(img_1, inpFF), idx(img_2, inpFB), t);
  }*/
}