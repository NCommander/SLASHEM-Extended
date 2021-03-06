/*   SCCS Id: @(#)txt2bmp.c   3.3     95/01/26                     */
/*   Copyright (c) NetHack PC Development Team 1993, 1994, 1995     */
/*   NetHack may be freely redistributed.  See license for details. */

/*
 * This creates a txt2bmp.exe
 *
 * txt2bmp.exe -i generates 'index'
 * txt2bmp.exe -h generates 'allegfx.h' THIS FILE IS OBSOLETE
 * txt2bmp.exe -fXX generates all the *.bmp
 * txt2bmp.exe -bXX generates slamXX.bmp
 * txt2bmp.exe -c txtname bmpname converts txtname to a single bmpname
 * txt2bmp.exe -p bmpname changes the bitmap to a 256 color bmp with 
 *		  the pink as the first color
 *
 * Edit History:
 *
 *      Initial Creation                        K.Youngblood    ??/??/??
 *      added 32x32 text tile support		W.Cheung	00/06/19
 *	added direct write to files		W.Cheung	00/06/21
 *
 */

#include <allegro.h>

#include "hack.h"
#include "pcvideo.h"
#include "tile.h"
#include "pctiles.h"

#include <ctype.h>
#include <dos.h>
#include <stdlib.h>
#include <time.h>

extern char *tilename(int, int);

static int num_colors;
static pixel pixels[MAX_TILE_Y][MAX_TILE_X];
static BITMAP *bigtile_bmp;

static struct tibhdr_struct tibheader;

/* [WAC] Use the 32x32x262 color tiles */

static const char 
    *tilefiles3d[] = {  "../win/share/mon3d.txt",
                        "../win/share/obj3d.txt",
                        "../win/share/oth3d.txt"},
    *tilefiles32[] = {  "../win/share/mon32.txt",
                        "../win/share/obj32.txt",
                        "../win/share/oth32.txt"},
    *tilefiles16[] = {  "../win/share/monsters.txt",
                        "../win/share/objects.txt",
                        "../win/share/other.txt"};

static const char
    *Dont_Edit_Code =
	"/* This source file is generated by 'txt2bmp'.  Do not edit. */\n",
    *Dont_Edit_Index =
	"# This index file was generated by 'txt2bmp'.\n";

static int tilecount;
static int filenum;
static int paletteflag;

#define TRANS_COLOR MASK_COLOR_24

static const char
    *bigtile_file3d = "../src/slam3D.bmp",
    *bigtile_file32 = "../src/slam32.bmp",
    *bigtile_file16 = "../src/slam16.bmp",
    *index_file = "../src/index",
    *allegfx_file = "../include/allegfx.h";

/* #define FINAL_COLORDEPTH_8_BPP */

