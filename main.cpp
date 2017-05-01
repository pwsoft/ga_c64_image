
#include <SDL/SDL.h>
#include "lodepng/lodepng.h"

extern "C" long random();

#define STOCHASTIC
//#define DITHER_1X2
//#define DITHER_2X2
#define DITHER_3X3
#define INTERLACE
//#define DELTA_X
//#define DELTA_Y
//#define SPRITES
//#define REMAP_PALETTE

static SDL_Surface *ui_theScreen;
static int theQuitFlag;

static int window_xsize = 1024;
static const int column_width = 8;
static const int nr_columns = 320 / column_width;
static const int nr_rows = 25;
static const int sprite_columns = 24; // 8 *3 bytes
static const int sprite_rows = 100;   // vertical stretched. Not enough memory otherwise
static const int sprite_size = sprite_columns * sprite_rows;
static const int sprite_color = 0;
static const int regen_size = 1024;
static const int pixel_size = 8192;
static const int max_pop = 1;
static const int frame_shift_y = 0;

typedef uint32_t color_t;

typedef struct {
    int r;
    int g;
    int b;
} rgb_t;

typedef struct {
    int error[nr_columns];
    unsigned char sprite_1[sprite_size];
    unsigned char sprite_2[sprite_size];
    unsigned char regen_1[regen_size];
    unsigned char pixel_1[pixel_size];
    unsigned char regen_2[regen_size];
    unsigned char pixel_2[pixel_size];
} c64_mem_t;


static rgb_t flash_320[200][320]; // Difference between the interlace screen, causing flickering. Delta should be small
static rgb_t c64_320[200][320];
static rgb_t image_320[200][320];
static rgb_t diffh_320[200][320];
static rgb_t diffv_320[200][320];
static rgb_t image_160[100][160];
static rgb_t image_80[50][80];
static rgb_t image_40[25][40];
static rgb_t c64_palette[16] = {
    {0x00,0x00,0x00},
    {0xFF,0xFF,0xFF},
	{0x96,0x28,0x2E},
	{0x5B,0xD6,0xCE},
	{0x9F,0x2D,0xAD},
	{0x41,0xB9,0x36},
	{0x27,0x24,0xC4},
	{0xEF,0xF3,0x47},
	{0x9F,0x48,0x15},
	{0x5E,0x35,0x00},
	{0xDA,0x5F,0x66},
	{0x47,0x47,0x47},
	{0x78,0x78,0x78},
	{0x91,0xFF,0x84},
	{0x68,0x64,0xFF},
	{0xAE,0xAE,0xAE}};
static rgb_t palette[16];
static c64_mem_t pop[max_pop];


static void ui_draw(void) {
	color_t *q = (color_t *)ui_theScreen->pixels;
	SDL_LockSurface(ui_theScreen);

	for(int y=0; y<200; y++) {
		for(int x=0; x<320; x++) {
            *q = SDL_MapRGB(ui_theScreen->format, image_320[y][x].r, image_320[y][x].g, image_320[y][x].b);
            q++;
		}
//		for(int x=0; x<320; x++) {
//            *q = (diffh_320[y][x].r<<16) | (diffh_320[y][x].g<<8) | diffh_320[y][x].b;
//            q++;
//		}
//		for(int x=0; x<320; x++) {
//            *q = (diffv_320[y][x].r<<16) | (diffv_320[y][x].g<<8) | diffv_320[y][x].b;
//            q++;
//		}
//		q += window_xsize-960;
		q += window_xsize-320;
	}

	q += window_xsize * 16;

	for(int y=0; y<400; y++) {
		for(int x=0; x<640; x++) {
			*q = SDL_MapRGB(ui_theScreen->format, c64_320[y/2][x/2].r, c64_320[y/2][x/2].g, c64_320[y/2][x/2].b);
            q++;
		}
		q += window_xsize-640;
	}

#ifdef REMAP_PALETTE
	// Draw remapped palette based on image data next to original C64 palette
	q = (color_t *)ui_theScreen->pixels;
	q += 216 * window_xsize + 700;
	for(int i=0; i<16; i++) {

		for(int y=0; y<16; y++) {
			for(int x=0; x<16; x++) {
				*q = SDL_MapRGB(ui_theScreen->format, c64_palette[i].r, c64_palette[i].g, c64_palette[i].b);
				q++;
			}
			q += 16;
			for(int x=0; x<16; x++) {
				*q = (palette[i].r<<16) | (palette[i].g<<8) | (palette[i].b);
				q++;
			}
			q += window_xsize-48;
		}
	}
#endif

	SDL_LockSurface(ui_theScreen);
	SDL_UpdateRect(ui_theScreen, 0, 0, 0, 0);
}

