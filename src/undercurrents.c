/*
 * Undercurrents
 *
 * A fun visualizer using C and OpenGL
 *
 * Author: Dave Eddy <dave@daveeddy.com>
 * Date: December 12, 2020
 * License: MIT
 */

#include <assert.h>
#include <err.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "particle.h"
#include "ryb2rgb.h"

// Configuration

// Window width and height
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 1200

/*
 * Maximum particle circular speed.  Each particle will have a speed between
 * 0 through PARTICLE_SPEED_MAXIMUM inclusive.
 */
#define PARTICLE_SPEED_MAXIMUM 25

/*
 * Particle radius (dot when drawn).  Each particle will have a radius between
 * PARTICLE_RADIUS_MINIMUM through PARTICLE_RADIUS_MAXIMUM inclusive.
 */
#define PARTICLE_RADIUS_MINIMUM 1
#define PARTICLE_RADIUS_MAXIMUM 5

/*
 * Particle height (distance from center mass).  Each particle will have a
 * height between PARTICLE_HEIGHT_MINIMUM through PARTICLE_HEIGHT_MAXIMUM
 * inclusive.  This value will grow by a factor of PARTICLE_EXPAND_RATE over
 * time.
 */
#define PARTICLE_HEIGHT_MINIMUM 0
#define PARTICLE_HEIGHT_MAXIMUM 5

/*
 * Particle line distance.  Each particle has a "distance" that it will use to
 * check against every other particle in its ring/orbit.  Any other particle
 * found within that distance will have a line drawn between them.  Said
 * differently, as particles pass close by each other they will connect with a
 * line while in range.  The range is from PARTICLE_LINE_DISTANCE_MINIMUM
 * through PARTICLE_LINE_DISTANCE_MAXIMUM.
 */
#define PARTICLE_LINE_DISTANCE_MINIMUM 0
#define PARTICLE_LINE_DISTANCE_MAXIMUM 200

/*
 * How quickly the particles expand outward from the center.
 */
#define PARTICLE_EXPAND_RATE 20

/*
 * How long it takes for a particle to become active (in milliseconds).  When a
 * particle is * created, this value will be set randomly between 0 and
 * PARTICLE_BORN_TIMER_MAXIMUM.  This value will be decremented by the amout of
 * milliseconds that have elapsed every iteration of the game loop, and once it
 * hits 0, will result in the particle being displayed.
 *
 * This value makes it so multiple particles can be added at the same time
 * without them all just suddenly popping into existence.
 */
#define PARTICLE_BORN_TIMER_MAXIMUM 1000

/*
 * How quickly the colors cycle.
 */
#define PARTICLE_COLOR_SPEED 50

/*
 * The maximum number of rings to create.  Any new rings will result in the
 * last ring being recycled.
 */
#define RINGS_MAXIMUM 34

/*
 * The alpha value to use (when fading is enabled) when clearing the screen
 * (ALPHA_BACKGROUND) and when drawing the particles or lines (ALPHA_ELEMENTS).
 */
#define ALPHA_BACKGROUND 0.032
#define ALPHA_ELEMENTS 0.10

/*
 * Color modes
 */
enum ColorMode {
	ColorModeSolid,
	ColorModeRinged,
	ColorModeCircular,
	ColorModeIndividual
};

/*
 * A linked-list for particles
 */
typedef struct ParticleNode {
	Particle *particle;
	struct ParticleNode *next;
} ParticleNode;

/*
 * A linked-list of particle nodes
 */
typedef struct RingNode {
	struct ParticleNode *particleNode;
	struct RingNode *next;
} RingNode;

// Linked list of existing rings
RingNode *rings = NULL;

// How many rings are currently active
unsigned int ringCount = 0;

// How many particles are currently allocated
unsigned int particleCount = 0;

// How many particles are currently on the free list
unsigned int recycledParticles = 0;

// The free particle list
ParticleNode *freeParticleNodes = NULL;

/*
 * Generate an RGB color from a given index.
 *
 * adapted from
 * https://community.khronos.org/t/how-does-one-create-animated-rgb-rainbow-effect-on-objects-lines-quads-triangles/76562/13
 */
