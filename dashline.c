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
#include <assert.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>

#include "dashline.h"

// raster file
#define RASTER_FILE "dap_raster_test.bmp"

// window size and location
#define WIN_WIDTH   1024
#define WIN_HEIGHT  768
#define WIN_LOC_X   400
#define WIN_LOC_Y   10
#define HOME_X      0
#define HOME_Y      0

#define RAD_PER_CIRCLE  ((float)2 * M_PI)
#define DASH_PER_CIRCLE 30
#define PIX_PER_DASH    10
#define LINE_WIDTH      1
//#define TEXTURE_LINE_WIDTH 1
#define BORDER_LINE_WIDTH   1
#define NUM_OF_TEXTURE_BITS    16
#define STARTING_TEXTURE_MASK   0x8000
#define RASTER_BITS    8
#define STARTING_RASTER_MASK   0x80

// aliases
#define LINE_NONE   BORDER_NONE
#define LINE_SOLID  BORDER_SOLID
#define LINE_DASH   BORDER_DASH
#define LINE_PATTERN    BORDER_PATTERN

enum GBORDER {
    BORDER_NONE,
    BORDER_SOLID,
    BORDER_DASH,
    BORDER_PATTERN,
    BORDER_MAX,
};

enum GFILL {
    FILL_NONE,
    FILL_SOLID,
    FILL_VERTBARS,
    FILL_PATTERN,
    FILL_MAX,
};

enum GTYPE {
    TYPE_LINE,
    TYPE_CIRCLE,
    TYPE_RECTANGLE,
    TYPE_RASTER,
    TYPE_MAX,
};

typedef struct gclr {
    ALLEGRO_COLOR fg;  // foreground color
    ALLEGRO_COLOR bg;  // background color
    bool invert;
    bool overlay;       // write only the foreground color
    bool erase;         // make foreground equal to the background color and write only the foreground color
} GCOLOR;               // and write only the foreground color

typedef struct gs {
    int border;         // valid values are in enum GBORDER
    int fill;           // valid values are in enum GFILL
    uint16_t pattern;   // hash pattern
    bool clip;          // if true, clip if not viewable, otherwise wrap
} GSTYLE;

typedef struct gc{
    float x;            // circle parameters
    float y;
    float radius;
} GCIRCLE;

typedef struct gr{
    float x0;           // rectangle parameters
    float y0;
    float x1;
    float y1;
} GRECTANGLE;

typedef struct gl{
    float x0;           // line parameters
    float y0;
    float x1;
    float y1;
} GLINE;

typedef struct grast {
    float x;
    float y;
    int width;
    int fd;             // file descriptor of raster file
    size_t  fdlength;   // file length
    uint8_t *rdataptr;  // pointer to raster data array in memory
} GRASTER;

typedef struct gro {
    int     gtype;      // Valid values are defined in the GTYPE enum
    GCOLOR  gc;
    GSTYLE  gs;

    union {
        GCIRCLE gcirc;
        GLINE   gline;
        GRASTER grast;
        GRECTANGLE   grect;
    };

} GRAPH_OBJ;

GRAPH_OBJ   g;

// prototypes
int dap_open_raster_file(GRAPH_OBJ *go, char *filename);
void dap_draw_line(GRAPH_OBJ *go);



// texture mask
uint16_t texture_mask(GRAPH_OBJ *go, float x, float y) {
    return go->gs.pattern & (1 << ((int)(x + y)) % 16);
}

// set graphic type
void dap_set_graph_type(GRAPH_OBJ *go, int gt) {

    assert(go != NULL);
    assert(gt < TYPE_MAX);
    go->gtype = gt;
}

// set graphics colors
void dap_set_graph_color(GRAPH_OBJ *go, bool invert, ALLEGRO_COLOR fgc, ALLEGRO_COLOR bgc) {

    assert(go != NULL);
    go->gc.invert = invert;
    memcpy(&go->gc.fg, &fgc, sizeof(ALLEGRO_COLOR));
    memcpy(&go->gc.bg, &bgc, sizeof(ALLEGRO_COLOR));
}

// set graphic style hash pattern
void dap_set_graph_style_pattern(GRAPH_OBJ *go, uint16_t pattern) {

    assert(go != NULL);
    go->gs.pattern = pattern;
}

