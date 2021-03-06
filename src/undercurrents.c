/*
 * Undercurrents
 *
 * A fun visualizer using C and OpenGL.
 *
 * This code borrows spiritually from the algorithm used in the 2nd inspiration
 * link belew.  It works by spawning "rings" of "particles", where each
 * particle can only interact with particles in its own ring (usually by
 * drawing lines between them if within a certain range).  Each "ring" can be
 * thought of like a ring or orbit around a planet, where each particle is
 * debris within that specific ring.  The rings spawn in the middle of the
 * screen, and slowly eminate outwards while new rings are added on a given
 * interval to the center.  When a maximum number of rings are generated they
 * will be removed from the end of the list of rings.  All of the ranges and
 * maximum valus can be configured below or with CLI options at runtime.
 *
 * To implement this, there are 3 types (structs) to consider:
 *
 * - Particle
 * - ParticleNode
 * - RingNode
 *
 * Particle represents a single particle (see particle.h for more information)
 *
 * ParticleNode can create a linked-list of Particles
 *
 * RingNode can create a linked-list of ParticleNodes
 *
 * Put simply: there are buckets of "rings", each with their own collection of
 * particles in them - all implemented via linked-lists.
 *
 * Inspired from:
 * - https://www.renderforest.com/template/melodic-vibes-visualizer
 * - https://pcvector.net/codepen/760-sverkajuschie-krugi-iz-chastic.html
 *
 * Author: Dave Eddy <dave@daveeddy.com>
 * Date: December 12, 2020
 * License: MIT
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef __APPLE__
#include <SDL.h>
#include <SDL_opengl.h>
#else
#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#endif

#include "particle.h"
#include "ryb2rgb.h"

// Configuration

// Window width and height
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 1200

/*
 * Maximum particle circular speed.  Each particle will have a speed between
 * 0 through PARTICLE_SPEED_MAXIMUM inclusive.
 *
 * PARTICLE_SPEED_FACTOR is the rate (percentage, 100 by default) to multiply
 * the speed by.  This variable is most useful to be modified while the program
 * is running - which is why this can be modified with the arrow keys.
 */
#define PARTICLE_SPEED_MAXIMUM 30
#define PARTICLE_SPEED_FACTOR 100

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
 *
 * PARTICLE_LINE_DISTANCE_FACTOR operates the same way as
 * PARTICLE_SPEED_FACTOR: it allows the line distances to be modified in
 * realtime.  This value can be thought of as a percentage and defaults to 100
 * (normal).
 *
 * PARTICLE_LINE_RING_DISABLE is a bit of an odd (and custom) variable.
 * Setting this a positive number will result in all particle in that ring (and
 * beyond) to have their lines disabled.  This is a way of making it so the
 * outermost rings will not have lines drawn in them.  Setting this to -1 will
 * result in *all* rings/orbits having particle lines enabled.
 */
#define PARTICLE_LINE_DISTANCE_MINIMUM 0
#define PARTICLE_LINE_DISTANCE_MAXIMUM 200
#define PARTICLE_LINE_DISTANCE_FACTOR 100
#define PARTICLE_LINE_RING_DISABLE -1

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
#define RINGS_MAXIMUM 35

/*
 * The alpha value to use (when fading is enabled) when clearing the screen
 * (ALPHA_BACKGROUND) and when drawing the particles or lines (ALPHA_ELEMENTS).
 * This number should be between 0 (fully transparent) and 100 (fully opaque).
 */
#define ALPHA_BACKGROUND 7
#define ALPHA_ELEMENTS 25

/*
 * Time (in milliseconds) to do certain tasks
 *
 * TIMER_PRINT_STATUS_LINE - how often to print the status line, 0 to disable.
 * TIMER_ADD_NEW_RING - how often to add a new ring / orbit.
 *
 */
#define TIMER_PRINT_STATUS_LINE 2000
#define TIMER_ADD_NEW_RING 1000

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

