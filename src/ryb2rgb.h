/*
 * Implementation based on
 * https://github.com/bahamas10/ryb/blob/gh-pages/assets/ryb.pdf
 *
 * Author: Dave Eddy <dave@daveeddy.com>
 * Date: December 12, 2020
 * License: MIT
 */

typedef struct RGB {
	float r;
	float g;
	float b;
} RGB;

RGB interpolate2rgb(float a, float b, float c, const float magic[8][3]);
RGB ryb2rgb(float r, float y, float b);
