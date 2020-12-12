/*
 * A single particle (floating orb)
 *
 * Author: Dave Eddy <dave@daveeddy.com>
 * Date: December 12, 2020
 * License: MIT
 */

typedef struct Particle {
	float position;
	float height;
	unsigned int radius;
	unsigned int lineDistance;
	int speed;
	int x;
	int y;
} Particle;

Particle *particleCreate();
void *particleInit(Particle *p, unsigned int radius, unsigned int height,
    int speed, unsigned int lineDistance, float position);
void *particlePrint(Particle *p);
void particleCalculateCoordinates(Particle *p);
void *particleDestroy(Particle *p);