// get graphic style hash pattern
uint16_t dap_get_graph_style_pattern(GRAPH_OBJ *go) {

    assert(go != NULL);
    return go->gs.pattern;
}

// set graphic style clip or wrap if graphic is outside of viewable window
void dap_set_graph_style_clip(GRAPH_OBJ *go, bool clip) {

    assert(go != NULL);
    go->gs.clip = clip;
}

// set graphic style fill, type of fill for circle and rectangles
void dap_set_graph_style_fill(GRAPH_OBJ *go, int filltype) {

    assert(go != NULL);
    assert(filltype < FILL_MAX);
    go->gs.fill = filltype;
}

// set graphic style border, type of border for circle and rectangles
void dap_set_graph_style_border(GRAPH_OBJ *go, int bordertype) {

    assert(go != NULL);
    assert(bordertype < BORDER_MAX);
    go->gs.border = bordertype;
}

// set graphic style
void dap_set_graph_style(GRAPH_OBJ *go, int gb, int gf, uint16_t pattern) {

    assert(go != NULL);
    assert(gf < FILL_MAX);
    assert(gb < BORDER_MAX);

    go->gs.border = (int)gb;
    go->gs.fill = (int)gf;
    go->gs.pattern = pattern;
}

// set circle
void dap_set_circle(GRAPH_OBJ *go, float x, float y, float r) {

    assert(go != NULL);
    go->gcirc.x = x;
    go->gcirc.y = y;
    go->gcirc.radius = r;
    go->gtype = TYPE_CIRCLE;
}

// set rectangle
void dap_set_rectangle(GRAPH_OBJ *go, float x0, float y0, float x1, float y1) {

    assert(go != NULL);
    go->grect.x0 = x0;
    go->grect.y0 = y0;
    go->grect.x1 = x1;
    go->grect.y1 = y1;
    go->gtype = TYPE_RECTANGLE;
}

// set line
void dap_set_line(GRAPH_OBJ *go, float x0, float y0, float x1, float y1) {

    assert(go != NULL);
    go->gline.x0 = x0;
    go->gline.y0 = y0;
    go->gline.x1 = x1;
    go->gline.y1 = y1;
    go->gtype = TYPE_LINE;
}

// set raster file
// use when raster data is a file
// return 0 if success, otherwise -1
int dap_set_raster_file(GRAPH_OBJ *go, char *filename, float x0, float y0, int width) {

    assert(go != NULL);
    assert(filename != NULL);
    int r;

    go->gtype = TYPE_RASTER;
    go->grast.x = x0;
    go->grast.y = y0;
    go->grast.width = width; // width of screen

    // open file and map into memory
    r = dap_open_raster_file(go, filename);
    return r;
}

// set raster data
// use when raster data resides n memory
void dap_set_raster_data(GRAPH_OBJ *go, float x0, float y0, int width, uint8_t *rptr, size_t len) {

    assert(go != NULL);
    assert(rptr != NULL);
    assert(len > 0);
    assert(width > 0);

    go->gtype = TYPE_RASTER;
    go->grast.rdataptr = rptr;
    go->grast.fdlength = len;
    go->grast.x = x0;
    go->grast.y = y0;
    go->grast.width = width;
}

// draws a vertical line using pixel primatives, to facilitate wrapping and clipping
// helper function for drawing vertical bars
void draw_vert_line(float x, float y0, float y1, ALLEGRO_COLOR fg) {

    int i, dy;

    if (y1 > y0) {
        dy = (int)(y1 - y0);
        for (i = 0; i < dy; i++) {
            al_draw_pixel(x, y0+i, fg);
        }
    }
    else if (y1 < y0) {
        dy = (int)(y0 - y1);
        for (i = 0; i < dy; i++) {
            al_draw_pixel(x, y1+i, fg);
        }
    }
}