#define MAX_COLORS (256 * 6)
RGB rainbow(unsigned int idx) {
	assert(idx >= 0);
	assert(idx < MAX_COLORS);
	//idx = MAX_COLORS - idx - 1;

	RGB rgb;
	float a, b, c;

        unsigned int which = idx / 256;
        float t = (float)(idx % 256) / 256.0;

        switch (which) {
        case 0: a =  1; b = t,  c =   0; break; // r->y
        case 1: a =1-t; b = 1,  c =   0; break; // y->g
        case 2: a =  0; b = 1,  c =   t; break; // g->c
        case 3: a =  0; b =1-t, c =   1; break; // c->b
        case 4: a =  t; b = 0,  c =   1; break; // b->m
        case 5: a =  1; b = 0,  c = 1-t; break; // m->r
	default: assert(false);
	}

	rgb.r = a;
	rgb.g = b;
	rgb.b = c;

	return rgb;
}

/*
 * Wrapper for malloc that takes an error message as the second argument and
 * exits on failure.
 */
void *safeMalloc(size_t size, const char *msg) {
	void *ptr = malloc(size);
	if (ptr == NULL) {
		err(2, msg);
	}
	return ptr;
}

/*
 * Generate random values for an existing particle.
 */
void randomizeParticle(Particle *p) {
	int speed = rand() % PARTICLE_SPEED_MAXIMUM;
	unsigned int radius = PARTICLE_RADIUS_MINIMUM +
	    (rand() % (PARTICLE_RADIUS_MAXIMUM - PARTICLE_RADIUS_MINIMUM));
	unsigned int height = PARTICLE_HEIGHT_MINIMUM +
	    (rand() % (PARTICLE_HEIGHT_MAXIMUM - PARTICLE_HEIGHT_MINIMUM));
	unsigned int lineDistance = PARTICLE_LINE_DISTANCE_MINIMUM +
	    (rand() % (PARTICLE_LINE_DISTANCE_MAXIMUM - PARTICLE_LINE_DISTANCE_MINIMUM));
	unsigned int color = rand() % MAX_COLORS;
	int bornTimer = rand() % PARTICLE_BORN_TIMER_MAXIMUM;
	float position = rand() % 360;

	assert(p != NULL);

	// 50% chance the particle moves backwards (negative speed)
	if (rand() % 2) {
		speed *= -1;
	}

	particleInit(p, bornTimer, radius, height, speed, lineDistance,
	    position, color);
}

/*
 * Wrapper for creating a particle.  This has the benefit of failing if the
 * particle fails to be created.
 */
Particle *createParticle() {
	Particle *p = particleCreate();
	if (p == NULL) {
		err(2, "createParticle");
	}
	particleCount++;
	return p;
}

/*
 * Convenience function for getting a ParticleNode with an attached Particle
 * object.  This function will retreive an existing one from the recycled free
 * list, or malloc a new one if one isn't available.
 */
ParticleNode *makeOrReclaimRandomizedParticleNode() {
	ParticleNode *particleNode;
	Particle *p;

	if (freeParticleNodes == NULL) {
		assert(recycledParticles == 0);

		// allocate a new particle and particleNode
		particleNode = safeMalloc(sizeof (ParticleNode),
		    "makeOrReclaimParticleNode malloc ParticleNode");
		p = createParticle();

		particleNode->particle = p;
	} else {
		// reclaim an existing particle
		particleNode = freeParticleNodes;
		freeParticleNodes = freeParticleNodes->next;
		recycledParticles--;
	}

	particleNode->next = NULL;

	p = particleNode->particle;
	randomizeParticle(p);

	return particleNode;
}

/*
 * Adds a new ring to the global rings linked list head.
 */
void addRing() {
	RingNode *ringNode = safeMalloc(sizeof (RingNode), "addRing malloc RingNode");

	ringNode->particleNode = NULL;
	ringNode->next = rings;

	rings = ringNode;
	ringCount++;
}

/*
 * Remove the last ring from the rings linked list tail.
 */
