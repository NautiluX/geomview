/* Copyright (C) 1992-1998 The Geometry Center
 * Copyright (C) 1998-2000 Geometry Technologies, Inc.
 * Copyright (C) 2006-2007 Claus-Justus Heine
 *
 * This file is part of Geomview.
 * 
 * Geomview is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * Geomview is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with Geomview; see the file COPYING.  If not, write
 * to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
 * USA, or visit http://www.gnu.org.
 */
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <obstack.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free  free

#ifndef COLORDEFS
typedef struct Color {
  float r, g, b;
} Color;
#endif

typedef struct _vertex {	/* A single vertex */
  struct _vertex *next;		/* Pointer to next vertex */

  float *coord;			/* My coordinates for ND case. */

  Color c;			/* Vertex color if any */
  bool clip;			/* Have I been clipped or not? */
  float val;			/* Function value at this vertex */
  int num;			/* Reference number of vertex in array */
} vertex;

typedef struct _vertex_list {	/* Global list of shared vertices */
  int numvtx;			/* Number of vertices */

/* private */
  vertex *head;			/* pointer to head of list */
  vertex *point;		/* pointer used while traversing list */

} vertex_list;

typedef struct _pvtx
{				/* vertex holder for vertex list */
  /* of each polygon */
  int num;			/* reference number of vertex in array */
  struct _vertex *me;		/* Pointer to actual (shared) vertex. */
  struct _pvtx *next;		/* Pointer to next pvtx in polyvtx_list */

} pvtx;

typedef struct _polyvtx_list {	/* vertex list for each polygon */
  int numvtx;			/* number of vertices in polygon */

  pvtx *head;			/* head of polygon vertex list */
  pvtx *point;			/* pointer used while traversing list */
} polyvtx_list;


typedef struct _poly {		/* A polygon */
  int numvtx;			/* Number of vertices */
  bool clipped;			/* has the whole polygon been cut away? */
  /* 1 if it has been cut away, 0 if some */
  /* or all of the polygon remains */
  polyvtx_list *me;		/* Vertex list of polygon */
  Color c;			/* Color of this polygon if doing per */
  /* face coloring. */
  struct _poly *next;		/* Next polygon in list of polys. */
} poly;

typedef struct _poly_list {	/* List of polygons */
  int numpoly;			/* number of polygons */

#define	HAS_VN	0x1
#define HAS_VC	0x4
#define	HAS_PC	0x8
  int has;			/* bit mask of associated data */
 
  poly *head;			/* head of list of polys */
  poly *point;			/* pointer for traversing list */

} poly_list;

#define NEWLINE '\n'
#define SPACE	' '

enum clip_side { CLIP_LE, CLIP_GE };

typedef struct clip {
  poly_list polyhedron;
  vertex_list polyvertex;
  enum clip_side side;	/* side to cut... 0 -- less than   1 -- greater than */
  int dim;		/* We are working in dim Dimensions. */
  float *surf;		/* Surface parameters.
			 * For a plane, the plane normal vector
			 * (or, other parameters for non-planar clip)
			 */

  float (*clipfunc) ();	/* Clipping function, whose value is zero
			 * along the surface where we should clip.
			 */
  int nonlinear;	/* Is clipfunc an affine function of position? */
  float level;		/* Value of clipfunc at clipping surface */

  struct obstack obst;  /* temporary memory is allocated through an
			 * obstack for easy clean-up.
			 */
  
} Clip;

/*
 * Typical usage:
 *    Have a "Clip" variable;
 *    clip_init(&clip);
 *    setGeom(&clip, oogl-geometry-object);
 *    setSide(&clip, CLIP_xx);
 *    // Possibly change clip.clipfunc; clip_init sets planar clipping.
 *    setClipPlane(&clip, coeffs[], threshold);
 *    do_clip(&clip);
 *    g = getGeom(&clip);
 *    // Must call setGeom() before another do_clip; the internal data
 *    // structures can't handle successive clips against the same object.
 *    // Could simply setGeom(&clip, g) if that's intended.
 *    clip_destroy(&clip);
 */
void setClipPlane(Clip *, float coeffs[], float level);

void setSide(Clip *, enum clip_side side); /* 0->negative, 1->positive side */

void setGeom(Clip *, void *g);
void *getGeom(Clip *);
void clip_init(Clip *);
void clip_destroy(Clip *);
void do_clip(Clip *);

/*
 * Local Variables: ***
 * c-basic-offset: 2 ***
 * End: ***
 */