// fill circle with vertical bars
void circle_fill_vertbar(GRAPH_OBJ *go) {

    int i;
    int n, m;
    uint16_t tmask;
    float posyt, posyb, posxs, posx, dy, r;

    tmask = STARTING_TEXTURE_MASK;
    r = go->gcirc.radius;
    posyt = go->gcirc.y;
    posyb = go->gcirc.y;
    posxs = go->gcirc.x - r;
    posx = posxs;
    n = (int)(2 * r);

    for (i = 0; i < n; i++) {

        m = i % NUM_OF_TEXTURE_BITS;
        if (m == 0) {
            tmask = STARTING_TEXTURE_MASK;
        }

        // calculate the start and end of a vertical line to use for texture
        // circle equation: (x-x1)^2 + (y-y1)^2 = r^2
        // x1, y1 is the center of the circle and r is the radius
        // solve for y top and bottom for a series of x values
        posyb = sqrtf(powf(r,2) - powf((posx - go->gcirc.x),2)) + go->gcirc.y;
        posyt = go->gcirc.y - (posyb - go->gcirc.y);
        dy = posyb - posyt;

        if (go->gs.pattern & tmask) {
            if (dy > 0) {
                draw_vert_line(posx, posyt, posyb, go->gc.fg);
            }
        }
        else {
            if (dy > 0) {
                draw_vert_line(posx, posyt, posyb, go->gc.bg);
            }
        }

        tmask = tmask >> 1;
        posx = posxs + (float)i;
    }
}

// fill circle with texture pattern
void circle_fill_pattern(GRAPH_OBJ *go) {

    int i, r;
    int n, q;
    uint16_t tmask;
    float posyt, posyb, posxs, posx, posy, dy, rad;

    rad = go->gcirc.radius;
    posyt = go->gcirc.y;
    posyb = go->gcirc.y;
    posxs = go->gcirc.x - rad;
    posx = posxs;
    n = (int)(2 * rad);

    for (i = 0; i <= n; i++) {

        // calculate the start and end of a vertical line to use for texture
        // circle equation: (x-x1)^2 + (y-y1)^2 = r^2
        // x1, y1 is the center of the circle and r is the radius
        // solve for y top and bottom for a series of x values
        posyb = sqrtf(powf(rad,2) - powf((posx - go->gcirc.x),2)) + go->gcirc.y;
        posyt = go->gcirc.y - (posyb - go->gcirc.y);
        dy = posyb - posyt;

        q = (int)(dy);
        for (r = 0; r <= q; r++) {

            posy = (float)r + posyt;
            tmask = texture_mask(go, posx, posy);

            if (go->gs.pattern & tmask) {
                al_draw_pixel(posx, posy, go->gc.fg);
            }
            else {
                al_draw_pixel(posx, posy, go->gc.bg);
            }

        }

        posx = posxs + (float)i;

    }
}

// fill circle with solid fill
void circle_fill_solid(GRAPH_OBJ *go) {

    int i, r;
    int n, q;
    float posyt, posyb, posxs, posx, posy, dy, rad;

    rad = go->gcirc.radius;
    posyt = go->gcirc.y;
    posyb = go->gcirc.y;
    posxs = go->gcirc.x - rad;
    posx = posxs;
    n = (int)(2 * rad);

    for (i = 0; i <= n; i++) {

        // calculate the start and end of a vertical line to use for texture
        // circle equation: (x-x1)^2 + (y-y1)^2 = r^2
        // x1, y1 is the center of the circle and r is the radius
        // solve for y top and bottom for a series of x values
        posyb = sqrtf(powf(rad,2) - powf((posx - go->gcirc.x),2)) + go->gcirc.y;
        posyt = go->gcirc.y - (posyb - go->gcirc.y);
        dy = posyb - posyt;

        q = (int)(dy);
        for (r = 0; r <= q; r++) {

            posy = (float)r + posyt;
            al_draw_pixel(posx, posy, go->gc.fg);
        }
        posx = posxs + (float)i;
    }
}


// add vertical bar pattern to a rectangle
void rect_fill_vertbar(GRAPH_OBJ *go) {

    int i;
    int n, m;
    uint16_t tmask;
    float posx0, posy0, posy1;

    tmask = STARTING_TEXTURE_MASK;
    n = (int)(go->grect.x1 - go->grect.x0);
    posx0 = go->grect.x0;
    posy0 = go->grect.y0;
    posy1 = go->grect.y1;

    for (i = 0; i < n; i++) {

        m = i % NUM_OF_TEXTURE_BITS;
        if (m == 0) {
            tmask = STARTING_TEXTURE_MASK;
        }

        if (go->gs.pattern & tmask) {
            draw_vert_line(posx0, posy0, posy1, go->gc.fg);
        }
        else {
            draw_vert_line(posx0, posy0, posy1, go->gc.bg);
        }

        tmask = tmask >> 1;
        posx0 = go->grect.x0 + (float)i;
    }
}

