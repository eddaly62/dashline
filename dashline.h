#ifndef _DASHLINE_H
#define _DASHLINE_H

#ifdef __cplusplus
extern "C" {
#endif

// dashline.h

#include <allegro5/allegro.h>

// color definitions
#define BLACK   (al_map_rgb(0, 0, 0))
#define WHITE   (al_map_rgb(200, 200, 200))
#define RED     (al_map_rgb(255, 0, 0))
#define BLUE    (al_map_rgb(0, 0, 255))
#define GREEN   (al_map_rgb(0, 255, 0))
// retro colors of older terminals
#define AMBER   (al_map_rgb(255,176,0))
#define LT_AMBER    (al_map_rgb(255,204,0))
#define APPLE2  (al_map_rgb(51,255,51))
#define APPLE2C (al_map_rgb(102,255,102))
#define GREEN1  (al_map_rgb(51,255,0))
#define GREEN2  (al_map_rgb(0,255,51))
#define GREEN3  (al_map_rgb(0,255,102))
#define C585NM  (al_map_rgb(255,140,23))


// window defaults
#define DEFAULT_WINDOW_BGCOLOR  BLACK
#define DEFAULT_WINDOW_FGCOLOR  C585NM
#define DEFAULT_WINDOW_HOME_X   0
#define DEFAULT_WINDOW_HOME_Y   0
#define DEFAULT_WINDOW_SCALE    2
#define DEFAULT_WINDOW_STYLE    NO_STYLE
#define DEFAULT_WINDOW_BLINKRATE BLINK_MASK_1


// window flags
#define DEFAULT_WINDOW_FLAGS (ALLEGRO_NOFRAME)



#ifdef __cplusplus
}
#endif

#endif