/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DRIVER_OPENGL_SCAN_CONVERTER_H
#define DRIVER_OPENGL_SCAN_CONVERTER_H

#define SPAN_MAX_WIDTH 16384
#define MAX_GLUINT	0xffffffff

#define X_COMP 0
#define Y_COMP 1
#define Z_COMP 2
#define W_COMP 3

#define PIXEL_TEST_PASS 1
#define PIXEL_TEST_FAIL 0

#define MAX(a,b) \
({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })

#define MIN(a,b) \
({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })


/* Convert float to int by rounding to nearest integer, away from zero */
static inline int IROUND(float f)
{
	return (int) ((f >= 0.0F) ? (f + 0.5F) : (f - 0.5F));
}

#define SUB_PIXEL_BITS 4

/* Fixed point arithmetic macros */
#ifndef FIXED_FRAC_BITS
#define FIXED_FRAC_BITS 11
#endif

#define FIXED_SHIFT     FIXED_FRAC_BITS
#define FIXED_ONE       (1 << FIXED_SHIFT)
#define FIXED_HALF      (1 << (FIXED_SHIFT-1))
#define FIXED_FRAC_MASK (FIXED_ONE - 1)
#define FIXED_INT_MASK  (~FIXED_FRAC_MASK)
#define FIXED_EPSILON   1
#define FIXED_SCALE     ((float) FIXED_ONE)
#define FIXED_DBL_SCALE ((double) FIXED_ONE)
#define FloatToFixed(X) (IROUND((X) * FIXED_SCALE))
#define FixedToDouble(X) ((X) * (1.0 / FIXED_DBL_SCALE))
#define IntToFixed(I)   ((I) << FIXED_SHIFT)
#define FixedToInt(X)   ((X) >> FIXED_SHIFT)
#define FixedToUns(X)   (((unsigned int)(X)) >> FIXED_SHIFT)
#define FixedCeil(X)    (((X) + FIXED_ONE - FIXED_EPSILON) & FIXED_INT_MASK)
#define FixedFloor(X)   ((X) & FIXED_INT_MASK)
#define FixedToFloat(X) ((X) * (1.0F / FIXED_SCALE))
#define PosFloatToFixed(X)      FloatToFixed(X)
#define SignedFloatToFixed(X)   FloatToFixed(X)

/* Forward declaration */
struct list_t;
struct opengl_sc_edge_func_t;

struct opengl_sc_vertex_t
{
	float pos[4];
};

struct opengl_sc_triangle_t
{
	/* 3 verticies */
	struct opengl_sc_vertex_t *vtx0;
	struct opengl_sc_vertex_t *vtx1;
	struct opengl_sc_vertex_t *vtx2;
	/* And 3 edge functions */
	struct opengl_sc_edge_func_t *edgfunc0;
	struct opengl_sc_edge_func_t *edgfunc1;
	struct opengl_sc_edge_func_t *edgfunc2;	
};

struct opengl_sc_edge_t
{
	struct opengl_sc_vertex_t *vtx0; 	/* Y(vtx0) < Y(vtx1) */
	struct opengl_sc_vertex_t *vtx1;
	float dx;				/* X(vtx1) - X(vtx0) */
	float dy;				/* Y(vtx1) - Y(vtx0) */
	float dxdy;				/* dx/dy */
	int fdxdy;				/* dx/dy in fixed-point */
	float adjy;				/* adjust from v[0]->fy to fsy, scaled */
	int fsx;					/* first sample point x coord */
	int fsy;					/* first sample point y coord */
	int fx0;					/* fixed pt X of lower endpoint */
	int lines;				/* number of lines to be sampled on this edge */	
};

struct opengl_sc_pixel_info_t
{
	/* Window coordinates of a pixel */
	int pos[4];
	char wndw_init;

	/* Barycentric coordinates to be load to VGPRs */
	float brctrc_i; 
	float brctrc_j;
};

struct opengl_sc_span_array_t
{
	unsigned int  z[SPAN_MAX_WIDTH];  /* fragment Z coords */	
};

struct opengl_sc_span_t
{
	/* Coord of first fragment in horizontal span/run */
	int x;
	int y;

	float attrStart[4];   /* initial value */
	float attrStepX[4];   /* dvalue/dx */
	float attrStepY[4];   /* dvalue/dy */

	int z;
	int zStep;

	/* Number of fragments in the span */
	unsigned int end;

	struct opengl_sc_span_array_t *array;

};

/* Used to check if a pixel is inside the triangle */
struct opengl_sc_edge_func_t
{
	float a;
	float b;
	float c;
};

/* Bounding box for an triangle */
struct opengl_sc_bounding_box_t
{
	/* Top left of the bounding box */
	int x0;
	int y0;
	/* Size must be power of 2 */
	int size;
};

struct opengl_sc_vertex_t *opengl_sc_vertex_create();
void opengl_sc_vertex_free(struct opengl_sc_vertex_t *vtx);

struct opengl_sc_triangle_t *opengl_sc_triangle_create();
void opengl_sc_triangle_free(struct opengl_sc_triangle_t *triangle);
void opengl_sc_triangle_set(struct opengl_sc_triangle_t *triangle, struct opengl_sc_vertex_t *vtx0,
	struct opengl_sc_vertex_t *vtx1, struct opengl_sc_vertex_t *vtx2);

struct opengl_sc_edge_t *opengl_sc_edge_create(struct opengl_sc_vertex_t *vtx0, struct opengl_sc_vertex_t *vtx1);
void opengl_sc_edge_free(struct opengl_sc_edge_t *edge);

struct opengl_sc_span_t *opengl_sc_span_create();
void opengl_sc_span_free(struct opengl_sc_span_t *spn);
void opengl_sc_span_interpolate_z(struct opengl_sc_span_t *spn);

struct opengl_sc_pixel_info_t *opengl_sc_pixel_info_create();
void opengl_sc_pixel_info_free(struct opengl_sc_pixel_info_t *pxl_info);

struct opengl_sc_edge_func_t *opengl_sc_edge_func_create();
void opengl_sc_edge_func_free(struct opengl_sc_edge_func_t *edge_func);
void opengl_sc_edge_func_set(struct opengl_sc_edge_func_t *edge_func,struct opengl_sc_vertex_t *vtx0, struct opengl_sc_vertex_t *vtx1);
int opengl_sc_edge_func_test_pixel(struct opengl_sc_edge_func_t *edge_func, int x, int y);

struct list_t *opengl_sc_tiled_rast_triangle_gen(struct opengl_sc_triangle_t *triangle);
void opengl_sc_triangle_tiled_pixel_gen(struct opengl_sc_triangle_t *triangle, int x, int y, int size, struct list_t *pxl_lst);

struct list_t *opengl_sc_rast_triangle_gen(struct opengl_sc_triangle_t *triangle);
void opengl_sc_rast_triangle_done(struct list_t *pxl_lst);

#endif