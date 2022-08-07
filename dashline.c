// dashline.c

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <float.h>
#include <math.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>

#include "dashline.h"

// window size and location
#define WIN_WIDTH   512
#define WIN_HEIGHT  700
#define WIN_LOC_X   400
#define WIN_LOC_Y   10
#define HOME_X      0
#define HOME_Y      0

#define PIX_PER_DASH    10

void dline(bool dash, float x0, float y0, float x1, float y1) {
    float l;
    float m;
    float dx, dy;
    float dxn, dyn;
    float xs, ys;
    float xe, ye;
    int nl;
    int i;
    float pps;

    l = sqrtf(powf((x1 - x0), (float)2) + powf((y1 - y0), (float)2));

    printf("length = %f pixels\n", l);

    nl = ((int)(l / PIX_PER_DASH));
    if (nl % 2 == 0) {
        nl++; // make it an odd number
    }
    pps = l / (float)nl;

    dy = y1 - y0;
    dx = x1 - x0;

    printf("number of dashes = %d\n", nl);
    printf("length per dash = %f\n", pps);
    printf("m = %f\n", m);

    xs = x0;
    ys = y0;
    for (i = 0; i < nl; i++) {

        if (dy == 0) {
            // slope is zero
            if (x1 > x0) {
                xe = xs + pps;
                ye = ys;
            }
            else {
                xe = xs - pps;
                ye = ys;
            }
        }
        else if (dx == 0) {
            // slope is infinite
            if (y1 > y0) {
                xe = xs;
                ye = ys + pps;
            }
            else {
                xe = xs;
                ye = ys + pps;
            }
        }
        else {
            m = (y1 -y0) / (x1 - x0);

            dxn = (pps / sqrt(1 + (m * m)));
            dyn = m * dxn;

            if (m > 0) {
                // slope is positive
                xe = xs + dxn;
                ye = ys + dyn;

            }
            else {
                // slope is negative
                xe = xs - dxn;
                ye = ys - dyn;
            }

            if (x1 > x0) {
                xe = xs + dxn;
            }
            else {
                xe = xs - dxn;
            }

            if (y1 > y0) {
                ye = ys + dyn;
            }
            else {
                ye = ys - dyn;
            }

        }
        if (dash == true) {
            if (i % 2 ==  0) {
                al_draw_line(xs, ys, xe, ye, C585NM, 1);
            }
            else {
                al_draw_line(xs, ys, xe, ye, BLACK, 1);
            }
        }
        else {
            al_draw_line(xs, ys, xe, ye, C585NM, 1);
        }

        xs = xe;
        ys = ye;

    }

}



int main()  {

    int r;
    bool running = true;

    ALLEGRO_DISPLAY *display = NULL;
    ALLEGRO_EVENT_QUEUE *q;
    ALLEGRO_TIMER *timer;
    ALLEGRO_EVENT event;

    al_init();
    al_init_font_addon();
    al_init_primitives_addon();
    al_init_image_addon();
    al_install_keyboard();

    al_set_new_window_position(WIN_LOC_X, WIN_LOC_Y);
    al_set_new_display_flags(DEFAULT_WINDOW_FLAGS);
    display = al_create_display(WIN_WIDTH, WIN_HEIGHT);

    q = al_create_event_queue();
    al_register_event_source(q, al_get_keyboard_event_source());
    timer = al_create_timer(1.0 / 4);
    al_start_timer(timer);

    //al_register_event_source(q, al_get_display_event_source(display));
    al_register_event_source(q, al_get_timer_event_source(timer));

    // clear display
    al_set_target_backbuffer(display);
    al_clear_to_color(BLACK);

    // draw a horizontal line
    dline(true, 10, 10, 200, 10);
    // draw a vertical line
    dline(true, 200, 10, 200, 300);
    // draw diagnol
    dline(true, 200, 300, 10, 10);


    // update display
    al_flip_display();

    while (running) {

        al_wait_for_event(q, &event);

        if (event.type == ALLEGRO_EVENT_KEY_CHAR) {
            printf("key pressed = %d\n", event.keyboard.unichar);
            running = false;
        }

        if (event.type == ALLEGRO_EVENT_TIMER) {

        }
    }

    // quit
    al_destroy_timer(timer);
    al_uninstall_keyboard();

    return 0;
}