static const int scale_1x1 = 8;
static const int scale_flash = 0;
static const int scale_deltax = 1;
//static const int scale_diffh = 100;
//static const int scale_diffv = 100;
static const int scale_1x2 = 8;
static const int scale_1x2_gray = 0;
static const int scale_2x2 = 8; // 8
static const int scale_2x2_gray = 0;
static const int scale_3x3 = 8;
static const int scale_flash_2x2 = 0;
static void eval_c64(c64_mem_t *img) {
    for(int i=0; i<nr_columns; i++) {
        img->error[i] = 0;
    }

    for(int y=0; y<200; y++) {
        for(int x=0; x<320; x++) {
            int err_1x1 = 0;
//            int err_flash = 0;
            err_1x1 += abs(image_320[y][x].r - c64_320[y][x].r);
            err_1x1 += abs(image_320[y][x].g - c64_320[y][x].g);
            err_1x1 += abs(image_320[y][x].b - c64_320[y][x].b);
//            err_flash += abs(flash_320[y][x].r);
//            err_flash += abs(flash_320[y][x].g);
//            err_flash += abs(flash_320[y][x].b);
//            img->error[(x/8)+(y/8)*40] += err_1x1*scale_1x1 + err_flash*scale_flash;
            img->error[x/column_width] += err_1x1*scale_1x1;
        }
    }

#ifdef DELTA_X
    for(int y=0; y<200; y++) {
        for(int x=0; x<319; x++) {
            int err_deltax = 0;

//            err_deltax += abs(0 - (c64_320[y][x].r-c64_320[y][x+1].r));
            err_deltax += abs((image_320[y][x].r-image_320[y][x+1].r) - (c64_320[y][x].r-c64_320[y][x+1].r));
            err_deltax += abs((image_320[y][x].g-image_320[y][x+1].g) - (c64_320[y][x].g-c64_320[y][x+1].g));
            err_deltax += abs((image_320[y][x].b-image_320[y][x+1].b) - (c64_320[y][x].b-c64_320[y][x+1].b));

            img->error[x/column_width] += err_deltax*scale_deltax;
		}
	}
#endif
#ifdef DELTA_Y
#endif

#if 0
    // 16 Pixel average
    for(int y=0; y<196; y++) {
        for(int x=0; x<316; x++) {
            int err = 0;
            int rs = 0;
            int gs = 0;
            int bs = 0;
            int rc = 0;
            int gc = 0;
            int bc = 0;
            for(int dy=0; dy<4; dy++) {
                for(int dx=0; dx<4; dx++) {
                    rs += image_320[y+dy][x+dx].r;
                    gs += image_320[y+dy][x+dx].g;
                    bs += image_320[y+dy][x+dx].b;
                    rc += c64_320[y+dy][x+dx].r;
                    gc += c64_320[y+dy][x+dx].g;
                    bc += c64_320[y+dy][x+dx].b;
                }
            }
            err += abs(rs - rc);
            err += abs(gs - gc);
            err += abs(bs - bc);
            img->error[(x/column_width)+(y/8)*40] += err;
        }
    }
#endif
#if 1
    // 4 Pixel average
    for(int y=0; y<199; y++) {
        for(int x=0; x<319; x++) {
#ifdef DITHER_1X2
            {
            int err = 0;
            const int rs = image_320[y][x].r + image_320[y+1][x].r + image_320[y+1][x+1].r;
            const int gs = image_320[y][x].g + image_320[y+1][x].g + image_320[y+1][x+1].g;
            const int bs = image_320[y][x].b + image_320[y+1][x].b + image_320[y+1][x+1].b;
            const int rc = c64_320[y][x].r + c64_320[y+1][x].r;
            const int gc = c64_320[y][x].g + c64_320[y+1][x].g;
            const int bc = c64_320[y][x].b + c64_320[y+1][x].b;
            err += abs(rs - rc);
            err += abs(gs - gc);
            err += abs(bs - bc);
            img->error[x/column_width] += (err * scale_1x2) / 2;
            img->error[x/column_width] += abs((rs+gs+bs) - (rc+gc+bc)) * scale_1x2_gray / 2;
            }
#endif
#ifdef DITHER_2X2
            {
            int err = 0;
            const int rs = image_320[y][x].r + image_320[y][x+1].r + image_320[y+1][x].r + image_320[y+1][x+1].r;
            const int gs = image_320[y][x].g + image_320[y][x+1].g + image_320[y+1][x].g + image_320[y+1][x+1].g;
            const int bs = image_320[y][x].b + image_320[y][x+1].b + image_320[y+1][x].b + image_320[y+1][x+1].b;
            const int rc = c64_320[y][x].r + c64_320[y][x+1].r + c64_320[y+1][x].r + c64_320[y+1][x+1].r;
            const int gc = c64_320[y][x].g + c64_320[y][x+1].g + c64_320[y+1][x].g + c64_320[y+1][x+1].g;
            const int bc = c64_320[y][x].b + c64_320[y][x+1].b + c64_320[y+1][x].b + c64_320[y+1][x+1].b;
            err += abs(rs - rc);
            err += abs(gs - gc);
            err += abs(bs - bc);
            img->error[x/column_width] += (err * scale_2x2) / 4;
            img->error[x/column_width] += abs((rs+gs+bs) - (rc+gc+bc)) * scale_2x2_gray / 4;
            }
#endif
#ifdef DITHER_3X3
            {
            int err = 0;
            const int rs = image_320[y][x].r + image_320[y][x+1].r + image_320[y][x+2].r
                + image_320[y+1][x].r + image_320[y+1][x+1].r + image_320[y+1][x+2].r
                + image_320[y+2][x].r + image_320[y+2][x+1].r + image_320[y+2][x+2].r;
            const int gs = image_320[y][x].g + image_320[y][x+1].g + image_320[y][x+2].g
                + image_320[y+1][x].g + image_320[y+1][x+1].g + image_320[y+1][x+2].g
                + image_320[y+2][x].g + image_320[y+2][x+1].g + image_320[y+2][x+2].g;
            const int bs = image_320[y][x].b + image_320[y][x+1].b + image_320[y][x+2].b
                + image_320[y+1][x].b + image_320[y+1][x+1].b + image_320[y+1][x+2].b
                + image_320[y+2][x].b + image_320[y+2][x+1].b + image_320[y+2][x+2].b;
            const int rc = c64_320[y][x].r + c64_320[y][x+1].r + c64_320[y][x+2].r
                + c64_320[y+1][x].r + c64_320[y+1][x+1].r + c64_320[y+1][x+2].r
                + c64_320[y+2][x].r + c64_320[y+2][x+1].r + c64_320[y+2][x+2].r;
            const int gc = c64_320[y][x].g + c64_320[y][x+1].g + c64_320[y][x+2].g
                + c64_320[y+1][x].g + c64_320[y+1][x+1].g + c64_320[y+1][x+2].g
                + c64_320[y+2][x].g + c64_320[y+2][x+1].g + c64_320[y+2][x+2].g;
            const int bc = c64_320[y][x].b + c64_320[y][x+1].b + c64_320[y][x+2].b
                + c64_320[y+1][x].b + c64_320[y+1][x+1].b + c64_320[y+1][x+2].b
                + c64_320[y+2][x].b + c64_320[y+2][x+1].b + c64_320[y+2][x+2].b;
            err += abs(rs - rc);
            err += abs(gs - gc);
            err += abs(bs - bc);
            img->error[x/column_width] += (err * scale_3x3) / 9;
            }
#endif

#if 0
            int flash_r = flash_320[y][x].r + flash_320[y][x+1].r + flash_320[y+1][x].r + flash_320[y+1][x+1].r;
            int flash_g = flash_320[y][x].g + flash_320[y][x+1].g + flash_320[y+1][x].g + flash_320[y+1][x+1].g;
            int flash_b = flash_320[y][x].b + flash_320[y][x+1].b + flash_320[y+1][x].b + flash_320[y+1][x+1].b;
            img->error[x/column_width] += abs(flash_r) * scale_flash_2x2;
            img->error[x/column_width] += abs(flash_g) * scale_flash_2x2;
            img->error[x/column_width] += abs(flash_b) * scale_flash_2x2;
#endif
        }
    }
#endif
#if 0
    err_320 *= 4;

    // Line accumulated error
    for(int y=0; y<200; y++) {
        int64_t line_err_r = 0;
        int64_t line_err_g = 0;
        int64_t line_err_b = 0;
        for(int x=0; x<320; x++) {
            line_err_r += image_320[y][x].r - c64_320[y][x].r;
            line_err_g += image_320[y][x].g - c64_320[y][x].g;
            line_err_b += image_320[y][x].b - c64_320[y][x].b;
        }
        err_320 += abs(line_err_r);
        err_320 += abs(line_err_g);
        err_320 += abs(line_err_b);
    }

    // Column accumulated error
    for(int x=0; x<320; x++) {
        int64_t col_err_r = 0;
        int64_t col_err_g = 0;
        int64_t col_err_b = 0;
        for(int y=0; y<200; y++) {
            col_err_r += image_320[y][x].r - c64_320[y][x].r;
            col_err_g += image_320[y][x].g - c64_320[y][x].g;
            col_err_b += image_320[y][x].b - c64_320[y][x].b;
        }
        err_320 += abs(col_err_r);
        err_320 += abs(col_err_g);
        err_320 += abs(col_err_b);
    }
#endif
#if 0
    for(int y=0; y<200; y++) {
        for(int x=0; x<319; x++) {
            int err = 0;
            err += abs(diffh_320[y][x].r - abs(c64_320[y][x].r - c64_320[y][x+1].r));
            err += abs(diffh_320[y][x].g - abs(c64_320[y][x].g - c64_320[y][x+1].g));
            err += abs(diffh_320[y][x].b - abs(c64_320[y][x].b - c64_320[y][x+1].b));
            img->error[(x/8)+(y/8)*40] += err * scale_diffh;
        }
    }
#endif
#if 0
    for(int y=0; y<199; y++) {
        for(int x=0; x<320; x++) {
            int err = 0;
            err += abs(diffh_320[y][x].r - abs(c64_320[y][x].r - c64_320[y+1][x].r));
            err += abs(diffh_320[y][x].g - abs(c64_320[y][x].g - c64_320[y+1][x].g));
            err += abs(diffh_320[y][x].b - abs(c64_320[y][x].b - c64_320[y+1][x].b));
            img->error[(x/8)+(y/8)*40] += err * scale_diffv;
        }
    }
#endif
#if 0
    for(int y=0; y<100; y++) {
        for(int x=0; x<160; x++) {
            int r = c64_320[y*2][x*2].r + c64_320[y*2+1][x*2].r + c64_320[y*2][x*2+1].r + c64_320[y*2+1][x*2+1].r;
            int g = c64_320[y*2][x*2].g + c64_320[y*2+1][x*2].g + c64_320[y*2][x*2+1].g + c64_320[y*2+1][x*2+1].g;
            int b = c64_320[y*2][x*2].b + c64_320[y*2+1][x*2].b + c64_320[y*2][x*2+1].b + c64_320[y*2+1][x*2+1].b;
            err_160 += abs(image_160[y][x].r - r/4);
            err_160 += abs(image_160[y][x].g - g/4);
            err_160 += abs(image_160[y][x].b - b/4);
        }
    }
#endif
#if 0
    for(int y=0; y<25; y++) {
        for(int x=0; x<40; x++) {
            int r=0;
            int g=0;
            int b=0;
            for(int dy=0; dy<8; dy++) {
                for(int dx=0; dx<8; dx++) {
                    r += c64_320[y*8+dy][x*8+dx].r;
                    g += c64_320[y*8+dy][x*8+dx].g;
                    b += c64_320[y*8+dy][x*8+dx].b;
                }
            }
            err_40 += abs(image_40[y][x].r - r/64);
            err_40 += abs(image_40[y][x].g - g/64);
            err_40 += abs(image_40[y][x].b - b/64);
        }
    }
#endif

#if 0
    return err_320*err_320_scale + err_h*err_h_scale + err_v*err_v_scale
        + err_160*err_160_scale + err_40*err_40_scale;
#endif
}