/* Currently interpolation is OFF */
/* Should handle all sizes tile files now */
void
make_bitmap(pixels, tilecount, trans, filename, make_big)
pixel (*pixels)[MAX_TILE_X];
int tilecount;
int trans;
char *filename;
boolean make_big;
{
        int i, j, x, y;
        long color, color2;
        char cmd[60];
        BITMAP *tilebmp;

        tilebmp = create_bitmap(tile_x, tile_y);

        color2 = 0L;
        color2 |= 71;
        color2 |= (108) << 8;
        color2 |= (108) << 16;

        /* load the origional tile */

        for (j = 0; j < tile_y; j++) {
                for (i = 0; i < tile_x; i++) 
                {
                        color = 0L;
                        color |= (pixels[j][i].r & 0x0000ff);
                        color |= (pixels[j][i].g & 0x0000ff) << 8;
                        color |= (pixels[j][i].b & 0x0000ff) << 16;
                        putpixel(tilebmp, i, j, color);
                }
        }

#if 0
        drawing_mode(DRAW_MODE_TRANS, tilebmp, 0, 0);
        set_trans_blender(255, 255, 255, 127);

        for(y = 0; y < tile_y; y++)
                for(x = 1; x < tile_x; x += 2)
                {
                        color=getpixel(tilebmp, min(x+1,tile_x - 2),y);
                        putpixel(tilebmp, x, y, color);
                }
        /* interpolate horizontal lines */

        for(x = 0; x < tile_x; x++)
                for(y = 1; y < tile_y; y += 2)
                {
                        color = getpixel(tilebmp, x, min(y+1,tile_y - 2));
                        putpixel(tilebmp, x, y, color);
                }
#endif

        drawing_mode(DRAW_MODE_SOLID, tilebmp, 0, 0);

        if(trans)
        {
		/* WAC probably slower, but more accurate */
		for (x = 0; x < tile_x; x++)
		    for (y = 0; y < tile_y; y++)
	                if(getpixel(tilebmp, x, y) == color2)
        	                putpixel(tilebmp,x,y, TRANS_COLOR);
#if 0
                if(getpixel(tilebmp, 0, 0) == color2)
                        floodfill(tilebmp,0,0, TRANS_COLOR);
                if(getpixel(tilebmp, 0, 31) == color2)
                        floodfill(tilebmp,0,31, TRANS_COLOR);
                if(getpixel(tilebmp, 31, 0) == color2)
                        floodfill(tilebmp,31,0, TRANS_COLOR);
                if(getpixel(tilebmp, 31, 31) == color2)
                        floodfill(tilebmp,31,31, TRANS_COLOR);
#endif
        }

	if (!make_big) {
            save_bitmap(filename, tilebmp, 0);
	} else {
	    int col = (int)(tilecount % TILES_PER_ROW);
	    int row = (int)(tilecount / TILES_PER_ROW);
#ifdef DEBUG
	    fprintf(stderr, "col: %i row: %i\n", col, row);
#endif
	    blit(tilebmp, bigtile_bmp, 0, 0, col * tile_x, row * tile_y, tile_x, tile_y);
	}
        destroy_bitmap(tilebmp);
}