// add texture pattern to a rectangle
void rect_fill_pattern(GRAPH_OBJ *go) {

    int r, c, n, q;
    uint16_t tmask;
    float posx, posy;

    n = (int)(go->grect.x1 - go->grect.x0);
    q = (int)(go->grect.y1 - go->grect.y0);

    posx = go->grect.x0;
    posy = go->grect.y0;

    for (r = 0; r < q; r++) {
        for (c = 0; c < n; c++) {

            tmask = texture_mask(go, posx, posy);
            if (go->gs.pattern & tmask) {
                al_draw_pixel(posx, posy, go->gc.fg);
            }
            else {
                al_draw_pixel(posx, posy, go->gc.bg);
            }

            posx = go->grect.x0 + (float)c;
        }
        posy = go->grect.y0 + (float)r;
    }
}

// add solid fill to a rectangle
void rect_fill_solid(GRAPH_OBJ *go) {

    int r, c, n, q;
    float posx, posy;

    n = (int)(go->grect.x1 - go->grect.x0);
    q = (int)(go->grect.y1 - go->grect.y0);

    posx = go->grect.x0;
    posy = go->grect.y0;

    for (r = 0; r < q; r++) {
        for (c = 0; c < n; c++) {

            al_draw_pixel(posx, posy, go->gc.fg);
            posx = go->grect.x0 + (float)c;
        }
        posy = go->grect.y0 + (float)r;
    }
}

// draw a straight line using a texture pattern
void line_pattern(GRAPH_OBJ *go) {

    assert(go != NULL);
    int i;
    float l, m;
    float dx, dy, dxn, dyn;
    float xs, ys, xe, ye;
    float x0, y0, x1, y1;
    uint16_t mask;

    x0 = go->gline.x0;
    y0 = go->gline.y0;
    x1 = go->gline.x1;
    y1 = go->gline.y1;

    // determine length of line
    l = sqrtf(powf((x1 - x0), (float)2) + powf((y1 - y0), (float)2));

    dy = y1 - y0;
    dx = x1 - x0;
    xs = x0;
    ys = y0;
    for (i = 0; i < (int)l; i++) {

        if (dy == 0) {
            // slope is zero
            if (x1 > x0) {
                xe = xs + 1;
                ye = ys;
            }
            else {
                xe = xs - 1;
                ye = ys;
            }
        }
        else if (dx == 0) {
            // slope is infinite
            if (y1 > y0) {
                xe = xs;
                ye = ys + 1;
            }
            else {
                xe = xs;
                ye = ys + 1;
            }
        }
        else {
            // slope is neither 0 or infinite
            // knowing the slope and dash length, calculate the ending x and y points
            m = (y1 -y0) / (x1 - x0);
            dxn = (1 / sqrt(1 + (m * m)));
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

        // draw
        mask = texture_mask(go, xe, ye);
        if (mask) {
            al_draw_pixel(xe, ye, go->gc.fg);
        }
        else {
            al_draw_pixel(xe, ye, go->gc.bg);
        }

        // initialize for next segment calculation
        xs = xe;
        ys = ye;
    }
}

void line_dash(GRAPH_OBJ *go) {

    assert(go != NULL);
    float pps;
    float l, m;
    float dx, dy, dxn, dyn;
    float xs, ys, xe, ye;
    float x0, y0, x1, y1;
    int nl, i;

    x0 = go->gline.x0;
    y0 = go->gline.y0;
    x1 = go->gline.x1;
    y1 = go->gline.y1;

    // determine length of line
    l = sqrtf(powf((x1 - x0), (float)2) + powf((y1 - y0), (float)2));

    // determine length of dash
    nl = ((int)(l / PIX_PER_DASH));
    if (nl % 2 == 0) {
        // make it an odd number,
        // so there is a solid dash at the start and end of the line
        nl++;
    }
    pps = l / (float)nl;

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
            // slope is neither 0 or infinite
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
            // draw odd number segments with foreground color so
            // start and end segments can be seen
            al_draw_line(xs, ys, xe, ye, go->gc.fg, LINE_WIDTH);
        }
        else {
            al_draw_line(xs, ys, xe, ye, go->gc.bg, LINE_WIDTH);
        }

        // initialize for next dash calculation
        xs = xe;
        ys = ye;
    }
}