static void gen_c64(c64_mem_t *img) {
    for(int y=0; y<200; y++) {
        for(int x=0; x<320; x++) {
            const int shift_y = y + frame_shift_y;
            const int color_1 = img->regen_1[(x/8)+(y/8)*40];
            const int pixel_1 = img->pixel_1[(y/8)*320 + (y&7) + (x&(~7))]&(1<<(7-(x&7)));
#ifdef SPRITES
            const int sprite_1 = img->sprite_1[(x/16) + (y/2)*sprite_columns] & (1<<(7-((x/2)&7)));
            const int sprite_2 = img->sprite_2[(x/16) + (y/2)*sprite_columns] & (1<<(7-((x/2)&7)));
#endif
            const int color_2 = img->regen_2[(x/8)+((shift_y)/8)*40];
            const int pixel_2 = img->pixel_2[((shift_y)/8)*320 + ((shift_y)&7) + (x&(~7))]&(1<<(7-(x&7)));
#ifdef SPRITES
            const rgb_t *rgb_1 = &palette[pixel_1?(color_1>>4):(sprite_1?sprite_color:(color_1&15))];
            const rgb_t *rgb_2 = &palette[pixel_2?(color_2>>4):(sprite_2?sprite_color:(color_2&15))];
#else
            const rgb_t *rgb_1 = &palette[pixel_1?(color_1>>4):(color_1&15)];
            const rgb_t *rgb_2 = &palette[pixel_2?(color_2>>4):(color_2&15)];
#endif // SRPITES
#ifdef INTERLACE
            c64_320[y][x].r = (rgb_1->r + rgb_2->r) / 2;
            c64_320[y][x].g = (rgb_1->g + rgb_2->g) / 2;
            c64_320[y][x].b = (rgb_1->b + rgb_2->b) / 2;
            flash_320[y][x].r = rgb_1->r - rgb_2->r;
            flash_320[y][x].g = rgb_1->g - rgb_2->g;
            flash_320[y][x].b = rgb_1->b - rgb_2->b;
#else
            c64_320[y][x].r = rgb_1->r;
            c64_320[y][x].g = rgb_1->g;
            c64_320[y][x].b = rgb_1->b;
            flash_320[y][x].r = 0;
            flash_320[y][x].g = 0;
            flash_320[y][x].b = 0;
#endif
        }
    }
}


