CC := cc
CFLAGS := -Wall -Werror -O2

UNAME := $(shell uname -s)

ifeq ($(UNAME),Darwin)
	GL := -framework OpenGL
else
	GL := -lGL
endif

# targets to compile the code
undercurrents: src/main.c src/ryb2rgb.o src/particle.o
	$(CC) -o $@ `sdl2-config --libs --cflags` $(GL) -lm $(CFLAGS) $^

src/ryb2rgb.o: src/ryb2rgb.c src/ryb2rgb.h
	$(CC) -o $@ -c $(CFLAGS) $<

src/particle.o: src/particle.c src/particle.h
	$(CC) -o $@ -c $(CFLAGS) $<

# run the compiled program
.PHONY: run
run: undercurrents
	./undercurrents

# compile the web version
.PHONY: web
web:
	mkdir -p web/_site
	cat web/index.html > web/_site/index.html
	cat web/style.css > web/_site/style.css
	emcc src/*.c --use-port=sdl2 -O2 -o web/_site/debug.html

# serve the website (for testing)
.PHONY: serve
serve:
	emrun web/_site/index.html


# clean up any generated files
.PHONY: clean
clean:
	rm -f undercurrents src/*.o
	rm -rf web/_site