// draw a solid line
void line_solid(GRAPH_OBJ *go) {

    assert(go != NULL);
    float x0, y0, x1, y1;

    x0 = go->gline.x0;
    y0 = go->gline.y0;
    x1 = go->gline.x1;
    y1 = go->gline.y1;

    al_draw_line(x0, y0, x1, y1, go->gc.fg, LINE_WIDTH);
}

// draw a rectangle with solid lines
void rect_border_solid(GRAPH_OBJ *go) {

    assert(go != NULL);
    float x0,y0, x1, y1;
    float w, h;
    GRAPH_OBJ gl;

    x0 = go->grect.x0;
    y0 = go->grect.y0;
    x1 = go->grect.x1;
    y1 = go->grect.y1;
    w = x1 - x0;
    h = y1 - y0;

    // rectangles are just four lines, so use the line functions with a local GRAPH_OBJ
    dap_set_graph_color(&gl, go->gc.invert, go->gc.fg, go->gc.bg);
    dap_set_graph_style_border(&gl, LINE_SOLID);
    dap_set_line(&gl, x0, y0, x0 + w, y0);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0 + w, y0, x0 + w, y1);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0, y0, x0, y1);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0, y0 + h, x1, y1);
    dap_draw_line(&gl);
}

// draw a rectangle with dash lines
void rect_border_dash(GRAPH_OBJ *go) {

    assert(go != NULL);
    float x0,y0, x1, y1;
    float w, h;
    GRAPH_OBJ gl;

    x0 = go->grect.x0;
    y0 = go->grect.y0;
    x1 = go->grect.x1;
    y1 = go->grect.y1;
    w = x1 - x0;
    h = y1 - y0;

    // rectangles are just four lines, so use the line functions with a local GRAPH_OBJ
    dap_set_graph_color(&gl, go->gc.invert, go->gc.fg, go->gc.bg);
    dap_set_graph_style_border(&gl, LINE_DASH);
    dap_set_line(&gl, x0, y0, x0 + w, y0);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0 + w, y0, x0 + w, y1);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0, y0, x0, y1);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0, y0 + h, x1, y1);
    dap_draw_line(&gl);
}

// draw a rectangle with using the pattern
void rect_border_pattern(GRAPH_OBJ *go) {

    assert(go != NULL);
    float x0,y0, x1, y1;
    float w, h;
    uint16_t pattern;
    GRAPH_OBJ gl;

    x0 = go->grect.x0;
    y0 = go->grect.y0;
    x1 = go->grect.x1;
    y1 = go->grect.y1;
    w = x1 - x0;
    h = y1 - y0;


    // rectangles are just four lines, so use the line functions with a local GRAPH_OBJ
    dap_set_graph_color(&gl, go->gc.invert, go->gc.fg, go->gc.bg);
    pattern = dap_get_graph_style_pattern(go);
    dap_set_graph_style_pattern(&gl, pattern);
    dap_set_graph_style_border(&gl, LINE_PATTERN);
    dap_set_line(&gl, x0, y0, x0 + w, y0);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0 + w, y0, x0 + w, y1);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0, y0, x0, y1);
    dap_draw_line(&gl);
    dap_set_line(&gl, x0, y0 + h, x1, y1);
    dap_draw_line(&gl);
}