// If fading mode is enabled or disabled
bool fadingMode = true;

// If blank mode is enabled or disabled
bool blankMode = false;

// If lines should be drawn
bool linesEnabled = true;

// If the program is running
bool running = false;

// If the animation is paused
bool paused = false;

// Magic colors (for use with ryb2rgb) randomized
float randomMagic[8][3];

// Current color mode
int currentColorMode = ColorModeSolid;

/*
 * All of the #defines above made available as global variables that can be
 * modified at runtime with CLI options.
 */
int windowWidth = WINDOW_WIDTH;
int windowHeight = WINDOW_HEIGHT;
int particleSpeedMaximum = PARTICLE_SPEED_MAXIMUM;
int particleSpeedFactor = PARTICLE_SPEED_FACTOR;
int particleRadiusMinimum = PARTICLE_RADIUS_MINIMUM;
int particleRadiusMaximum = PARTICLE_RADIUS_MAXIMUM;
int particleHeightMinimum = PARTICLE_HEIGHT_MINIMUM;
int particleHeightMaximum = PARTICLE_HEIGHT_MAXIMUM;
int particleLineDistanceMinimum = PARTICLE_LINE_DISTANCE_MINIMUM;
int particleLineDistanceMaximum = PARTICLE_LINE_DISTANCE_MAXIMUM;
int particleLineDistanceFactor = PARTICLE_LINE_DISTANCE_FACTOR;
int particleLineRingDisable = PARTICLE_LINE_RING_DISABLE;
int particleExpandRate = PARTICLE_EXPAND_RATE;
int particleBornTimerMaximum = PARTICLE_BORN_TIMER_MAXIMUM;
int particleColorSpeed = PARTICLE_COLOR_SPEED;
int ringsMaximum = RINGS_MAXIMUM;
int alphaBackground = ALPHA_BACKGROUND;
int alphaElements = ALPHA_ELEMENTS;
int timerPrintStatusLine = TIMER_PRINT_STATUS_LINE;
int timerAddNewRing = TIMER_ADD_NEW_RING;

/*
 * All of the above configuration options.  Adding an option here will make it
 * show up automatically in `-h` and also be accepted as a '--' long option.
 */
struct ConfigurationParameter {
	char *name;
	int *value;
};

struct ConfigurationParameter config[] = {
	{ "windowWidth", &windowWidth },
	{ "windowHeight", &windowHeight },
	{ "particleSpeedMaximum", &particleSpeedMaximum },
	{ "particleSpeedFactor", &particleSpeedFactor },
	{ "particleRadiusMinimum", &particleRadiusMinimum },
	{ "particleRadiusMaximum", &particleRadiusMaximum },
	{ "particleHeightMinimum", &particleHeightMinimum },
	{ "particleHeightMaximum", &particleHeightMaximum },
	{ "particleLineDistanceMinimum", &particleLineDistanceMinimum },
	{ "particleLineDistanceMaximum", &particleLineDistanceMaximum },
	{ "particleLineRingDisable", &particleLineRingDisable },
	{ "particleExpandRate", &particleExpandRate },
	{ "particleBornTimerMaximum", &particleBornTimerMaximum },
	{ "particleColorSpeed", &particleColorSpeed },
	{ "ringsMaximum", &ringsMaximum },
	{ "alphaBackground", &alphaBackground },
	{ "alphaElements", &alphaElements },
	{ "timerPrintStatusLine", &timerPrintStatusLine },
	{ "timerAddNewRing", &timerAddNewRing },
	{ NULL, NULL }
};

/*
 * Convert a color mode enum to a string
 */
char *colorModeToString(enum ColorMode mode) {
	char *s = NULL;
	switch (mode) {
	case ColorModeSolid:      return "ColorModeSolid";
	case ColorModeRinged:     return "ColorModeRinged";
	case ColorModeCircular:   return "ColorModeCircular";
	case ColorModeIndividual: return "ColorModeIndividual";
	default: assert(false);
	}
	return s;
}