static void load_scale_picture() {
	unsigned char *image = NULL;
	unsigned width = 0;
	unsigned height = 0;
//	lodepng_decode32_file(&image, &width, &height, "birds_hd.png");
	lodepng_decode32_file(&image, &width, &height, "monster.png");

    if (image) {
        for(int y=0; y<200; y++) {
            for(int x=0; x<320; x++) {
                int r_sum = 0;
                int g_sum = 0;
                int b_sum = 0;
                for(int dy=0; dy<6; dy++) {
                    for(int dx=0; dx<6; dx++) {
                        const int offs = 4*((y*6+dy)*width+(x*6+dx));
                        const int r = image[offs];
                        const int g = image[offs + 1];
                        const int b = image[offs + 2];
                        r_sum += r;
                        g_sum += g;
                        b_sum += b;
                    }
                }
                image_320[y][x].r = r_sum / 36;
                image_320[y][x].g = g_sum / 36;
                image_320[y][x].b = b_sum / 36;
            }
        }

        for(int y=0; y<200; y++) {
            for(int x=0; x<319; x++) {
                diffh_320[y][x].r = abs(image_320[y][x].r - image_320[y][x+1].r);
                diffh_320[y][x].g = abs(image_320[y][x].g - image_320[y][x+1].g);
                diffh_320[y][x].b = abs(image_320[y][x].b - image_320[y][x+1].b);
            }
        }
        for(int y=0; y<199; y++) {
            for(int x=0; x<320; x++) {
                diffv_320[y][x].r = abs(image_320[y][x].r - image_320[y+1][x].r);
                diffv_320[y][x].g = abs(image_320[y][x].g - image_320[y+1][x].g);
                diffv_320[y][x].b = abs(image_320[y][x].b - image_320[y+1][x].b);
            }
        }

        for(int y=0; y<100; y++) {
            for(int x=0; x<160; x++) {
                int r_sum = 0;
                int g_sum = 0;
                int b_sum = 0;
                for(int dy=0; dy<12; dy++) {
                    for(int dx=0; dx<12; dx++) {
                        const int offs = 4*((y*12+dy)*width+(x*12+dx));
                        const int r = image[offs];
                        const int g = image[offs + 1];
                        const int b = image[offs + 2];
                        r_sum += r;
                        g_sum += g;
                        b_sum += b;
                    }
                }
                image_160[y][x].r = r_sum / 144;
                image_160[y][x].g = g_sum / 144;
                image_160[y][x].b = b_sum / 144;
            }
        }

        for(int y=0; y<25; y++) {
            for(int x=0; x<40; x++) {
                int r_sum = 0;
                int g_sum = 0;
                int b_sum = 0;
                for(int dy=0; dy<48; dy++) {
                    for(int dx=0; dx<48; dx++) {
                        const int offs = 4*((y*48+dy)*width+(x*48+dx));
                        const int r = image[offs];
                        const int g = image[offs + 1];
                        const int b = image[offs + 2];
                        r_sum += r;
                        g_sum += g;
                        b_sum += b;
                    }
                }
                image_40[y][x].r = r_sum / 2304;
                image_40[y][x].g = g_sum / 2304;
                image_40[y][x].b = b_sum / 2304;
            }
        }

        free(image);
    }
}
#if 0
static void combine(c64_mem_t *img) {
    int index1 = random() % max_pop;
    int index2 = random() % max_pop;
    for(int i=0; i<regen_size; i++) {
        img->regen_1[i] = (random()&1)?pop[index1].regen_1[i]:pop[index2].regen_1[i];
    }
    for(int i=0; i<pixel_size; i++) {
        img->pixel_1[i] = (random()&1)?pop[index1].pixel_1[i]:pop[index2].pixel_1[i];
    }
}