// draw a dashed circle
void circle_border_dash(GRAPH_OBJ *go) {

    assert(go != NULL);
    int i;
    float ds, dd;
    float x, y, r;

    x = go->gcirc.x;
    y = go->gcirc.y;
    r = go->gcirc.radius;

    dd = (float)RAD_PER_CIRCLE / (float)DASH_PER_CIRCLE;

    for ( i = 0; i < DASH_PER_CIRCLE; i++) {
        ds = i * dd;
        if (i % 2 != 0) {
            al_draw_arc(x, y, r, ds, dd, go->gc.fg, BORDER_LINE_WIDTH);
        }
        else {
            al_draw_arc(x, y, r, ds, dd, go->gc.bg, BORDER_LINE_WIDTH);
        }
    }
}

// draw a solid circle border
void circle_border_solid(GRAPH_OBJ *go) {

    assert(go != NULL);
    float x, y, r;

    x = go->gcirc.x;
    y = go->gcirc.y;
    r = go->gcirc.radius;
    al_draw_circle(x, y, r, go->gc.fg, BORDER_LINE_WIDTH);
}

// draw a circular border using a texture pattern
void circle_border_pattern(GRAPH_OBJ *go) {

    int i, n;
    uint16_t tmask;
    float posyt, posyb, posxs, posx, rad;

    rad = go->gcirc.radius;
    posyt = go->gcirc.y;
    posyb = go->gcirc.y;
    posxs = go->gcirc.x - rad;
    posx = posxs;
    n = (int)(2 * rad);

    for (i = 0; i <= n; i++) {

        // calculate the start and end of a vertical line to use for texture
        // circle equation: (x-x1)^2 + (y-y1)^2 = r^2
        // x1, y1 is the center of the circle and r is the radius
        // solve for y top and bottom for a series of x values
        posyb = sqrtf(powf(rad,2) - powf((posx - go->gcirc.x),2)) + go->gcirc.y;
        posyt = go->gcirc.y - (posyb - go->gcirc.y);

        // draw top half of circle
        tmask = texture_mask(go, posx, posyt);
        if (go->gs.pattern & tmask) {
            al_draw_pixel(posx, posyt, go->gc.fg);
        }
        else {
            al_draw_pixel(posx, posyt, go->gc.bg);
        }

        // draw bottom half of circle
        tmask = texture_mask(go, posx, posyb);
        if (go->gs.pattern & tmask) {
            al_draw_pixel(posx, posyb, go->gc.fg);
        }
        else {
            al_draw_pixel(posx, posyb, go->gc.bg);
        }

        posx = posxs + (float)i;
    }
}

// get raster data
// open file and memory map so it can be accessed as an array
// returns 0 if success, otherwise -1
 int dap_open_raster_file(GRAPH_OBJ *go, char *filename) {

    assert(go != NULL);
    assert(filename != NULL);

    int r;
    uint8_t *dataptr;
    struct stat sb;

    // open file and get stats (size)
    go->grast.fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR);
    assert(go->grast.fd != -1);

    r = fstat(go->grast.fd, &sb);
    if (r == -1) {
        go->grast.fdlength = 0;
        return -1;
    }
    else {
        go->grast.fdlength = sb.st_size;
    }

    // memory map file, return pointer to array of data bytes
    dataptr = mmap(NULL, go->grast.fdlength, PROT_READ, MAP_PRIVATE, go->grast.fd, 0);
    if (dataptr == NULL) {
        go->grast.rdataptr = NULL;
        return -1;
    }
    else {
        go->grast.rdataptr = dataptr;
    }

    return 0;
}

// close raster file
// returns 0 if success, otherwise -1
 int dap_close_raster_file(GRAPH_OBJ *go) {

    assert(go != NULL);
    int r;

    // open file and get stats (size)
    r = close(go->grast.fd);
    return r;
}

