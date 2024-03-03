#include <u.h>
#include <libc.h>

#include <draw.h>
#include <cursor.h>
#include <event.h>
#include <bio.h>
/* #include <ttf.h> */

#define PLAN9
#include "game.h"

#define FACTOR 100
#define WIDTH (16*FACTOR)
#define HEIGHT (9*FACTOR)

#define color2color(color) \
 (((color>>0)&0xff)<<24)| \
 (((color>>8)&0xff)<<16)| \
 (((color>>16)&0xff)<<8)| \
 (((color>>24)&0xff)<<0)

static int xr, yr;
//static TTFont *ttf_font;

void
platform_fill_rect(i32 x, i32 y, i32 w, i32 h, u32 color)
{
  Image *tmp_img = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1, color2color(color));
  draw(screen, Rect(xr+x, yr+y, xr+x+w, yr+y+h), tmp_img, nil, ZP);
}

void
platform_stroke_rect(i32 x, i32 y, i32 w, i32 h, u32 color)
{
  Image *tmp_img = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1, color2color(color));
  line(screen, Pt(x+xr, y+yr), Pt(x+xr+w, y+h+yr), 0, 0, 0, tmp_img, ZP);
  line(screen, Pt(x+xr, y+yr), Pt(x+xr, y+h+yr), 0, 0, 0, tmp_img, ZP);
  line(screen, Pt(x+w+xr, y+yr), Pt(x+xr+w, y+h+yr), 0, 0, 0, tmp_img, ZP);
  line(screen, Pt(x+xr, y+h+yr), Pt(x+xr+w, y+h+yr), 0, 0, 0, tmp_img, ZP);
}

u32
platform_text_width(const char *text, u32 size)
{
  return strlen(text) * 8;
}

void
platform_fill_text(i32 x, i32 y, const char *text, u32 fontSize, u32 color)
{
  Image *tmp_img = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1, color2color(color));
  Point pt = string(screen, Pt(xr + x, yr + y), tmp_img, ZP, font, text);
}

void
platform_panic(const char *file_path, i32 line, const char *message)
{
  sysfatal("%s:%d: GAME ASSERTION FAILED: %s\n", file_path, line, message);
}

void
platform_log(const char *message)
{
  (void)message;
}

void
eresized(int new)
{
  if (new && getwindow(display, Refnone) < 0)
    fprint(1, "can't reattach window");

  xr = screen->r.min.x, yr = screen->r.min.y;
}

void
resize(int x, int y, int w, int h)
{
  int wctl = open("/dev/wctl", ORDWR);
  fprint(wctl, "resize -r %d %d %d %d\n", x, y, x+w, x+h);
  close(wctl);

  eresized(69);
}

int
main(int argc, char **argv)
{
  if (initdraw(nil, nil, argv0) < 0)
    perror(argv0);

  einit(Ekeyboard);

  resize(100, 100, WIDTH, HEIGHT);
  game_init(WIDTH, HEIGHT);

  vlong last = nsec();
  float dt = 0;
  while (1) {
    last = nsec();
    if (ecankbd()) {
      int k = ekbd();
      if (k == 'q')
        exits(argv0);
      game_keydown(k);
    }

    game_update(dt);
    game_render();
    /*
     char s[512];
     sprint(s, "%f", dt);
     platform_fill_text(200, 200, s, 1, 0xffffffff);
    */
    flushimage(display, Refnone);
    dt = (nsec() - last)/1000000000.0;
  }

  return 0;
}