/*
 * Generate an RGB color from a given index.
 *
 * adapted from
 * https://community.khronos.org/t/a/76562/14
 */
#define MAX_COLORS (256 * 6)
RGB rainbow(unsigned int idx) {
	assert(idx >= 0);
	assert(idx < MAX_COLORS);

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
		err(2, "malloc %s", msg);
	}
	return ptr;
}

/*
 * Generate random values for an existing particle.
 */
void randomizeParticle(Particle *p) {
	int speed = rand() % particleSpeedMaximum;
	unsigned int radius = particleRadiusMinimum +
	    (rand() % (particleRadiusMaximum - particleRadiusMinimum));
	unsigned int height = particleHeightMinimum +
	    (rand() % (particleHeightMaximum - particleHeightMinimum));
	unsigned int lineDistance = particleLineDistanceMinimum +
	    (rand() % (particleLineDistanceMaximum - particleLineDistanceMinimum));
	unsigned int color = rand() % MAX_COLORS;
	int bornTimer = rand() % particleBornTimerMaximum;
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
	RingNode *ringNode = safeMalloc(sizeof (RingNode),
	    "addRing malloc RingNode");

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

	if (last == NULL) {
		return;
	}

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
	float x = (windowWidth / 2) + particle->x;
	float y = (windowHeight / 2) + particle->y;

	DrawCircle(x, y, particle->radius);
}

/*
 * Draw a line between 2 particles
 */
void DrawLinesConnectingParticles(Particle *p1, Particle *p2) {
	float x1 = (windowWidth / 2) + p1->x;
	float y1 = (windowHeight / 2) + p1->y;
	float x2 = (windowWidth / 2) + p2->x;
	float y2 = (windowHeight / 2) + p2->y;

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
void setColor(unsigned int idx, const float magic[8][3], int alpha) {
	idx = idx % MAX_COLORS;
	RGB rgb = rainbow(idx);
	rgb = interpolate2rgb(rgb.r, rgb.g, rgb.b, magic);
	float alphaF = fadingMode ? ((float)alpha / 100.0) : 1.00;;

	glColor4f(rgb.r, rgb.g, rgb.b, alphaF);
}

/*
 * Set/reset the screen (should be called on creation or resize).
 */
void resetWindow(SDL_Window *window) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
	glViewport(0, 0, windowWidth, windowHeight);
	glEnable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	// clear both buffers initially
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SetSwapInterval(0);
	SDL_GL_SwapWindow(window);
	SDL_GL_SetSwapInterval(1);
	glClear(GL_COLOR_BUFFER_BIT);
}

/*
 * Print the current configuration settings to the given FILE stream
 */
void printConfiguration(FILE *s) {
	fprintf(s, "Configuration\n");
	struct ConfigurationParameter *ptr = config;
	while (ptr->name != NULL) {
		assert(ptr->value != NULL);
		fprintf(s, "  %s=%d\n", ptr->name, *ptr->value);
		ptr++;
	}
}

/*
 * Prin the controls to the given FILE stream
 */
void printControls(FILE *s) {
	fprintf(s, "Controls\n");
	fprintf(s, "- press up / down to modify particle speed\n");
	fprintf(s, "- press left / right to modify particle line distance factor\n");
	fprintf(s, "- press 'b' to toggle blank mode\n");
	fprintf(s, "- press 'f' to toggle fading mode\n");
	fprintf(s, "- press 'l' to toggle particle lines mode\n");
	fprintf(s, "- press 'm' to toggle color modes\n");
	fprintf(s, "- press 'r' to randomize colors\n");
	fprintf(s, "- press 'p' to pause or unpause visuals\n");
}

/*
 * Print the usage message to the given FILE stream
 */