static void combine_harm(c64_mem_t *img) {
    for(int i=0; i<regen_size; i++) {
        int index = random() % max_pop;
        index = random() % (index+1);
        img->regen[i] = pop[index].regen[i];
        for(int j=0; j<8; j++) {
            img->pixel[i*8+j] = pop[index].pixel[i*8+j];
        }
    }
}
#endif

static void mutate(c64_mem_t *img, int mutate_count) {
    for(int i=0; i<mutate_count/20; i++) {
        img->regen_1[random() % regen_size] ^= (random() & 15);
        img->regen_1[random() % regen_size] ^= (random() & 240);
        img->regen_2[random() % regen_size] ^= (random() & 15);
        img->regen_2[random() % regen_size] ^= (random() & 240);
    }

    for(int i=0; i<mutate_count; i++) {
        img->pixel_1[random() % pixel_size] ^= (1<<(random()&7));
        img->pixel_2[random() % pixel_size] ^= (1<<(random()&7));
        img->sprite_1[random() % sprite_size] ^= (1<<(random()&7));
        img->sprite_2[random() % sprite_size] ^= (1<<(random()&7));
    }
}

static void write_c64_regen_1(c64_mem_t *img) {
    FILE *f = fopen("bird_regen_1.asm", "wb");

    if (f) {
        fprintf(f, "\n\nbird_regen_1:\n");
        for(int i=0; i<regen_size; i+=16) {
            fprintf(f, "\t.byte\t");
            for(int j=0; j<16; j++) {
                if (j>0) { fprintf(f, ","); }
                fprintf(f, "%d", img->regen_1[i+j]);
            }
            fprintf(f, "\n");
        }

        fclose(f);
    }
}

