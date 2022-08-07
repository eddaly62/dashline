CC=gcc
CFLAGS=-ggdb -O0
ALLEGRO_FLAGS=-I/usr/local/include/allegro5 -L/usr/local/lib/ -Wl,-R/usr/local/lib -lallegro_primitives -lallegro_image -lallegro -lallegro_color -lallegro_main -lallegro_font -lpthread -lm

dashline: dashline.c
	$(CC) $(CFLAGS) -o $@ $^ $(ALLEGRO_FLAGS)

clean:
	rm -f dashline