// draw raster pattern
// returns 0 if success, otherwise -1
int dap_draw_raster(GRAPH_OBJ *go) {

    assert(go != NULL);

    uint32_t i;
    uint8_t m, rmask, pattern;
    uint8_t *ptr;
    float nx, ny;
    float posx, posy;

    if ((go->grast.width == 0) || (go->grast.fdlength == 0) || go->grast.rdataptr == NULL) {
        // nothing to draw
        return -1;
    }

    ptr = go->grast.rdataptr;
    nx = 0;
    ny = 0;
    posx = go->grast.x;
    posy = go->grast.y;
    rmask = STARTING_RASTER_MASK;

    for (i = 0; i < (go->grast.fdlength * (uint32_t)RASTER_BITS); i++) {

        // check if time to get next byte from raster file and reset bit mask
        m = i % (uint32_t)RASTER_BITS;
        if (m == 0) {
            pattern = *ptr;
            ptr++;
            rmask = STARTING_RASTER_MASK;
        }

        if (pattern & rmask) {
            al_draw_pixel(posx, posy, go->gc.fg);
        }
        else {
            al_draw_pixel(posx, posy, go->gc.bg);
        }

        nx++;
        posx = nx + go->grast.x;
        if (posx >= go->grast.width) {
            nx = 0;
            ny++;
            posy = ny + go->grast.y;
        }
        rmask = rmask >> 1;
    }
    return 0;
}

// draw a line
void dap_draw_line(GRAPH_OBJ *go) {

    assert(go != NULL);
    if (go->gtype == TYPE_LINE) {

        switch(go->gs.border)
        {
            case BORDER_SOLID:
            line_solid(go);
            break;

            case BORDER_DASH:
            line_dash(go);
            break;

            case BORDER_PATTERN:
            line_pattern(go);
            break;

            default:
            assert(go->gs.border < BORDER_MAX);
            break;
        }
    }
}

// draw rectangle fill
void dap_draw_rectangle_fill(GRAPH_OBJ *go) {

    assert(go != NULL);
    if (go->gtype == TYPE_RECTANGLE) {

        switch(go->gs.fill)
        {
            case FILL_SOLID:
            rect_fill_solid(go);
            break;

            case FILL_VERTBARS:
            rect_fill_vertbar(go);
            break;

            case FILL_PATTERN:
            rect_fill_pattern(go);
            break;

            default:
            assert(go->gs.fill < FILL_MAX);
            break;
        }
    }
}


// draw circle fill
void dap_draw_circle_fill(GRAPH_OBJ *go) {

    assert(go != NULL);
    if (go->gtype == TYPE_CIRCLE) {

        switch(go->gs.fill)
        {
            case FILL_SOLID:
            circle_fill_solid(go);
            break;

            case FILL_VERTBARS:
            circle_fill_vertbar(go);
            break;

            case FILL_PATTERN:
            circle_fill_pattern(go);
            break;

            default:
            assert(go->gs.fill < FILL_MAX);
            break;
        }
    }
}

// draw circle border
void dap_draw_circle_border(GRAPH_OBJ *go) {

    assert(go != NULL);
    if (go->gtype == TYPE_CIRCLE) {

        switch(go->gs.border)
        {
            case BORDER_SOLID:
            circle_border_solid(go);
            break;

            case BORDER_DASH:
            circle_border_dash(go);
            break;

            case BORDER_PATTERN:
            circle_border_pattern(go);
            break;

            default:
            assert(go->gs.fill < BORDER_MAX);
            break;
        }
    }
}

// draw rectangle border
void dap_draw_rectangle_border(GRAPH_OBJ *go) {

    assert(go != NULL);
    if (go->gtype == TYPE_RECTANGLE) {

        switch(go->gs.border)
        {
            case BORDER_SOLID:
            rect_border_solid(go);
            break;

            case BORDER_DASH:
            rect_border_dash(go);
            break;

            case BORDER_PATTERN:
            rect_border_pattern(go);
            break;

            default:
            assert(go->gs.fill < BORDER_MAX);
            break;
        }
    }
}

// draw a circle
void dap_draw_circle(GRAPH_OBJ *go) {
    assert(go != NULL);
    dap_draw_circle_fill(go);
    dap_draw_circle_border(go);
}

// draw a rectangle
void dap_draw_rectangle(GRAPH_OBJ *go) {
    assert(go != NULL);
    dap_draw_rectangle_fill(go);
    dap_draw_rectangle_border(go);
}




void shutdown() {
    // quit
}

