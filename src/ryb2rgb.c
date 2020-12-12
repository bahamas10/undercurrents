/*
 * Implementation based on
 * https://github.com/bahamas10/ryb/blob/gh-pages/assets/ryb.pdf
 *
 * Author: Dave Eddy <dave@daveeddy.com>
 * Date: December 12, 2020
 * License: MIT
 */

#include <math.h>

#include "ryb2rgb.h"

static const float RYB_MAGIC_COLORS[8][3] = {
        {1,     1,     1},
        {1,     1,     0},
        {1,     0,     0},
        {1,     0.5,   0},
        {0.163, 0.373, 0.6},
        {0.0,   0.66,  0.2},
        {0.5,   0.0,   0.5},
        {0.2,   0.094, 0.0}
};

static float cubicInt(float t, float A, float B) {
	float weight = t * t * (3 - 2 * t);
	return A + weight * (B - A);
}

RGB interpolate2rgb(float a, float b, float c, const float magic[8][3]) {
	RGB rgb;
	float x0, x1, x2, x3;
	float y0, y1;

	// red
	x0 = cubicInt(c, magic[0][0], magic[4][0]);
	x1 = cubicInt(c, magic[1][0], magic[5][0]);
	x2 = cubicInt(c, magic[2][0], magic[6][0]);
	x3 = cubicInt(c, magic[3][0], magic[7][0]);
	y0 = cubicInt(b, x0, x1);
	y1 = cubicInt(b, x2, x3);
	rgb.r = cubicInt(a, y0, y1);

	// green
	x0 = cubicInt(c, magic[0][1], magic[4][1]);
	x1 = cubicInt(c, magic[1][1], magic[5][1]);
	x2 = cubicInt(c, magic[2][1], magic[6][1]);
	x3 = cubicInt(c, magic[3][1], magic[7][1]);
	y0 = cubicInt(b, x0, x1);
	y1 = cubicInt(b, x2, x3);
	rgb.g = cubicInt(a, y0, y1);

	// blue
	x0 = cubicInt(c, magic[0][2], magic[4][2]);
	x1 = cubicInt(c, magic[1][2], magic[5][2]);
	x2 = cubicInt(c, magic[2][2], magic[6][2]);
	x3 = cubicInt(c, magic[3][2], magic[7][2]);
	y0 = cubicInt(b, x0, x1);
	y1 = cubicInt(b, x2, x3);
	rgb.b = cubicInt(a, y0, y1);

	return rgb;
}

RGB ryb2rgb(float r, float y, float b) {
	return interpolate2rgb(r, y, b, RYB_MAGIC_COLORS);
}
