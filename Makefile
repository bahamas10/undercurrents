CC := cc
CFLAGS := -Wall -Werror -O2

UNAME := $(shell uname -s)

ifeq ($(UNAME),Darwin)
	GL := -framework OpenGL
else
	GL := -lGL
endif

undercurrents: src/main.c src/ryb2rgb.o src/particle.o
	$(CC) -o $@ `sdl2-config --libs --cflags` $(GL) -lm $(CFLAGS) $^

src/ryb2rgb.o: src/ryb2rgb.c src/ryb2rgb.h
	$(CC) -o $@ -c $(CFLAGS) $<

src/particle.o: src/particle.c src/particle.h
	$(CC) -o $@ -c $(CFLAGS) $<

.PHONY: run
run: undercurrents
	./undercurrents

.PHONY: clean
clean:
	rm -f undercurrents src/*.o
