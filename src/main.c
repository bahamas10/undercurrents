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

struct GameState {
	// Linked list of existing rings
	RingNode *rings;

	// How many rings are (currently active)
	unsigned int ringCount;

	// How many particles are currently allocated
	unsigned int particleCount;

	// How many particles are currently on the free list
	unsigned int recycledParticles;

	// The free particle list
	ParticleNode *freeParticleNodes;

	// If fading mode is enabled or disabled
	bool fadingMode;

	// If blank mode is enabled or disabled
	bool blankMode;

	// If lines should be drawn
	bool linesEnabled;

	// If the program is running
	bool running;

	// If the animation is paused
	bool paused;

	// Magic colors (for use with ryb2rgb) randomized
	float randomMagic[8][3];

	// Current color mode
	int colorMode;

	// SDL windows and render objects
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *canvas;
};

static struct GameState gState = {
	.rings = NULL,
	.ringCount = 0,
	.particleCount = 0,
	.recycledParticles = 0,
	.freeParticleNodes = NULL,
	.fadingMode = true,
	.blankMode = false,
	.linesEnabled = true,
	.running = false,
	.paused = false,
	.colorMode = ColorModeSolid,
	.window = NULL,
	.renderer = NULL,
	.canvas = NULL,
};

// SDL windows and render objects
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *canvas = NULL;

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

// Handle window resize
static void handleResize(int newW, int newH) {
	windowWidth = newW;
	windowHeight = newH;

	SDL_RenderSetLogicalSize(renderer, windowWidth, windowHeight);
	SDL_DestroyTexture(canvas);
	canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
	    SDL_TEXTUREACCESS_TARGET, windowWidth, windowHeight);

	printf("window size changed to %dx%d\n", windowWidth, windowHeight);
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
	gState.particleCount++;
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

	if (gState.freeParticleNodes == NULL) {
		assert(gState.recycledParticles == 0);

		// allocate a new particle and particleNode
		particleNode = safeMalloc(sizeof (ParticleNode),
		    "makeOrReclaimParticleNode malloc ParticleNode");
		p = createParticle();

		particleNode->particle = p;
	} else {
		// reclaim an existing particle
		particleNode = gState.freeParticleNodes;
		gState.freeParticleNodes = gState.freeParticleNodes->next;
		gState.recycledParticles--;
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
	ringNode->next = gState.rings;

	gState.rings = ringNode;
	gState.ringCount++;
}

/*
 * Remove the last ring from the rings linked list tail.
 */
