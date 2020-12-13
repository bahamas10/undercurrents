/*
 * A single particle (floating orb)
 *
 * Author: Dave Eddy <dave@daveeddy.com>
 * Date: December 12, 2020
 * License: MIT
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "particle.h"

/*
 * Create an empty Particle object
 *
 * Must be freed by the caller with particleDestroy()
 */
Particle *particleCreate() {
	Particle *p = malloc(sizeof (Particle));
	return p;
}

/*
 * Initialize Particle
 */
void *particleInit(Particle *p, int bornTimer, unsigned int radius, unsigned
    int height, int speed, unsigned int lineDistance, float position, unsigned
    int color) {

	assert(p != NULL);

	p->bornTimer = bornTimer;
	p->height = height;
	p->radius = radius;
	p->speed = speed;
	p->lineDistance = lineDistance;
	p->position = position;
	p->color = color;

	particleCalculateCoordinates(p);
}

/*
 * Print a particle object
 */
void *particlePrint(Particle *p) {
	printf("<Particle bornTimer=%d position=%f height=%f x=%f y=%f \
	    radius=%u lineDistance=%u speed=%d color=%u>\n",
	    p->bornTimer, p->position, p->height, p->x, p->y, p->radius,
	    p->lineDistance, p->speed, p->color);
}

/*
 * recalculate the x and y positions of a particle
 */
void particleCalculateCoordinates(Particle *p) {
	while (p->position >= 360.0) { p->position -= 360.0; }
	while (p->position < 0) { p->position += 360.0; }

	// shift degrees over 270 degrees (from 3 o'clock to 12 o'clock)
	float degrees = p->position + 270.0f;
	float radians = degrees * M_PI / 180.0;

	p->x = p->height * cos(radians);
	p->y = p->height * sin(radians);
}

/*
 * Free a particle object
 */
void *particleDestroy(Particle *p) {
	free(p);
}
