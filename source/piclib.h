//code is written by creckeryop
#ifndef PICLIB_H
#define PICLIB_H

#include <vita2d.h>

vita2d_texture *load_PIC_file(const char *filename, int x, int y, int width, int height);
void get_PIC_resolution(const char *filename, int *dest_width, int *dest_height);

#endif