void printUsage(FILE *s) {
	fprintf(s, "Usage: undercurrents [-h] [--longOpt var]\n");
	fprintf(s, "\n");
	fprintf(s, "Options\n");
	fprintf(s, "    -h, --help                      "
	    "print this message and exit\n");
	fprintf(s, "    -p, --paused                    "
	    "start in the 'paused' state\n");
	fprintf(s, "    --configVariableName value      "
	    "set a configuration variable, see below\n");
	fprintf(s, "\n");
	fprintf(s, "  configuration variables can be passed as long-opts\n");
	fprintf(s, "    ie: undcurrents --windowHeight 500 --windowWidth 700 "
	    "--ringsMaximum 20\n");
	fprintf(s, "\n");
	printConfiguration(s);
	fprintf(s, "\n");
	printControls(s);
}

/*
 * Parse arguments
 *
 * Arguments are valid as both short opts (single '-') and long opts
 * (double '--').
 *
 * Double options will match 1-to-1 with the global configuration
 * variables.  For example:
 *
 *   --windowWidth 600 --particleExpandRate 20
 *
 * Will set windowWidth=600 and particleExpandRate=20 as opposed to
 * using the #define'd values.
 */
void parseArguments(char **argv) {
	argv++;

	while (*argv != NULL) {
		char *arg = *argv;

		if (strncmp(arg, "--", 2) == 0) {
			// long-options
			arg += 2;
			if (strcmp(arg, "help") == 0) {
				printUsage(stdout);
				exit(0);
			} else if (strcmp(arg, "paused") == 0) {
				paused = true;
				goto loop;
			}

			/*
			 * all global options can be modified with
			 * long-options.  because they are all "int" type that
			 * conversion is done here.
			 */
			char *next = *(argv + 1);
			char *end = NULL;
			errno = 0;
			int num;
			if (next != NULL) {
				num = strtol(next, &end, 10);
			}

			// ensure the int parsed
			if (next == NULL || next == end || errno != 0 || num < 0) {
				fprintf(stderr, "failed to parse '%s'\n", next);
				goto error;
			}

			// loop over all config options as long opts
			struct ConfigurationParameter *ptr = config;
			while (ptr->name != NULL) {
				assert(ptr->value != NULL);
				if (strcmp(arg, ptr->name) == 0) {
					*(ptr->value) = num;
					break;
				}
				ptr++;
			}

			// check if the option didn't match
			if (ptr->name == NULL) {
				goto error;
			}

			argv++;
		} else if (strncmp(arg, "-", 1) == 0) {
			// short options
			arg++;
			switch (arg[0]) {
			case 'h':
				printUsage(stdout);
				exit(0);
			case 'p':
				paused = true;
				break;
			default:
				goto error;
			}
		} else {
			// invalid option
			goto error;
		}

loop:
		argv++;
	}

	return;
error:
	fprintf(stderr, "invalid argument: '%s'\n\n", *argv);
	printUsage(stderr);
	exit(1);
}

/*
 * Process SDL and keyboard events
 */