static void write_c64_regen_2(c64_mem_t *img) {
    FILE *f = fopen("bird_regen_2.asm", "wb");

    if (f) {
        fprintf(f, "\n\nbird_regen_2:\n");
        for(int i=0; i<regen_size; i+=16) {
            fprintf(f, "\t.byte\t");
            for(int j=0; j<16; j++) {
                if (j>0) { fprintf(f, ","); }
                fprintf(f, "%d", img->regen_2[i+j]);
            }
            fprintf(f, "\n");
        }

        fclose(f);
    }
}

static void write_c64_pixel_1(c64_mem_t *img) {
    FILE *f = fopen("bird_pixel_1.asm", "wb");

    if (f) {
        fprintf(f, "\n\nbird_pixel_1:\n");
        for(int i=0; i<pixel_size; i+=16) {
            fprintf(f, "\t.byte\t");
            for(int j=0; j<16; j++) {
                if (j>0) { fprintf(f, ","); }
                fprintf(f, "%d", img->pixel_1[i+j]);
            }
            fprintf(f, "\n");
        }

        fclose(f);
    }
}

static void write_c64_pixel_2(c64_mem_t *img) {
    FILE *f = fopen("bird_pixel_2.asm", "wb");

    if (f) {
        fprintf(f, "\n\nbird_pixel_2:\n");
        for(int i=0; i<pixel_size; i+=16) {
            fprintf(f, "\t.byte\t");
            for(int j=0; j<16; j++) {
                if (j>0) { fprintf(f, ","); }
                fprintf(f, "%d", img->pixel_2[i+j]);
            }
            fprintf(f, "\n");
        }

        fclose(f);
    }
}

