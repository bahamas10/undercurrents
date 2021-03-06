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
	unsigned int color;
	int speed;
	int bornTimer;
	int x;
	int y;
} Particle;

Particle *particleCreate();
void particleInit(Particle *p, int bornTimer, unsigned int radius, unsigned
    int height, int speed, unsigned int lineDistance, float position, unsigned
    int color);
void particlePrint(Particle *p);
void particleCalculateCoordinates(Particle *p);
void particleDestroy(Particle *p);
