// dashline.c

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
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

#define RAD_PER_CIRCLE  ((float)2 * M_PI)
#define DASH_PER_CIRCLE 40
#define PIX_PER_DASH    10
#define LINE_WIDTH      2
#define TEXTURE_LINE_WIDTH 1

// add texture to rectangle
void texture_rect(float x0, float y0, float x1, float y1, uint16_t t, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg) {

    int i;
    int n, m;
    uint16_t tmask;
    float posx0, posy0, posx1, posy1;

    n = (int)(x1 - x0);
    posx0 = x0;
    posy0 = y0;
    posx1 = x0;
    posy1 = y1;
    printf("sizeof t = %ld\n", sizeof(uint16_t));

    for (i = 0; i < n; i++) {

        m = i % 16;
        if (m == 0) {
            tmask = 0x8000;
        }
        printf("m = %u\n", m);
        printf("tmask = %u\n", tmask);
        printf("tmask & t = %u\n", tmask& t);
        if (t & tmask) {
            al_draw_line(posx0, posy0, posx1, posy1, fg, TEXTURE_LINE_WIDTH);
        }
        else {
            al_draw_line(posx0, posy0, posx1, posy1, bg, TEXTURE_LINE_WIDTH);
        }

        tmask = tmask >> 1;
        posx0 = x0 + (float)i;
        posx1 = x0 + (float)i;
    }
}

void dline(float x0, float y0, float x1, float y1, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg) {
    float l;
    float m;
    float dx, dy;
    float dxn, dyn;
    float xs, ys;
    float xe, ye;
    int nl;
    int i;
    float pps;

    // determine length of line
    l = sqrtf(powf((x1 - x0), (float)2) + powf((y1 - y0), (float)2));

    printf("length = %f pixels\n", l);

    // determine length of dash
    nl = ((int)(l / PIX_PER_DASH));
    if (nl % 2 == 0) {
        // make it an odd number,
        // so there is a solid dash at the start and end of the line
        nl++;
    }
    pps = l / (float)nl;

    printf("number of dashes = %d\n", nl);
    printf("length per dash = %f\n", pps);
    printf("m = %f\n", m);

    dy = y1 - y0;
    dx = x1 - x0;
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
            // knowing the slope and dash length, calculate the ending x and y points
            m = (y1 -y0) / (x1 - x0);
            dxn = (pps / sqrt(1 + (m * m)));
            dyn = m * dxn;

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

        // draw a dash
        if (i % 2 ==  0) {
            al_draw_line(xs, ys, xe, ye, fg, LINE_WIDTH);
        }
        else {
            al_draw_line(xs, ys, xe, ye, bg, LINE_WIDTH);
        }

        // initialize for next dash calculation
        xs = xe;
        ys = ye;

    }

}

void line(bool dash, float x0, float y0, float x1, float y1, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg) {

    if (dash == true) {
        // draw a dashed line
        dline(x0, y0, x1, y1, fg, bg);
    }
    else {
        // draw a solid line
        al_draw_line(x0, y0, x1, y1, fg, LINE_WIDTH);
    }
}

// draw a rectangle, solid or dashed outline
void rect(bool dash, float x0, float y0, float x1, float y1, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg) {
    float w, h;
    w = x1 - x0;
    h = y1 - y0;
    line(dash, x0, y0, x0 + w, y0, fg, bg);
    line(dash, x0 + w, y0, x0 + w, y1, fg, bg);
    line(dash, x0, y0, x0, y1, fg, bg);
    line(dash, x0, y0 + h, x1, y1, fg, bg);
}

// draw a rectangle, filled with a solid color
void rect_filled(float x0, float y0, float x1, float y1, uint16_t t, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg) {
    if (t == 0xffff) {
        al_draw_filled_rectangle(x0, y0, x1, y1, fg);
    }
    else {
        texture_rect(x0, y0, x1, y1, t, fg, bg);
        rect(false, x0, y0, x1, y1, fg, bg);
    }
}

// draw a dashed circle
void dcircle(float x, float y, float r, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg, float t) {

    int i;
    float ds, dd;

    dd = (float)RAD_PER_CIRCLE / (float)DASH_PER_CIRCLE;

    for ( i = 0; i < DASH_PER_CIRCLE; i++) {
        ds = i * dd;
        if (i %2 != 0) {
            al_draw_arc(x, y, r, ds, dd, fg, t);
        }
        else {
            al_draw_arc(x, y, r, ds, dd, bg, t);
        }
    }
}

// draw a dashed or solid circle
void circle(bool dash, float x, float y, float r, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg, float t) {

    if (dash == true) {
        dcircle(x, y, r, fg, bg, t);

    }
    else {
        al_draw_circle(x, y, r, fg, t);

    }
}

// get raster data
 uint8_t * get_raster_file(char *filename, struct stat *sb) {

    assert(filename != NULL);
    assert(sb != NULL);

    int fd;
    int r;
    uint8_t *file_in_mem;

    fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR);
    r = fstat(fd, sb);
    assert(r != -1);

    file_in_mem = mmap(NULL, sb->st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    return (uint8_t *)file_in_mem;
}


const uint8_t raster_mask[8] = {
    0b10000000,
    0b01000000,
    0b00100000,
    0b00010000,
    0b00001000,
    0b00000100,
    0b00000010,
    0b00000001,
};

// draw raster pattern
void draw_raster(float x, float y, int width, uint8_t *data, uint32_t size, ALLEGRO_COLOR fg, ALLEGRO_COLOR bg) {

    uint32_t i;
    uint8_t m, pattern;
    uint8_t *ptr;
    float nx, ny;
    float posx, posy;

    ptr = data;
    nx = 0;
    ny = 0;
    posx = x;
    posy = y;

    for (i = 0; i < size; i++) {

        pattern = *ptr;

        for (m = 0; m < 8; m++) {

            if (pattern & raster_mask[m]) {
                al_draw_pixel(posx, posy, fg);
            }
            else {
                al_draw_pixel(posx, posy, bg);
            }

            nx++;
            posx = nx + x;
            if (posx >= width) {
                nx = 0;
                ny++;
                posy = ny + y;
            }
        }
        ptr++;
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
    line(true, 10, 10, 200, 10, C585NM, BLACK);
    // draw a vertical line
    line(true, 200, 10, 200, 300, C585NM, BLACK);
    // draw diagonal
    line(true, 200, 300, 10, 10, C585NM, BLACK);
    line(false, 200, 300, 400, 300, C585NM, BLACK);
    line(false, 200, 300, 400, 10, C585NM, BLACK);

    // draw a dashed rectangle
    rect(true, 10, 400, 400, 500, C585NM, BLACK);
    // draw a filled rectangle
    rect_filled(10, 550, 400, 600, 0xF0F0, C585NM, BLACK);

    // draw dashed circle
    circle(true, 60, 300, 50, C585NM, BLACK, LINE_WIDTH);

    // draw raster pattern
    #define RASTER_FILE "dap_raster_test.bmp"
    uint8_t *file_in_mem;
    struct stat sb;
    file_in_mem = get_raster_file(RASTER_FILE, &sb);
    draw_raster(0, 610, 512, file_in_mem+0, sb.st_size, C585NM, BLACK);

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