static void write_c64_code(c64_mem_t *img) {
    write_c64_regen_1(img);
    write_c64_regen_2(img);
    write_c64_pixel_1(img);
    write_c64_pixel_2(img);
}

static void random_img(c64_mem_t *img) {
    for(int i=0; i<sizeof(c64_mem_t); i++) {
            ((unsigned char *)img)[i] = random();
    }
}

static void remove_dupicate_color(c64_mem_t *img) {
    for(int i=0; i<regen_size; i++) {
        int again = 0;
        do {
            again = 0;
            const int color_1 = img->regen_1[i];
            const int color_2 = img->regen_2[i];
            if ((color_1>>4) == (color_1&15)) {
                img->regen_1[i] = random();
                again = 1;
            }
            if ((color_2>>4) == (color_2&15)) {
                img->regen_2[i] = random();
                again = 1;
            }
#ifdef SPRITES
            if ((color_1>>4) == sprite_color) {
                img->regen_1[i] = random();
                again = 1;
            }
            if ((color_1&15) == sprite_color) {
                img->regen_1[i] = random();
                again = 1;
            }
            if ((color_2>>4) == sprite_color) {
                img->regen_2[i] = random();
                again = 1;
            }
            if ((color_2&15) == sprite_color) {
                img->regen_2[i] = random();
                again = 1;
            }

#endif // SPRITES
        } while(again);
    }
}

// Search index with largest error an nuke it. Hopefully a new solution comes up.
static void nuke_largest(c64_mem_t *img) {
    int largest = 0;
    int index = 0;
    for(int i=0; i<nr_columns; i++) {
        if (img->error[i] > largest) {
            largest = img->error[i];
            index = i;
        }
    }

    img->error[index] = 100000000;
}

