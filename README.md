Undercurrents
=============

Visualizer made in C and OpenGL and SDL2.

Compile
-------

Make sure `sdl2` is installed.  Run `make` to compile the program:

    $ make
    cc -o src/ryb2rgb.o -c `sdl2-config --libs --cflags` -lGL -lm src/ryb2rgb.c
    cc -o undercurrents `sdl2-config --libs --cflags` -lGL -lm src/undercurrents.c src/ryb2rgb.o
    $ ./undercurrents
    - press up / down to modify expansion rate
    - press left / right to modify max rings
    - press 'r' to randomize colors

    fps=9.900990 ringCount=0 particleCount=0 recycledParticles=0
    fps=62.500000 ringCount=1 particleCount=2 recycledParticles=0
    fps=66.666667 ringCount=2 particleCount=6 recycledParticles=0
    ...

License
-------

MIT License