void recycleLastRing() {
	RingNode *ringPtr = rings;

	// fast forward ringPtr to second last item
	while (ringPtr != NULL && ringPtr->next != NULL &&
	    ringPtr->next->next != NULL) {

		ringPtr = ringPtr->next;
	}

	if (ringPtr == NULL) {
		printf("nothing to recycle\n");
		return;
	}

	// grab the last item
	RingNode *last = ringPtr->next;

	// truncate the list
	ringPtr->next = NULL;

	// loop particles in ring and append them to the freed list
	ParticleNode *cur = last->particleNode;
	while (cur != NULL) {
		ParticleNode *next = cur->next;

		cur->next = freeParticleNodes;
		freeParticleNodes = cur;
		recycledParticles++;

		cur = next;
	}

	// free the ring
	free(last);
}

/*
 * Draw a circle at the given x and y coordinates with a radius r.
 *
 * Taken from http://slabode.exofire.net/circle_draw.shtml
 */
void DrawCircle(float cx, float cy, float r) {
	int num_segments = 10 * sqrtf(r);
	float theta = 2 * M_PI / num_segments;
	float c = cosf(theta);//precalculate the sine and cosine
	float s = sinf(theta);
	float t;

	float x = r;//we start at angle = 0
	float y = 0;

	//glBegin(GL_LINE_LOOP);
	glBegin(GL_POLYGON);
	for (int ii = 0; ii < num_segments; ii++) {
		glVertex2f(x + cx, y + cy);//output vertex

		//apply the rotation matrix
		t = x;
		x = c * x - s * y;
		y = s * t + c * y;
	}
	glEnd();
}

/*
 * Draw a particle on the window
 */
void DrawParticle(Particle *particle) {
	float x = (WINDOW_WIDTH / 2) + particle->x;
	float y = (WINDOW_HEIGHT / 2) + particle->y;

	DrawCircle(x, y, particle->radius);
}

/*
 * Draw a line between 2 particles
 */
void DrawLinesConnectingParticles(Particle *p1, Particle *p2) {
	float x1 = (WINDOW_WIDTH / 2) + p1->x;
	float y1 = (WINDOW_HEIGHT / 2) + p1->y;
	float x2 = (WINDOW_WIDTH / 2) + p2->x;
	float y2 = (WINDOW_HEIGHT / 2) + p2->y;

	glBegin(GL_LINES);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glEnd();
}

/*
 * Generate random values for a magic color array (used by ryb2rgb)
 */
void randomizeMagic(float magic[8][3]) {
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 3; j++) {
			magic[i][j] = (float)rand()/(float)RAND_MAX;
		}
	}
}

/*
 * Set the glColor to the given rainbow index
 */
void setColor(unsigned int idx, const float magic[8][3], float alpha) {
	idx = idx % MAX_COLORS;
	RGB rgb = rainbow(idx);
	rgb = interpolate2rgb(rgb.r, rgb.g, rgb.b, magic);
	glColor4f(rgb.r, rgb.g, rgb.b, alpha);
}

/*
 * Print the usage message to the given FILE stream
 */
void usage(FILE *s) {
	fprintf(s, "- press up / down to modify expansion rate\n");
	fprintf(s, "- press left / right to modify max rings\n");
	fprintf(s, "- press 'r' to randomize colors\n");
	fprintf(s, "- press 'f' to toggle fade\n");
	fprintf(s, "- press 'm' to toggle color modes\n");
	fprintf(s, "\n");
	fprintf(s, "WINDOW_WIDTH=%d\n", WINDOW_WIDTH);
	fprintf(s, "WINDOW_HEIGHT=%d\n", WINDOW_HEIGHT);
	fprintf(s, "PARTICLE_SPEED_MAXIMUM=%d\n", PARTICLE_SPEED_MAXIMUM);
	fprintf(s, "PARTICLE_RADIUS_MINIMUM=%d\n", PARTICLE_RADIUS_MINIMUM);
	fprintf(s, "PARTICLE_RADIUS_MAXIMUM=%d\n", PARTICLE_RADIUS_MAXIMUM);
	fprintf(s, "PARTICLE_HEIGHT_MINIMUM=%d\n", PARTICLE_HEIGHT_MINIMUM);
	fprintf(s, "PARTICLE_HEIGHT_MAXIMUM=%d\n", PARTICLE_HEIGHT_MAXIMUM);
	fprintf(s, "PARTICLE_LINE_DISTANCE_MINIMUM=%d\n",
	    PARTICLE_LINE_DISTANCE_MINIMUM);
	fprintf(s, "PARTICLE_LINE_DISTANCE_MAXIMUM=%d\n",
	    PARTICLE_LINE_DISTANCE_MAXIMUM);
	fprintf(s, "PARTICLE_EXPAND_RATE=%d\n", PARTICLE_EXPAND_RATE);
	fprintf(s, "PARTICLE_BORN_TIMER_MAXIMUM=%d\n",
	    PARTICLE_BORN_TIMER_MAXIMUM);
	fprintf(s, "PARTICLE_COLOR_SPEED=%d\n", PARTICLE_COLOR_SPEED);
	fprintf(s, "RINGS_MAXIMUM=%d\n", RINGS_MAXIMUM);
	fprintf(s, "ALPHA_BACKGROUND=%f\n", ALPHA_BACKGROUND);
	fprintf(s, "ALPHA_ELEMENTS=%f\n", ALPHA_ELEMENTS);
}

