CC := cc
CFLAGS := `sdl2-config --libs --cflags` -lGL -lm

undercurrents: src/undercurrents.c src/ryb2rgb.o src/particle.o
	$(CC) -o $@ $(CFLAGS) $^

src/ryb2rgb.o: src/ryb2rgb.c src/ryb2rgb.h
	$(CC) -o $@ -c $(CFLAGS) $<

src/particle.o: src/particle.c src/particle.h
	$(CC) -o $@ -c $(CFLAGS) $<

.PHONY: clean
clean:
	rm -f undercurrents src/*.o