int main()  {

    int r;
    bool running = true;

    ALLEGRO_DISPLAY *display = NULL;
    ALLEGRO_EVENT_QUEUE *q;
    ALLEGRO_TIMER *timer;
    ALLEGRO_EVENT event;

    atexit(shutdown);

    al_init();
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

    printf("size of GRAPH_OBJ = %ld\n", sizeof(GRAPH_OBJ));

    // set defaults
    dap_set_graph_style_clip(&g, false);
    dap_set_graph_type(&g, TYPE_RECTANGLE);
    dap_set_graph_color(&g, false, C585NM, BLACK);
    dap_set_graph_style(&g, BORDER_PATTERN, FILL_VERTBARS, 0xFF00);

    // clear display
    al_set_target_backbuffer(display);
    al_clear_to_color(BLACK);

    // draw a horizontal line
    dap_set_graph_style_border(&g, LINE_PATTERN);
    dap_set_line(&g, 10, 10, 200, 10);
    dap_draw_line(&g);
    // draw a vertical line
    dap_set_line(&g, 200, 10, 200, 250);
    dap_draw_line(&g);
    // draw diagonal line
    dap_set_line(&g, 200, 250, 10, 10);
    dap_draw_line(&g);

    // draw rectangle fill
    dap_set_graph_style_fill(&g, FILL_VERTBARS);
    dap_set_rectangle(&g, 10, 550, 200, 600);
    dap_draw_rectangle_fill(&g);

    dap_set_graph_style_fill(&g, FILL_PATTERN);
    dap_set_rectangle(&g, 210, 550, 400, 600);
    dap_draw_rectangle_fill(&g);

    dap_set_graph_style_fill(&g, FILL_SOLID);
    dap_set_rectangle(&g, 410, 550, 600, 600);
    dap_draw_rectangle_fill(&g);

    // draw circle fill
    dap_set_graph_style_fill(&g, FILL_VERTBARS);
    dap_set_circle(&g, 60, 300, 50);
    dap_draw_circle_fill(&g);

    dap_set_graph_style_fill(&g, FILL_PATTERN);
    dap_set_circle(&g, 200, 300, 50);
    dap_draw_circle_fill(&g);

    dap_set_graph_style_fill(&g, FILL_SOLID);
    dap_set_circle(&g, 350, 300, 50);
    dap_draw_circle_fill(&g);

    // draw circle borders
    dap_set_graph_style_border(&g, BORDER_DASH);
    dap_set_circle(&g, 60, 450, 50);
    dap_draw_circle_border(&g);

    dap_set_graph_style_border(&g, BORDER_SOLID);
    dap_set_circle(&g, 200, 450, 50);
    dap_draw_circle_border(&g);

    dap_set_graph_style_border(&g, BORDER_PATTERN);
    dap_set_circle(&g, 350, 450, 50);
    dap_draw_circle_border(&g);

    // draw a solid rectangles
    dap_set_graph_style_border(&g, BORDER_SOLID);
    dap_set_rectangle(&g, 410, 650, 600, 700);
    dap_draw_rectangle_border(&g);

    dap_set_graph_style_border(&g, BORDER_DASH);
    dap_set_rectangle(&g, 10, 650, 200, 700);
    dap_draw_rectangle_border(&g);

    dap_set_graph_style_border(&g, BORDER_PATTERN);
    dap_set_rectangle(&g, 210, 650, 400, 700);
    dap_draw_rectangle_border(&g);

    // draw a rectangle
    dap_set_graph_style(&g, BORDER_PATTERN, FILL_PATTERN, 0xFF00);
    dap_set_rectangle(&g, 700, 400, 900, 500);
    dap_draw_rectangle(&g);

    // draw a circle
    dap_set_graph_style(&g, BORDER_PATTERN, FILL_PATTERN, 0xFF00);
    dap_set_circle(&g, 700, 200, 150);
    dap_draw_circle(&g);

    // get raster data and map into memory
    r = dap_set_raster_file(&g, RASTER_FILE, 0, 725, 512);
    if (r == -1) {
        printf("Could not open raster file\n");
    }

    // close the raster file, data is in memory
    r =  dap_close_raster_file(&g);
    if (r == -1) {
        printf("Could not close raster file\n");
    }

    // draw raster pattern
    r = dap_draw_raster(&g);
    if (r == -1) {
        printf("Nothing to draw\n");
    }


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
    al_destroy_event_queue(q);
    al_destroy_display(display);
    al_uninstall_keyboard();
    al_rest(5.0);
    al_uninstall_system();


    return 0;
}