/*
 * Main method!
 */
int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout);
			return (0);
		default:
			usage(stderr);
			return (1);
		}
	}
	argc -= optind;
	argv += optind;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetSwapInterval(1);

	SDL_Window *Window = SDL_CreateWindow("Undercurrents", 0, 0,
	    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
	assert(Window);
	SDL_GLContext Context = SDL_GL_CreateContext(Window);
	assert(Context);

	glMatrixMode(GL_PROJECTION);
	glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	srand(time(NULL));

	bool running = true;
	float alphaBackground = ALPHA_BACKGROUND;
	float alphaElements = ALPHA_ELEMENTS;
	float rainbowIdx = 0;
	float randomMagic[8][3];
	int currentColorMode = ColorModeSolid;
	int fpsCounter = 0;
	int ringCounter = 0;
	unsigned int currentTime;
	unsigned int delta;
	unsigned int expandRate = PARTICLE_EXPAND_RATE;
	unsigned int lastTime = 0;
	unsigned int maxRings = RINGS_MAXIMUM;

	usage(stdout);
	printf("\n");

	randomizeMagic(randomMagic);

	// main loop
	while (running) {
		// calculate time
		currentTime = SDL_GetTicks();
		delta = currentTime - lastTime;
		lastTime = currentTime;
		RingNode *ringPtr;

		// process events
		SDL_Event Event;
		while (SDL_PollEvent(&Event)) {
			switch (Event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN:
				switch (Event.key.keysym.sym) {
				case SDLK_ESCAPE:
					running = false;
					break;
				case SDLK_UP:
					expandRate++;
					printf("expandRate=%u\n", expandRate);
					break;
				case SDLK_DOWN:
					if (expandRate > 0) {
						expandRate--;
					}
					printf("expandRate=%u\n", expandRate);
					break;
				case SDLK_LEFT:
					if (maxRings > 0) {
						maxRings--;
					}
					printf("maxRings=%u\n", maxRings);
					break;
				case SDLK_RIGHT:
					maxRings++;
					printf("maxRings=%u\n", maxRings);
					break;
				case SDLK_f:
					if (alphaElements == 1.0) {
						alphaElements = ALPHA_ELEMENTS;
						alphaBackground = ALPHA_BACKGROUND;
						printf("fading enabled\n");
					} else {
						alphaElements = 1.0;
						alphaBackground = 1.0;
						printf("fading disabled\n");
					}
					break;
				case SDLK_r:
					randomizeMagic(randomMagic);
					printf("randomized colors\n");
					break;
				case SDLK_m:
					currentColorMode = (currentColorMode + 1) % 4;
					printf("currentColorMode = %d\n", currentColorMode);
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		// process timing stats
		fpsCounter -= delta;
		if (fpsCounter <= 0) {
			while (fpsCounter <= 0) { fpsCounter += 2000; }

			printf("fps=%f ringCount=%u particleCount=%u "
			    "recycledParticles=%u\n",
			    1000.0 / delta, ringCount, particleCount,
			    recycledParticles);
		}



		ringCounter -= delta;
		if (ringCounter <= 0) {
			while (ringCounter <= 0) { ringCounter += 1000; }

			// add a new ring
			addRing();

			// recycle out-of-view rings
			while (ringCount > maxRings) {
				recycleLastRing();
				ringCount--;
			}

			// add a particle to each existing ring
			ringPtr = rings;
			for (int i = 0; ringPtr != NULL; ringPtr = ringPtr->next, i++) {
				int num = (i / 4) + 2;

				for (int j = 0; j < num; j++) {
					ParticleNode *head = ringPtr->particleNode;
					ParticleNode *new = makeOrReclaimRandomizedParticleNode();

					if (head != NULL) {
						assert(head->particle);
						// TODO fix this height bs
						int variance = PARTICLE_HEIGHT_MAXIMUM - PARTICLE_HEIGHT_MINIMUM;
						new->particle->height = head->particle->height + (rand() % variance);
					}

					new->next = head;
					ringPtr->particleNode = new;
				}
			}
		}

		// Update rainbow index
		rainbowIdx += (float)delta / 1000.0 * PARTICLE_COLOR_SPEED;
		while (rainbowIdx > MAX_COLORS) { rainbowIdx -= MAX_COLORS; }
		while (rainbowIdx <= 0) { rainbowIdx += MAX_COLORS; }

		// calculate new particle locations
		ringPtr = rings;
		for (; ringPtr != NULL; ringPtr = ringPtr->next) {
			ParticleNode *particlePtr = ringPtr->particleNode;

			// loop particles in ring
			for (; particlePtr != NULL; particlePtr = particlePtr->next) {
				Particle *p = particlePtr->particle;

				// update particle location
				p->height += (float)delta * (float)expandRate / 1000.0;
				p->position += (float)p->speed / (float)delta;
				particleCalculateCoordinates(p);

				// reduce bornTimer by delta
				if (p->bornTimer != 0) {
					p->bornTimer -= delta;
					if (p->bornTimer < 0) {
						p->bornTimer = 0;
					}
				}
			}

		}

		// clear screen
		glColor4f(0.0f, 0.0f, 0.0f, alphaBackground);
		glRecti(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		if (currentColorMode == ColorModeSolid) {
			setColor(rainbowIdx, randomMagic, alphaElements);
		}

		// draw the particles and lines, start by looping rings
		ringPtr = rings;
		for (int i = 0; ringPtr != NULL; ringPtr = ringPtr->next, i++) {
			ParticleNode *particlePtr = ringPtr->particleNode;

			// set color here if ringed mode
			if (currentColorMode == ColorModeRinged) {
				unsigned int idx = rainbowIdx + (i * MAX_COLORS / RINGS_MAXIMUM);
				setColor(idx, randomMagic, alphaElements);
			}

			// loop particles in ring
			for (; particlePtr != NULL; particlePtr = particlePtr->next) {
				Particle *p = particlePtr->particle;

				// check if particle is born
				if (p->bornTimer > 0) {
					continue;
				}

				// set color here if circular mode or random mode
				if (currentColorMode == ColorModeCircular) {
					unsigned int idx = (unsigned int)(p->position / 360.0 * (float)MAX_COLORS);
					idx = (idx + (int)rainbowIdx) % MAX_COLORS;
					setColor(idx, randomMagic, alphaElements);
				} else if (currentColorMode == ColorModeIndividual) {
					setColor(p->color + rainbowIdx, randomMagic, alphaElements);
				}

				// draw the particle
				DrawParticle(p);

				// draw connected lines to any particles NEXT
				// in the ring/orbit
				ParticleNode *particlePtr2 = particlePtr->next;;
				for (; particlePtr2 != NULL; particlePtr2 = particlePtr2->next) {
					Particle *p2 = particlePtr2->particle;

					if (p2->bornTimer > 0) {
						continue;
					}

					float yd = (WINDOW_HEIGHT / 2 + p2->y) - (WINDOW_HEIGHT / 2 + p->y);
					float xd = (WINDOW_WIDTH / 2 + p2->x) - (WINDOW_HEIGHT / 2 + p->x);

					// distance between 2 particles
					float d = sqrt((xd * xd) + (yd * yd));

					// draw a line between the particles
					if (d < p->lineDistance) {
						DrawLinesConnectingParticles(p, p2);
					}
				}
			}
		}

		// swap windows
		SDL_GL_SwapWindow(Window);
		SDL_Delay(1);
	}

	return 0;
}
