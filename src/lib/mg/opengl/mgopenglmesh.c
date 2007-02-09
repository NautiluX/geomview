/* Copyright (C) 1992-1998 The Geometry Center
 * Copyright (C) 1998-2000 Stuart Levy, Tamara Munzner, Mark Phillips
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

#if 0
static char copyright[] = "Copyright (C) 1992-1998 The Geometry Center\n\
Copyright (C) 1998-2000 Stuart Levy, Tamara Munzner, Mark Phillips";
#endif


/* Authors: Charlie Gunn, Stuart Levy, Tamara Munzner, Mark Phillips */

#ifdef GLUT
# include <GL/glut.h>
#else
#define GL_GLEXT_PROTOTYPES
# include <GL/gl.h>
#endif

#include <mgP.h>
#include <mgopenglP.h>
#include <meshflag.h>

#undef P
/* ^^^ evil kludge XXX to get the P variables here to not get confused
 * with the P prototype macro. -nils */
#define HAS_N      0x01
#define HAS_NQ     0x02
#define HAS_C      0x04
#define HAS_SMOOTH 0x08
#define HAS_STR    0x10

void
mgopenglsubmesh(int wrap, int nu, int nv,
		int umin, int umax,
		int vmin, int vmax,
		HPoint3 *meshP, Point3 *meshN, Point3 *meshNQ,
		ColorA *meshC, Point3 *meshSTR,
		int mflags)
{
  int u, v;
  int ucnt, vcnt;
  HPoint3 *P;
  Point3 *N, *NQ;
  ColorA *C;
  Point3 *STR;
  int prev;
  int du;
  int douwrap;
  int i;
  int has;
  Appearance *ap;


  if(nu <= 0 || nv <= 0)
    return;

  ap = &_mgc->astk->ap;
  if ((_mgc->astk->mat.override & MTF_DIFFUSE) &&
      !(_mgc->astk->flags & MGASTK_SHADER))
    meshC = 0;

  has = 0;
  if (meshN && !(_mgc->astk->flags & MGASTK_SHADER))
    has |= HAS_N;
  if (meshNQ && !(_mgc->astk->flags & MGASTK_SHADER))
    has |= HAS_NQ;
  if (meshC)
    has |= HAS_C;
  if (IS_SMOOTH(ap->shading)) {
    has |= HAS_SMOOTH;
    has &= ~HAS_NQ;
  } else if (has & HAS_NQ) {
    has &= ~HAS_N;
  }
  if ((ap->flag & (APF_TEXTURE|APF_FACEDRAW)) == (APF_TEXTURE|APF_FACEDRAW)
     && _mgc->astk->ap.tex != NULL) {
    if (meshSTR != NULL)
      has |= HAS_STR;
    if (has&HAS_STR) {
      mgopengl_needtexture();
    }
  }

  if ((_mgc->astk->mat.valid & MTF_ALPHA)
      &&
      (_mgc->astk->mat.override & MTF_ALPHA) || !(has & HAS_C)) {
    if (_mgc->astk->ap.mat->diffuse.a != 1.0) {
      mflags |= COLOR_ALPHA;
    } else {
      mflags &= ~COLOR_ALPHA;
    }
  }

  if (ap->flag & APF_FACEDRAW && nu > 1 && nv > 1 &&
      !((ap->flag & APF_TRANSP) && (mflags & COLOR_ALPHA))) {

    /* We triangulate strips of (v,u) mesh points:
     *  (v,u)    (v,u+1)    (v,u+2) ...
     *  (v+1,u)  (v+1,u+1)  (v+1,u+2) ...
     * using the vertex sequence
     *  (v,u) , (v+1,u), (v,u+1), (v+1,u+1), (v,u+2), ...
     * This covers the territory from v to v+1; then repeat for other v's.
     * If we hit the 256-vertex triangle-mesh limit, the strip is spliced
     * by redrawing the latest (v,u+i),(v+1,u+i) pair of vertices.
     */


    glColorMaterial(GL_FRONT_AND_BACK, _mgopenglc->lmcolor);
    glEnable(GL_COLOR_MATERIAL);
    MAY_LIGHT();

    if(!(has & HAS_C))
      D4F(&ap->mat->diffuse);

    v = vmax - vmin + 1;
    du = umin + vmin * nu;

    if(wrap & MM_VWRAP) {
      /* V-wrapping: cur = mesh[vmin,u], prev = mesh[vmax,u] */
      prev = nu * (v - 1);
    } else {
      /* Not V-wrapping: cur = mesh[vmin+1,u], prev = mesh[vmin,u] */
      du += nu;
      prev = -nu;
      v--;	/* One less V-row, too */
    }

    do {					/* Loop over V */
      P  = meshP + du;
      N  = meshN + du;
      NQ = meshNQ + du;
      C  = meshC + du;
      STR = meshSTR + du;
      ucnt = umax - umin + 1;
      glBegin(GL_TRIANGLE_STRIP);
      douwrap = (wrap & MM_UWRAP);
      do {
	/* Loop over U */
	u = ucnt;
	ucnt = 0;

	switch( has ) {

	case 0:
	case HAS_SMOOTH:
	  do {
	    glVertex4fv((float *)P+prev);
	    glVertex4fv((float *)P);
	    P++;
	  } while(--u);
	  break;

	case HAS_C:
	  do {
	    D4F(C+prev); glVertex4fv((float *)(P+prev));
	    glVertex4fv((float *)P);
	    C++; P++;
	  } while(--u);
	  break;

	case HAS_C|HAS_SMOOTH:
	  do {
	    D4F(C+prev); glVertex4fv((float *)(P+prev));
	    D4F(C); glVertex4fv((float *)P);
	    C++; P++;
	  } while(--u);
	  break;

	case HAS_NQ:
	  do {
	    N3F(NQ+prev,P+prev);
	    glVertex4fv((float *)(P+prev));
	    glVertex4fv((float *)P);
	    NQ++; P++;
	  } while(--u);
	  break;

	case HAS_N:
	  do {
	    N3F(N+prev,P); glVertex4fv((float *)(P+prev));
	    glVertex4fv((float *)P);
	    N++; P++;
	  } while(--u);
	  break;

	case HAS_N|HAS_SMOOTH:
	  do {
	    N3F(N+prev,P); glVertex4fv((float *)(P+prev));
	    N3F(N,P); glVertex4fv((float *)P);
	    N++; P++;
	  } while(--u);
	  break;

	case HAS_C|HAS_NQ:
	  do {
	    D4F(C+prev); N3F(NQ+prev,P+prev); glVertex4fv((float *)(P+prev));
	    glVertex4fv((float *)P);
	    C++; NQ++; P++;
	  } while(--u);
	  break;

	case HAS_C|HAS_N:
	  do {
	    D4F(C+prev); N3F(N+prev,P); glVertex4fv((float *)(P+prev));
	    glVertex4fv((float *)P);
	    C++; N++; P++;
	  } while(--u);
	  break;

	case HAS_C|HAS_N|HAS_SMOOTH:
	  do {
	    D4F(C+prev);	N3F(N+prev,P);	glVertex4fv((float *)(P+prev));
	    D4F(C);		N3F(N,P);	glVertex4fv((float *)P);
	    C++; N++; P++;
	  } while(--u);
	  break;

        case HAS_STR:
          do {
            glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            P++; STR++;
          } while(--u);
          break;

        case HAS_C|HAS_STR:
          do {
            D4F(C+prev);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            C++; P++; STR++;
          } while(--u);
          break;

        case HAS_C|HAS_SMOOTH|HAS_STR:
          do {
            D4F(C+prev);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            D4F(C);
	    glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            C++; P++; STR++;
          } while(--u);
          break;

        case HAS_NQ|HAS_STR:
          do {
	    N3F(NQ+prev,P+prev);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            NQ++; P++; STR++;
          } while(--u);
          break;

        case HAS_N|HAS_STR:
          do {
            N3F(N+prev,P);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            N++; P++; STR++;
          } while(--u);
          break;

        case HAS_N|HAS_SMOOTH|HAS_STR:
          do {
            N3F(N+prev,P);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            N3F(N,P);
	    glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            N++; P++; STR++;
          } while(--u);
          break;

        case HAS_C|HAS_NQ|HAS_STR:
	  do {
	    N3F(NQ+prev,P+prev);
            D4F(C+prev);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            C++; NQ++; P++; STR++;
          } while(--u);
          break;

        case HAS_C|HAS_N|HAS_STR:
          do {
            D4F(C+prev); N3F(N+prev,P);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            C++; N++; P++; STR++;
          } while(--u);
          break;

        case HAS_C|HAS_N|HAS_SMOOTH|HAS_STR:
          do {
            D4F(C+prev); N3F(N+prev,P);
	    glTexCoord2fv((float *)(STR+prev));
	    glVertex4fv((float *)(P+prev));
            D4F(C); N3F(N,P);
	    glTexCoord2fv((float *)STR);
	    glVertex4fv((float *)P);
            C++; N++; P++; STR++;
          } while(--u);
          break;
        }

        if(ucnt == 0) {
          if(douwrap) {
            douwrap = 0;        /* Loop again on first vertex */
            ucnt = 1;
            P = meshP + du;
            N = meshN + du;
            NQ = meshNQ + du;
            C = meshC + du;
            STR = meshSTR + du;
          }
        } else {
          glEnd();           /* Hit tmesh limit, splice */
          glBegin(GL_TRIANGLE_STRIP);
          C--; N--; P--; STR--; /* Redraw last vertex */
        }
      } while(ucnt);

      glEnd();
      prev = -nu;
      du += nu;
    } while(--v > 0);
  }

  if(ap->flag & (APF_EDGEDRAW|APF_NORMALDRAW)
     || (ap->flag & APF_FACEDRAW && (nu == 1 || nv == 1))) {
    glDisable(GL_COLOR_MATERIAL);
    DONT_LIGHT();
    if(_mgopenglc->znudge) mgopengl_closer();
    if(ap->flag & APF_EDGEDRAW) {			/* Draw edges */
      glColor3fv((float *)&ap->mat->edgecolor);

      du = umin + vmin * nu;
      ucnt = umax - umin + 1;
      vcnt = vmax - vmin + 1;
      v = vcnt;
      do {
	if(wrap & MM_UWRAP)
	  glBegin(GL_LINE_LOOP);
	else
	  glBegin(GL_LINE_STRIP);
	u = ucnt;
	P = meshP + du;
	do {
	  glVertex4fv((float *)P);
	  P++;
	} while(--u > 0);
	if(wrap & MM_UWRAP)
	  glEnd();
	else
	  glEnd();
	du += nu;
      } while(--v > 0);

      du = umin + vmin * nu;
      u = ucnt;
      do {
	v = vcnt;
	if(wrap & MM_VWRAP)
	  glBegin(GL_LINE_LOOP);
	else
	  glBegin(GL_LINE_STRIP);
	P = meshP + du;
	do {
	  glVertex4fv((float *)P);
	  P += nu;
	} while(--v > 0);
	if(wrap & MM_VWRAP)
	  glEnd();
	else
	  glEnd();
	du++;
      } while(--u > 0);
    }

    if(ap->flag & APF_NORMALDRAW) {
      if (has & HAS_N) {
	glColor3fv((float *)&ap->mat->normalcolor);

	for (i = nu*nv, P=meshP, N=meshN; --i >= 0; P++, N++) {
	  mgopengl_drawnormal(P, N);
	}
      } else if (has & HAS_NQ) {
	glColor3fv((float *)&ap->mat->normalcolor);

	for (i = nu*nv, P=meshP, NQ=meshNQ; --i >= 0; P++, NQ++) {
	  mgopengl_drawnormal(P, NQ);
	}
      }
    }
    if(_mgopenglc->znudge) mgopengl_farther();
  }
}

void
mgopengl_mesh(int wrap, int nu, int nv,
	      HPoint3 *meshP, Point3 *meshN, Point3 *meshNQ,
	      ColorA *meshC, Point3 *meshSTR,
	      int mflags)
{
  mgopenglsubmesh(wrap, nu, nv, 0, nu-1, 0, nv-1, meshP,
		  meshN, meshNQ, meshC, meshSTR, mflags);
}

/*
 * Local Variables: ***
 * c-basic-offset: 2 ***
 * End: ***
 */
