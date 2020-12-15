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
    ...
    fps=11.627907 ringCount=0 particleCount=0 recycledParticles=0
    fps=66.666667 ringCount=2 particleCount=6 recycledParticles=0
    fps=66.666667 ringCount=4 particleCount=20 recycledParticles=0
    ...

Usage
-----

```
$ ./undercurrents -h
Usage: undercurrents [-h] [--longOpt var]

Options
    -h, --help                      print this message and exit
    -p, --paused                    start in the 'paused' state
    --configVariableName value      set a configuration variable, see below

  configuration variables can be passed as long-opts
    ie: undcurrents --windowHeight 500 --windowWidth 700 --ringsMaximum 20

Configuration
  windowWidth=1200
  windowHeight=1200
  particleSpeedMaximum=30
  particleSpeedFactor=100
  particleRadiusMinimum=1
  particleRadiusMaximum=5
  particleHeightMinimum=0
  particleHeightMaximum=5
  particleLineDistanceMinimum=0
  particleLineDistanceMaximum=200
  particleLineRingDisable=-1
  particleExpandRate=20
  particleBornTimerMaximum=1000
  particleColorSpeed=50
  ringsMaximum=35
  alphaBackground=7
  alphaElements=25
  timerPrintStatusLine=2000
  timerAddNewRing=1000

Controls
- press up / down to modify particle speed
- press left / right to modify particle line distance factor
- press 'b' to toggle blank mode
- press 'f' to toggle fading mode
- press 'l' to toggle particle lines mode
- press 'm' to toggle color modes
- press 'r' to randomize colors
- press 'p' to pause or unpause visuals
```

License
-------

MIT License