void processEvents() {
	SDL_Event Event;
	SDL_Window *window;
	int i;
	while (SDL_PollEvent(&Event)) {
		switch (Event.type) {
		case SDL_QUIT:
			running = false;
			break;
		case SDL_WINDOWEVENT:
			switch (Event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				windowWidth = Event.window.data1;
				windowHeight = Event.window.data2;
				window = SDL_GetWindowFromID(Event.window.windowID);
				resetWindow(window);
				printf("window size changed to %dx%d\n",
				    windowWidth, windowHeight);
				break;
			}
			break;
		case SDL_KEYDOWN:
			switch (Event.key.keysym.sym) {
			case SDLK_ESCAPE:
				running = false;
				break;
			case SDLK_UP:
				particleSpeedFactor++;
				printf("particleSpeedFactor=%d\n",
				    particleSpeedFactor);
				break;
			case SDLK_DOWN:
				if (particleSpeedFactor > 0) {
					particleSpeedFactor--;
				}
				printf("particleSpeedFactor=%d\n",
				    particleSpeedFactor);
				break;
			case SDLK_LEFT:
				if (particleLineDistanceFactor > 0) {
					particleLineDistanceFactor--;
				}
				printf("particleLineDistanceFactor=%d\n",
				    particleLineDistanceFactor);
				break;
			case SDLK_RIGHT:
				particleLineDistanceFactor++;
				printf("particleLineDistanceFactor=%d\n",
				    particleLineDistanceFactor);
				break;
			case SDLK_b:
				// b = blank
				blankMode = !blankMode;
				printf("blank %s\n",
				    blankMode ? "enabled" : "disabled");
				break;
			case SDLK_c:
				// c = clear
				i = 0;
				while (ringCount > 0) {
					recycleLastRing();
					ringCount--;
					i++;
				}
				printf("cleared %d rings\n", i);
				break;
			case SDLK_f:
				// f = fading
				fadingMode = !fadingMode;
				printf("fading %s\n",
				    fadingMode ? "enabled" : "disabled");
				break;
			case SDLK_l:
				// l = lines
				linesEnabled = !linesEnabled;
				printf("lines %s\n",
				    linesEnabled ? "enabled" : "disabled");
				break;
			case SDLK_m:
				// m = color mode
				currentColorMode = (currentColorMode + 1) % 4;
				printf("currentColorMode = %s\n",
				    colorModeToString(currentColorMode));
				break;
			case SDLK_p:
				// p = play/pause
				paused = !paused;
				printf("%s\n",
				    paused ? "paused" : "unpaused");
				break;
			case SDLK_r:
				// r = randomize colors
				randomizeMagic(randomMagic);
				printf("randomized colors\n");
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
}

/*
 * Main method!
 */
int main(int argc, char **argv) {
	float rainbowIdx = 0;
	int addNewRingCounter = 0;
	int printStatusLineCounter = 0;
	unsigned int lastTime = 0;

	// parse CLI options
	parseArguments(argv);

	// initalize SDL and OpenGL window
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_Window *window = SDL_CreateWindow("Undercurrents", 0, 0,
	    windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	assert(window != NULL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	assert(context != NULL);

	// initialize the screen/viewport/background color
	resetWindow(window);

	// initialize random
	srand(time(NULL));

	// initialize random colors
	randomizeMagic(randomMagic);

	// print config and controls
	printConfiguration(stdout);
	printf("\n");
	printControls(stdout);
	printf("\n");

	// main loop
	running = true;
	while (running) {
		RingNode *ringPtr;
		unsigned int currentTime;
		unsigned int delta;

		// calculate time since last iteration
		currentTime = SDL_GetTicks();
		delta = currentTime - lastTime;
		lastTime = currentTime;

		// process events
		processEvents();

		// check if status line should be printed
		printStatusLineCounter -= delta;
		if (printStatusLineCounter <= 0) {
			printStatusLineCounter += timerPrintStatusLine;

			printf("fps=%f ringCount=%u particleCount=%u "
			    "recycledParticles=%u\n",
			    1000.0 / delta, ringCount, particleCount,
			    recycledParticles);

			int i = 0;
			while (printStatusLineCounter <= 0) {
				i++;
				printStatusLineCounter += timerPrintStatusLine;
			}
			if (i > 0) {
				fprintf(stderr,
				    "[warn] missed %d status line calls\n", i);
			}
		}

		// just end here if we are paused
		if (paused) {
			goto swap;
		}

		// clear screen
		float alpha = fadingMode ? ((float)alphaBackground / 100.0) : 1.0;
		glColor4f(0.0f, 0.0f, 0.0f, alpha);
		glRecti(0, 0, windowWidth, windowHeight);

		// check if new ring (and particles) should be created
		addNewRingCounter -= delta;
		if (addNewRingCounter <= 0) {
			addNewRingCounter += timerAddNewRing;

			// add a new ring
			addRing();

			// recycle out-of-view rings
			while (ringCount > ringsMaximum) {
				recycleLastRing();
				ringCount--;
			}

			// add particle(s) to each existing ring
			ringPtr = rings;
			for (int i = 0; ringPtr != NULL; ringPtr = ringPtr->next, i++) {
				/*
				 * Calculate how many particles to add
				 *
				 * I'd like to make this function somehow more
				 * configurable.  The basic idea is that 'i' is
				 * the number of the current ring being
				 * processed, where 0 is always the innermost
				 * ring and the number increments as we loop
				 * towards the more outside rings.
				 */
				int num = (i / 4) + 4;

				for (int j = 0; j < num; j++) {
					ParticleNode *head = ringPtr->particleNode;
					ParticleNode *new = makeOrReclaimRandomizedParticleNode();

					if (head != NULL) {
						assert(head->particle);
						/*
						 * We use one of the particles
						 * existing height as an offset
						 * for the newly calculated
						 * particle.  This is a little
						 * sus but it works.
						 */
						new->particle->height += head->particle->height;
					}

					new->next = head;
					ringPtr->particleNode = new;
				}
			}

			int i = 0;
			while (addNewRingCounter <= 0) {
				i++;
				addNewRingCounter += timerAddNewRing;
			}
			if (i > 0) {
				fprintf(stderr, "[warn] missed %d add ring calls\n", i);
			}
		}

		// Update rainbow index
		rainbowIdx += (float)delta / 1000.0 * particleColorSpeed;
		while (rainbowIdx > MAX_COLORS) { rainbowIdx -= MAX_COLORS; }
		while (rainbowIdx <= 0) { rainbowIdx += MAX_COLORS; }

		// calculate new particle locations
		ringPtr = rings;
		for (; ringPtr != NULL; ringPtr = ringPtr->next) {
			ParticleNode *particlePtr = ringPtr->particleNode;

			// loop particles in ring
			for (; particlePtr != NULL; particlePtr = particlePtr->next) {
				Particle *p = particlePtr->particle;

				float speedRate = particleSpeedFactor / 100.0;

				// update particle location
				p->height += (float)delta * (float)particleExpandRate / 1000.0;
				p->position += (float)delta * ((float)p->speed / p->height / 5.0 * speedRate);
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

		// just finish if blank mode is set
		if (blankMode) {
			goto swap;
		}

		// set the color here just once if in solid mode
		if (currentColorMode == ColorModeSolid) {
			setColor(rainbowIdx, randomMagic, alphaElements);
		}

		// draw the particles and lines, start by looping rings
		ringPtr = rings;
		for (int i = 0; ringPtr != NULL; ringPtr = ringPtr->next, i++) {
			ParticleNode *particlePtr = ringPtr->particleNode;

			// set color here if ringed mode
			if (currentColorMode == ColorModeRinged) {
				unsigned int idx = rainbowIdx + (i * MAX_COLORS / ringsMaximum);
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

				// stop here if lines aren't enabled
				if (!linesEnabled) {
					continue;
				}

				// check if this ring has lines disabled
				if (particleLineRingDisable != -1 && i > particleLineRingDisable) {
					continue;
				}

				// draw connected lines to any particles NEXT
				// in the ring/orbit
				ParticleNode *particlePtr2 = particlePtr->next;
				for (; particlePtr2 != NULL; particlePtr2 = particlePtr2->next) {
					Particle *p2 = particlePtr2->particle;

					if (p2->bornTimer > 0) {
						continue;
					}

					float yd = p2->y -p->y;
					float xd = p2->x -p->x;

					// distance between 2 particles
					float d = sqrt((xd * xd) + (yd * yd));

					float maxDistance = (float)p->lineDistance * (particleLineDistanceFactor / 100.0);

					// draw a line between the particles
					if (d < maxDistance) {
						DrawLinesConnectingParticles(p, p2);
					}
				}
			}
		}

swap:
		// swap windows
		SDL_GL_SwapWindow(window);
		SDL_Delay(1);
	}

	return 0;
}
