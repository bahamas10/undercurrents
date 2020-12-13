Undercurrents
=============

Visualizer made in C and OpenGL and SDL2.

Compile
-------

Make sure `sdl2` is installed.  Run `make` to compile the program:

    $ make
    cc -o src/ryb2rgb.o -c `sdl2-config --libs --cflags` -lGL -lm -Wall -Werror -O2 src/ryb2rgb.c
    cc -o src/particle.o -c `sdl2-config --libs --cflags` -lGL -lm -Wall -Werror -O2 src/particle.c
    cc -o undercurrents `sdl2-config --libs --cflags` -lGL -lm -Wall -Werror -O2 src/undercurrents.c src/ryb2rgb.o src/particle.o
    $ ./undercurrents
    - press up / down to modify expansion rate
    - press left / right to modify max rings
    - press 'r' to randomize colors
    - press 'f' to toggle fade
    - press 'm' to toggle color modes

    WINDOW_WIDTH=1200
    WINDOW_HEIGHT=1200
    PARTICLE_SPEED_MAXIMUM=25
    PARTICLE_RADIUS_MINIMUM=1
    PARTICLE_RADIUS_MAXIMUM=5
    PARTICLE_HEIGHT_MINIMUM=0
    PARTICLE_HEIGHT_MAXIMUM=5
    PARTICLE_LINE_DISTANCE_MINIMUM=0
    PARTICLE_LINE_DISTANCE_MAXIMUM=200
    PARTICLE_EXPAND_RATE=20
    PARTICLE_BORN_TIMER_MAXIMUM=1000
    PARTICLE_COLOR_SPEED=50
    RINGS_MAXIMUM=34
    ALPHA_BACKGROUND=0.032000
    ALPHA_ELEMENTS=0.100000

    fps=11.627907 ringCount=0 particleCount=0 recycledParticles=0
    fps=66.666667 ringCount=2 particleCount=6 recycledParticles=0
    fps=66.666667 ringCount=4 particleCount=20 recycledParticles=0
    ...

License
-------

MIT License