void recycleLastRing() {
	RingNode *ringPtr = gState.rings;

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

		cur->next = gState.freeParticleNodes;
		gState.freeParticleNodes = cur;
		gState.recycledParticles++;

		cur = next;
	}

	// free the ring
	free(last);
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
				gState.paused = true;
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
				gState.paused = true;
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
	int i;
	while (SDL_PollEvent(&Event)) {
		switch (Event.type) {
		case SDL_QUIT:
			gState.running = false;
			break;
		case SDL_WINDOWEVENT:
			switch (Event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				windowWidth = Event.window.data1;
				windowHeight = Event.window.data2;
				window = SDL_GetWindowFromID(Event.window.windowID);
				handleResize(Event.window.data1, Event.window.data2);
				printf("window size changed to %dx%d\n",
				    windowWidth, windowHeight);
				break;
			}
			break;
		case SDL_KEYDOWN:
			switch (Event.key.keysym.sym) {
			case SDLK_ESCAPE:
				gState.running = false;
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
				gState.blankMode = !gState.blankMode;
				printf("blank %s\n",
				    gState.blankMode ? "enabled" : "disabled");
				break;
			case SDLK_c:
				// c = clear
				i = 0;
				while (gState.ringCount > 0) {
					recycleLastRing();
					gState.ringCount--;
					i++;
				}
				printf("cleared %d rings\n", i);
				break;
			case SDLK_f:
				// f = fading
				gState.fadingMode = !gState.fadingMode;
				printf("fading %s\n",
				    gState.fadingMode ? "enabled" : "disabled");
				break;
			case SDLK_l:
				// l = lines
				gState.linesEnabled = !gState.linesEnabled;
				printf("lines %s\n",
				    gState.linesEnabled ? "enabled" : "disabled");
				break;
			case SDLK_m:
				// m = color mode
				gState.colorMode = (gState.colorMode + 1) % 4;
				printf("gState.colorMode = %s\n",
				    colorModeToString(gState.colorMode));
				break;
			case SDLK_p:
				// p = play/pause
				gState.paused = !gState.paused;
				printf("%s\n",
				    gState.paused ? "paused" : "unpaused");
				break;
			case SDLK_r:
				// r = randomize colors
				randomizeMagic(gState.randomMagic);
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

// set the global draw color
static void setDrawColor(unsigned int idx, const float magic[8][3], int alpha) {
	idx %= MAX_COLORS;
	RGB rgb = rainbow(idx);
	rgb = interpolate2rgb(rgb.r, rgb.g, rgb.b, magic);

	Uint8 a = gState.fadingMode ? (Uint8)(alpha * 255 / 100) : 255;
	SDL_SetRenderDrawColor(renderer,
	    (Uint8)(rgb.r * 255.0f),
	    (Uint8)(rgb.g * 255.0f),
	    (Uint8)(rgb.b * 255.0f),
	    a
	);
}

// Draw a filled circle via scanlines
static void DrawFilledCircle(int cx, int cy, int r) {
	for (int dy = -r; dy <= r; dy++) {
		int dx = (int)(sqrtf((float)(r*r - dy*dy)) + 0.5f);
		SDL_RenderDrawLine(renderer,
		    cx - dx, cy + dy,
		    cx + dx, cy + dy
		);
	}
}

// Draw a single particle
static void DrawParticle(Particle *p) {
	int x = windowWidth / 2 + (int)roundf(p->x);
	int y = windowHeight / 2 + (int)roundf(p->y);
	DrawFilledCircle(x, y, p->radius);
}

// Draw line between particles
static void DrawLinesConnectingParticles(Particle *p1, Particle *p2) {
	int x1 = windowWidth / 2 + (int)roundf(p1->x);
	int y1 = windowHeight / 2 + (int)roundf(p1->y);
	int x2 = windowWidth / 2 + (int)roundf(p2->x);
	int y2 = windowHeight / 2 + (int)roundf(p2->y);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

// Initialize SDL window & renderer
static void initGraphics(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
	    err(1, "SDL_Init: %s", SDL_GetError());
    }

    window = SDL_CreateWindow("Undercurrents",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (window == NULL) {
	    err(1, "SDL_CreateWindow: %s", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == NULL) {
	    err(1, "SDL_CreateRenderer: %s", SDL_GetError());
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

// Clear screen with alpha blending
static void clearScreen(void) {
	Uint8 a = 255;
	if (gState.fadingMode) {
		// this does not take FPS into account - we should probably do
		// that
		a = alphaBackground;
	}

	SDL_Rect screen = {0, 0, windowWidth, windowHeight};
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
	SDL_RenderFillRect(renderer, &screen);
}

int main(int argc, char **argv) {
	float rainbowIdx = 0;
	int addNewRingCounter = 0;
	int printStatusLineCounter = 0;
	unsigned int lastTime = 0;

	// parse CLI options
	parseArguments(argv);

	// initalize SDL and OpenGL window
	initGraphics();

	// initialize random
	srand(time(NULL));

	// initialize random colors
	randomizeMagic(gState.randomMagic);

	// print config and controls
	printConfiguration(stdout);
	printf("\n");
	printControls(stdout);
	printf("\n");

	// main loop
	gState.running = true;

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	canvas = SDL_CreateTexture(renderer,
	    SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
	    windowWidth, windowHeight);

	SDL_SetRenderTarget(renderer, canvas);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_SetRenderTarget(renderer, NULL);

	while (gState.running) {
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
			    1000.0 / delta, gState.ringCount, gState.particleCount,
			    gState.recycledParticles);

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
		if (gState.paused) {
			goto show;
		}

		// start drawing to the canvas so can clear over it
		SDL_SetRenderTarget(renderer, canvas);
		clearScreen();

		// check if new ring (and particles) should be created
		addNewRingCounter -= delta;
		if (addNewRingCounter <= 0) {
			addNewRingCounter += timerAddNewRing;

			// add a new ring
			addRing();

			// recycle out-of-view rings
			while (gState.ringCount > ringsMaximum) {
				recycleLastRing();
				gState.ringCount--;
			}

			// add particle(s) to each existing ring
			ringPtr = gState.rings;
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
		ringPtr = gState.rings;
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
		if (gState.blankMode) {
			goto show;
		}

		// set the color here just once if in solid mode
		if (gState.colorMode == ColorModeSolid) {
			setDrawColor(rainbowIdx, gState.randomMagic, alphaElements);
		}

		// draw the particles and lines, start by looping rings
		ringPtr = gState.rings;
		for (int i = 0; ringPtr != NULL; ringPtr = ringPtr->next, i++) {
			ParticleNode *particlePtr = ringPtr->particleNode;

			// set color here if ringed mode
			if (gState.colorMode == ColorModeRinged) {
				unsigned int idx = rainbowIdx + (i * MAX_COLORS / ringsMaximum);
				setDrawColor(idx, gState.randomMagic, alphaElements);
			}

			// loop particles in ring
			for (; particlePtr != NULL; particlePtr = particlePtr->next) {
				Particle *p = particlePtr->particle;

				// check if particle is born
				if (p->bornTimer > 0) {
					continue;
				}

				// set color here if circular mode or random mode
				if (gState.colorMode == ColorModeCircular) {
					unsigned int idx = (unsigned int)(p->position / 360.0 * (float)MAX_COLORS);
					idx = (idx + (int)rainbowIdx) % MAX_COLORS;
					setDrawColor(idx, gState.randomMagic, alphaElements);
				} else if (gState.colorMode == ColorModeIndividual) {
					setDrawColor(p->color + rainbowIdx, gState.randomMagic, alphaElements);
				}

				// draw the particle
				DrawParticle(p);

				// stop here if lines aren't enabled
				if (!gState.linesEnabled) {
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

show:
		SDL_SetRenderTarget(renderer, NULL);
	        SDL_RenderCopy(renderer, canvas, NULL, NULL);

		SDL_RenderPresent(renderer);
		SDL_Delay(1);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