int
main(argc, argv)
int argc;
char *argv[];
{
        int i;
        struct tm *newtime;
        time_t aclock;
        char *paletteptr;
        char cmd2[60];
        char filename[60];
        char util_mode;
        FILE *ofp; /* Output for -i, -h */
        const char **tilefiles;
        char bigtile_file[BUFSZ];
        boolean trans_background = FALSE;

	/* All operations assume 24 bit internal files */
        set_color_depth(24);

        if (argc < 2) {
	    	fprintf(stderr, "Bad arg count (%d).\n", argc-1);
	    	(void) fflush(stderr);
                exit(EXIT_FAILURE);
        }
        
        util_mode = argv[1][1];
        
        if (util_mode == 'I') util_mode = 'i';
        if (util_mode == 'H') util_mode = 'h';
        if (util_mode == 'F') util_mode = 'f';
        if (util_mode == 'B') util_mode = 'b';
        if (util_mode == 'C') util_mode = 'c';
        
        if (util_mode != 'i' && util_mode != 'h' && util_mode != 'f' 
			     && util_mode != 'b' && util_mode != 'c'
			     && util_mode != 'p') {
	    	fprintf(stderr, "Unknown option '-%c'.\n Use -i, -h, -f, -b, -c or -p.\n", 
	    					util_mode);
	    	(void) fflush(stderr);
                exit(EXIT_FAILURE);        	
        }

        if (util_mode == 'p') {
            if (argc != 3) {
	    	fprintf(stderr, "Bad arg count (%d).\n", argc-1);
	    	(void) fflush(stderr);
                exit(EXIT_FAILURE);
            } else {
#ifdef FINAL_COLORDEPTH_8_BPP
            	PALETTE tmp_pal;
            	BITMAP* tmp_bmp;
        	char rsvd[256];
        	int i;
            	
        	strcpy(bigtile_file, argv[2]);
            	bigtile_bmp = load_bitmap(bigtile_file,tmp_pal);
            	
            	if (!bigtile_bmp) {
	    		fprintf(stderr, "Unable to load %s.\n", argv[2]);
	    		(void) fflush(stderr);
                	exit(EXIT_FAILURE);
            	}

		tmp_pal[0].r = tmp_pal[0].b = 63; 
		tmp_pal[0].g = 0;
		
		rsvd[0] = 1;
		for (i = 1; i < 256; i++) rsvd[i] = 0;

        	generate_optimized_palette(bigtile_bmp, tmp_pal, rsvd);
        	select_palette(tmp_pal);
        	
        	tmp_bmp = create_bitmap_ex(8, bigtile_bmp->w, bigtile_bmp->h);

		blit(bigtile_bmp, tmp_bmp,  0, 0, 0, 0,bigtile_bmp->w, bigtile_bmp->h);
		
        	save_bitmap(bigtile_file, tmp_bmp, tmp_pal);
        	
        	destroy_bitmap(bigtile_bmp);
        	destroy_bitmap(tmp_bmp);

#endif
	        exit(EXIT_SUCCESS);
            }
        }

        if ((util_mode == 'c' && argc != 4) || (util_mode != 'c' && argc != 2)) {
	    	fprintf(stderr, "Bad arg count (%d).\n", argc-1);
	    	(void) fflush(stderr);
                exit(EXIT_FAILURE);
        }
        
        if (util_mode != 'c') {
	    if (argv[1][2] == '1' && argv[1][3] == '6') {
    		fprintf(stderr, "Using 16x16 text tile files\n");
    		(void) fflush(stderr);
		tilefiles = tilefiles16;
		strcpy(bigtile_file, bigtile_file16);
	    } else if (argv[1][2] == '3' && 
		    	(argv[1][3] == 'D' || argv[1][3] == 'd')) {
    		fprintf(stderr, "Using 48x64 text tile files\n");
    		(void) fflush(stderr);
		tilefiles = tilefiles3d;
		strcpy(bigtile_file, bigtile_file3d);
		trans_background = TRUE;
	    } else {
	       	/* Default mode is 32 */
    		fprintf(stderr, "Using 32x32 text tile files\n");
    		(void) fflush(stderr);
		tilefiles = tilefiles32;
		strcpy(bigtile_file, bigtile_file32);
	    }
        } else {
        	strcpy(bigtile_file, argv[3]);
        }

        time(&aclock);
        newtime = localtime(&aclock);

        tilecount = 0;
        paletteflag = 0;        
        filenum = 0;
        
        /* Open file (if needed) */
        if (util_mode == 'i') {
	    if (!(ofp = fopen(index_file, WRTMODE))) {
		perror(index_file);
		exit(EXIT_FAILURE);
	    }
	    fprintf(ofp,Dont_Edit_Index);   
        } else if (util_mode == 'h') {
	    if (!(ofp = fopen(allegfx_file, WRTMODE))) {
		perror(allegfx_file);
		exit(EXIT_FAILURE);
	    }
    	    fprintf(ofp,"/*\tSCCS Id: @(#)allegfx.h\t3.2\t96/05/17 */\n\n");
	    fprintf(ofp,Dont_Edit_Code);	    
	    fprintf(ofp,"/*\tNOTE: This file is completely obselete! */\n"
		"/*\tI have no idea why you made it */\n\n");
        } else if (util_mode == 'b' || util_mode == 'c') {
            bigtile_bmp = NULL;
        }
        
        while (filenum < 3) {        	
                int index = 0;
                boolean trans_mode = ((filenum < 2) || trans_background);

		if (util_mode != 'c') {
                    if (!fopen_text_file(tilefiles[filenum], RDTMODE)) {
                        fprintf(stderr,
                         "Cannot find file '%s'.\n", tilefiles[filenum]);
                        exit(EXIT_FAILURE);
                    }
		} else if (!fopen_text_file(argv[2], RDTMODE)) {
                        fprintf(stderr,
                         "Cannot find file '%s'.\n", tilefiles[filenum]);
                        exit(EXIT_FAILURE);
		}
                    
                num_colors = colorsinmap;
                if (num_colors > MAXCOLORMAPSIZE) {
                        fprintf(stderr, "too many colors (%d)\n", num_colors);
                        exit(EXIT_FAILURE);
                }
                if (!paletteflag) {
                        paletteptr = tibheader.palette;
                        for (i = 0; i < num_colors; i++) {
                                *paletteptr++ = ColorMap[CM_RED][i],
                                *paletteptr++ = ColorMap[CM_GREEN][i],
                                *paletteptr++ = ColorMap[CM_BLUE][i];
                        }
                        paletteflag++;
                }

	    	if ((util_mode == 'b' || util_mode == 'c') && !bigtile_bmp) {
	    	    /* Only make this once */
	            bigtile_bmp = create_bitmap(tile_x * TILES_PER_ROW, tile_y * TILES_PER_COL);
	    	}

                while (read_text_tile(pixels)) {
                        sprintf(filename, "%c%03d%s", 
                                (!filenum) ? 'm' : (filenum == 1) ? 'o' : 'e',
                                index, ".bmp");
			if (util_mode == 'i') {
                            fprintf(ofp, "%04d (%s) : %s\n", 
                                tilecount, tilename(filenum+1,index), 
                                filename);
			} else if (util_mode == 'f') {
                                make_bitmap(pixels, tilecount, trans_mode
                                            ,filename, FALSE);
			} else if (util_mode == 'b' || util_mode == 'c') {
                                make_bitmap(pixels, tilecount, trans_mode
                                            ,filename, TRUE);
			}

                        tilecount++;     
                        index++;
                }
                
                if (util_mode == 'h') 
                    switch(filenum)
                    {
		            case 0:
		                fprintf(ofp, "#define NUMBER_OF_MONS %d\n", 
		                        tilecount);
		                break;
		            case 1:
		                fprintf(ofp, "#define NUMBER_OF_OBJS %d\n", 
		                        tilecount);
		                break;
		            case 2:
		                fprintf(ofp, "#define NUMBER_OF_TILES %d\n", 
		                        tilecount);
		                break;
                    }

		if (util_mode == 'f')
		    fprintf(stderr, "%d tiles processed from %s\n", 
					index, tilefiles[filenum]);

                (void) fclose_text_file();
                ++filenum;
                
                /* Only process 1 file */
                if (util_mode == 'c') break;
        }

	/* Close file */
        if ((util_mode == 'i') || (util_mode == 'h'))
        	fclose(ofp);
        else if (util_mode == 'f')
		fprintf(stderr, "Total of %d bmp tiles written.\n", tilecount);
        else if (util_mode == 'b' || util_mode == 'c') {
        	PALETTE tmp_pal;
#ifdef FINAL_COLORDEPTH_8_BPP
        	BITMAP *tmp_bmp;
        	char rsvd[256];
        	int i;

		tmp_pal[0].r = tmp_pal[0].b = 63; 
		tmp_pal[0].g = 0;
		
		rsvd[0] = 1;
		for (i = 1; i < 256; i++) rsvd[i] = 0;

        	generate_optimized_palette(bigtile_bmp, tmp_pal, rsvd);
        	select_palette(tmp_pal);
        	
        	tmp_bmp = create_bitmap_ex(8, TILES_PER_ROW * tile_x, 
       				(int)(tile_y * (1 + (tilecount / TILES_PER_ROW))));

		blit(bigtile_bmp, tmp_bmp,  0, 0, 0, 0,bigtile_bmp->w, bigtile_bmp->h);
		
        	save_bitmap(bigtile_file, tmp_bmp, tmp_pal);
        	
        	destroy_bitmap(bigtile_bmp);
        	destroy_bitmap(tmp_bmp);
        	
#else
        	save_bitmap(bigtile_file, bigtile_bmp, tmp_pal);
        	
        	destroy_bitmap(bigtile_bmp);

#endif
        }

        exit(EXIT_SUCCESS);
        /*NOTREACHED*/
        return 0;
}