static int iter;
static int iter_write;
static int mutate_rate = 50000;
static void step() {
    iter++;
    iter_write++;

#if 1
    c64_mem_t new_img = pop[0];
    // Speedup initial steps and add some random events to overcome local minima
    if ((iter < 10000) || ((iter % 100)==0)) {
        random_img(&new_img);
        //nuke_largest(&pop[0]);
    }
    mutate(&new_img, 50);
    remove_dupicate_color(&new_img);
#else
    c64_mem_t new_img;
    random_img(&new_img);
#endif

    gen_c64(&new_img);
    eval_c64(&new_img);

    int err_sum = 0;
    int err_sum_pop0 = 0;
#if 1
    for(int i=0; i<nr_columns; i++) {
        const int tmp_err = new_img.error[i] - random() % (1+(new_img.error[i] / (iter/10+1)));
#ifdef STOCHASTIC
        err_sum += new_img.error[i];
        if (tmp_err < pop[0].error[i]) {
#else
        err_sum += new_img.error[i];
        if (new_img.error[i] < pop[0].error[i]) {
#endif
            pop[0].error[i] = new_img.error[i];
            for(int y=0; y<nr_rows; y++) {
                pop[0].regen_1[i+y*nr_columns] = new_img.regen_1[i+y*nr_columns];
                pop[0].regen_2[i+y*nr_columns] = new_img.regen_2[i+y*nr_columns];
                for(int j=0; j<8; j++) {
                    pop[0].pixel_1[(i+y*nr_columns)*8+j] = new_img.pixel_1[(i+y*nr_columns)*8+j];
                    pop[0].pixel_2[(i+y*nr_columns)*8+j] = new_img.pixel_2[(i+y*nr_columns)*8+j];
                }
            }
#ifdef SPRITES
            for(int y=0; y<sprite_rows; y++) {
                if (i&1) {
                    pop[0].sprite_1[(i/2) + y*sprite_columns] &= 240;
                    pop[0].sprite_1[(i/2) + y*sprite_columns] |= new_img.sprite_1[(i/2) + y*sprite_columns] & 15;
                    pop[0].sprite_2[(i/2) + y*sprite_columns] &= 240;
                    pop[0].sprite_2[(i/2) + y*sprite_columns] |= new_img.sprite_2[(i/2) + y*sprite_columns] & 15;
                } else {
                    pop[0].sprite_1[(i/2) + y*sprite_columns] &= 15;
                    pop[0].sprite_1[(i/2) + y*sprite_columns] |= new_img.sprite_1[(i/2) + y*sprite_columns] & 240;
                    pop[0].sprite_2[(i/2) + y*sprite_columns] &= 15;
                    pop[0].sprite_2[(i/2) + y*sprite_columns] |= new_img.sprite_2[(i/2) + y*sprite_columns] & 240;
                }
            }
#endif
        }
        err_sum_pop0 += pop[0].error[i];
    }
#else
    for(int i=0; i<nr_columns; i++) {
        err_sum += new_img.error[i];
        err_sum_pop0 += pop[0].error[i];
    }
    if (err_sum < err_sum_pop0) {
        pop[0] = new_img;
    }
#endif



    if ((iter % 100) == 0) {
        gen_c64(&pop[0]);
        ui_draw();
    }
    if (iter_write > 10000) {
        iter_write = 0;
        write_c64_code(&pop[0]);
    }

    char tmp[256];
    sprintf(tmp, "%9d %9d %9d %9d", iter, mutate_rate, err_sum, err_sum_pop0);
    SDL_WM_SetCaption(tmp, NULL);


/*
    int hit = 0;
    //const int mutate_count = mutate_rate + (random()%3000) - 1000;
    const int mutate_count = 10000;

    combine_harm(&new_img);
    mutate(&new_img, mutate_count/1000);
    gen_c64(&new_img);
    new_img.error = eval_c64();
    for(int p=0; p<max_pop; p++) {
        if (new_img.error <= pop[p].error) {
            mutate_rate = (mutate_rate*99 + mutate_count) / 100;
            pop[p] = new_img;
            hit = 1;

            if (p==0) {
                ui_draw();
                char tmp[256];
                sprintf(tmp, "%9d %9d %lld %lld", iter, mutate_rate, new_img.error, pop[max_pop-1].error);
                SDL_WM_SetCaption(tmp, NULL);

                if (iter_write > 10000) {
                    iter_write = 0;
                    write_c64_code(&new_img);
                }
            }
            break;
        }
    }
    if (hit == 0) {
        mutate_rate = (mutate_rate*501 - mutate_count) / 500;
        if (mutate_rate < 2000) {
            mutate_rate = 5000;
        }
    }
*/
}

static inline int diff_color(rgb_t rgb1, rgb_t rgb2) {
	return abs(rgb1.b - rgb2.b) + abs(rgb1.g - rgb2.g) + abs(rgb1.r - rgb2.r);
}

static void remap_palette() {
	int palette_counts[16] = {};
	rgb_t palette_sums[16] = {};

	for(int y=0; y<200; y++) {
		for(int x=0; x<320; x++) {
			rgb_t pixel = image_320[y][x];
			int best_diff = diff_color(pixel, palette[0]);
			int index = 0;
			for(int i=1; i<16; i++) {
				int diff = diff_color(pixel, palette[i]);
				if (diff < best_diff) {
					best_diff = diff;
					index = i;
				}
			}
			palette_sums[index].r += pixel.r;
			palette_sums[index].g += pixel.g;
			palette_sums[index].b += pixel.b;
			palette_counts[index]++;
		}
	}

	for(int i=0; i<16; i++) {
		if (palette_counts[i] > 0) {
			palette[i].r = palette_sums[i].r / palette_counts[i];
			palette[i].g = palette_sums[i].g / palette_counts[i];
			palette[i].b = palette_sums[i].b / palette_counts[i];
		}
	}
}

static void ui_eventloop() {
	while(!theQuitFlag) {
        step();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                return;
            default:
                break;
            }
		}
	}
}

extern "C" int main(int argc, char **argv) {
	ui_theScreen = SDL_SetVideoMode(
		window_xsize, 768,
		32, SDL_SWSURFACE);
    load_scale_picture();

	memcpy(palette, c64_palette, sizeof(c64_palette));
#ifdef REMAP_PALETTE
	remap_palette();
#endif

    for(int p=0; p<max_pop; p++) {
        for(int i=0; i<sizeof(c64_mem_t); i++) {
            ((unsigned char *)&pop[p])[i] = random();
        }
        gen_c64(&pop[p]);
        eval_c64(&pop[p]);
    }
    ui_eventloop();

	return 0;
}
