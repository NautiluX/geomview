/* Copyright (C) 2006-2007 Claus-Justus Heine 
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

/* BSP-tree implementation. The members of the BSP-tree will be
 * polyhedrons, but the goal is to generate BSP-trees for QUADs and
 * MESHes too. Not for ND-objects, would probably not work.
 */

#include "geomclass.h"
#include "polylistP.h"
#include "mg.h"
#include "mesh.h"
#include "meshP.h"
#include "quadP.h"
#include "meshflag.h"
#include "bsptree.h"
#include "bsptreeP.h"

#define obstack_chunk_alloc malloc
#define obstack_chunk_free  free

#define POLY_SCRATCH 0x80000000 /* Flag indicates that this poly lives
				 * on our obstack
				 */

static void check_poly(Poly *poly)
{
  int i;
  
  for (i = 0; i < poly->n_vertices; i++) {
    if (!finite(poly->v[i]->pt.x +
		poly->v[i]->pt.y +
		poly->v[i]->pt.z +
		poly->v[i]->pt.w)) {
      abort();
    }
  }
}
#define ListPush(head, new_node)		\
  {						\
    PolyListNode *_lp_tmp_ = (new_node);	\
    _lp_tmp_->next = (head); (head) = _lp_tmp_;	\
  }
#define ListPop(head, car)					\
  (car) = (head); (head) = (head)->next; (car)->next = NULL;

typedef enum PolyPos {
  BACKOF     = -1,
  COPLANAR   = 0,
  INFRONTOF  = 1,
  BOTH_SIDES = 2
} PolyPos;

typedef struct EdgeIntersection
{
  int v[2];         /* The vertex number of the enclosing points */
  HPt3Coord scp[2]; /* The position of the vertices relative to the
		     * intersection plane.
		     */
} EdgeIntersection;

static void BSPTreeCreateRecursive(BSPTreeNode *tree,
				   PolyListNode *pllist,
				   struct obstack *scratch);

static inline Poly *new_poly(int nv, Vertex **v, struct obstack *scratch)
{
  Poly *newp;
 
  newp = obstack_alloc(scratch, sizeof(Poly));
  memset(newp, 0, sizeof(Poly));
  newp->flags = POLY_SCRATCH;
  newp->n_vertices = nv;
  if (v) {
    newp->v = v;
  } else {
    newp->v = obstack_alloc(scratch, nv*sizeof(Vertex *));
  }
  if (newp->v) {
    memset(newp->v, 0, nv*sizeof(Vertex *));
  }

  return newp;
}

static inline PolyListNode *new_poly_list_node(const void **tagged_app,
					       struct obstack *scratch)
{
  PolyListNode *new_pn;

  new_pn             = obstack_alloc(scratch, sizeof(PolyListNode));
  new_pn->pn         = NULL;
  new_pn->tagged_app = tagged_app;

  return new_pn;
}

/* Generate a transformed copy of poly. */
static inline Poly *transform_poly(Transform T, Transform Tdual, Transform TxT,
				   Poly *poly,
				   struct obstack *scratch)
{
  Poly *newp;
  int i;
  
  newp = new_poly(poly->n_vertices, NULL, scratch);
  for (i = 0; i < poly->n_vertices; i++) {
    newp->v[i] = obstack_alloc(scratch, sizeof(Vertex));
    HPt3Transform(T, &poly->v[i]->pt, &newp->v[i]->pt);
    if (poly->flags & PL_HASVCOL) {
      newp->v[i]->vcol = poly->v[i]->vcol;
    }
    if (poly->flags & PL_HASVN) {
      NormalTransform(Tdual, &poly->v[i]->vn, &newp->v[i]->vn);
    }
    if (poly->flags & PL_HASST) {
      TxSTTransform (TxT, &poly->v[i]->st, &newp->v[i]->st);
    }
  }

  if (poly->flags & PL_HASPCOL) {
    newp->pcol = poly->pcol;
  }
  if (poly->flags & PL_HASPN) {
    NormalTransform(Tdual, &poly->pn, &newp->pn);
  }
  
  newp->flags = poly->flags;

  return newp;
}

/* Split the given polygon -- which must describe a quadrilateral --
 * along the diagonal starting at "vertex". Used for splitting
 * non-flat or concave quadrilaterals.
 */
static inline void split_quad_poly(int vertex, Poly *poly0,
				   PolyListNode **plist,
				   const void **tagged_app,
				   struct obstack *scratch)
{
  PolyListNode *new_pn;
  Poly *poly[2];
  Poly savedp;
  Vertex *savedv[4];
  int i, v;

  if (poly0->flags & POLY_SCRATCH) {
    /* reuse the space */
    savedp = *poly0;
    savedp.v = savedv;
    memcpy(savedp.v, poly0->v, 4*sizeof(Vertex *));
    new_pn = new_poly_list_node(tagged_app, scratch);
    new_pn->poly = poly[0] = poly0;
    poly[0]->n_vertices = 3;
    ListPush(*plist, new_pn);
    poly0 = &savedp;
  }
  for (i = (poly0->flags & POLY_SCRATCH) != 0; i < 2; i++) {
    new_pn = new_poly_list_node(tagged_app, scratch);
    new_pn->poly = poly[i] = new_poly(3, NULL, scratch);
    ListPush(*plist, new_pn);
  }
  
  for (i = 0; i < 2; i++) {
    poly[i]->pcol   = poly0->pcol;
    poly[i]->pn     = poly0->pn;
    poly[i]->flags |= poly0->flags;
  }
  
  /* copy over vertex information, the _REAL_ poly normal will be
   * computed later, the need for that is flagged by POLY_SCRATCH.
   */
  v = vertex;
  for (i = 0; i < 3; i++) {
    poly[0]->v[i] = poly0->v[v++];
    v %= 4;
  }
	
  v = (v - 1 + 4) % 4;
  for (i = 0; i < 3; i++) {
    poly[1]->v[i] = poly0->v[v++];
    v %= 4;
  }
}

static inline void meshv_to_polyv(Vertex *pv, Mesh *mesh, int vidx)
{
  memset(pv, 0, sizeof(Vertex));

  pv->pt = mesh->p[vidx];
#if 0
#if 0
  HPt3Dehomogenize(&pv->pt, &pv->pt);
#else
  Pt3ToHPt3((Point3 *)(void *)&pv->pt, &pv->pt, 1);
#endif
#endif
  if (mesh->geomflags & MESH_N) {
    pv->vn = mesh->n[vidx];
  }
  if (mesh->geomflags & MESH_C) {
    pv->vcol = mesh->c[vidx];
  }
  if (mesh->geomflags & MESH_U) {
    pv->st = mesh->u[vidx];
  }
}

static inline void
meshv_to_polyv_trans(Transform T, Transform Tdual, Transform TxT,
		     Vertex *pv, Mesh *mesh, int vidx)
{
  memset(pv, 0, sizeof(Vertex));
  HPt3Transform(T, &mesh->p[vidx], &pv->pt);
#if 0 /* why should we */
#if 0
  HPt3Dehomogenize(&pv->pt, &pv->pt);
#else
  Pt3ToHPt3((Point3 *)(void *)&pv->pt, &pv->pt, 1);
#endif
#endif
  if (mesh->geomflags & MESH_N) {
    NormalTransform(Tdual, &mesh->n[vidx], &pv->vn);
  }
  if (mesh->geomflags & MESH_C) {
    pv->vcol = mesh->c[vidx];
  }
  if (mesh->geomflags & MESH_U) {
    TxSTTransform(TxT, &mesh->u[vidx], &pv->st);
  }
}

static PolyListNode *
QuadToLinkedPolyList(Transform T, Transform Tdual, Transform TxT,
		     const void **tagged_app, PolyListNode **plistp,
		     Quad *quad, struct obstack *scratch)
{
  PolyListNode *plist = NULL;
  Poly *qpoly;
  int i, j, concave;

  if (!plistp) {
    plistp = &plist;
  }

  if(!(quad->geomflags & QUAD_N)) {
    QuadComputeNormals(quad);
  }
  
  qpoly = NULL;
  for (i = 0; i < quad->maxquad; i++) {
    /* First try to create a single polygon, if that mesh-cell is
     * non-flat or concave, then sub-divide it.
     */
    if (!qpoly) {
      qpoly = new_poly(4, NULL, scratch);
      for (j = 0; j < 4; j++) {
	qpoly->v[j] = obstack_alloc(scratch, sizeof(Vertex));
      }
    }
    if (T && T != TM_IDENTITY) {
      for (j = 0; j < 4; j++) {
	memset(qpoly->v[j], 0, sizeof(Vertex));
	HPt3Transform(T, &quad->p[i][j], &qpoly->v[j]->pt);
	NormalTransform(T, &quad->n[i][j], &qpoly->v[j]->vn);
      }
    } else {
      for (j = 0; j < 4; j++) {
	memset(qpoly->v[j], 0, sizeof(Vertex));
	qpoly->v[j]->pt   = quad->p[i][j];
	qpoly->v[j]->vn   = quad->n[i][j];
      }
    }
    qpoly->flags |= PL_HASVN;
    if (quad->geomflags & QUAD_C) {
      qpoly->flags |= PL_HASVCOL;
      for (j = 0; j < 4; j++) {
	qpoly->v[j]->vcol = quad->c[i][j];
      }
    }
    if (quad->geomflags & COLOR_ALPHA) {
      qpoly->flags |= COLOR_ALPHA;
    }
    PolyNormal(qpoly, &qpoly->pn,
	       quad->geomflags & VERT_4D, false, &qpoly->flags, &concave);
    qpoly->flags |= PL_HASPN;
    if (qpoly->flags & POLY_NOPOLY) {
      /* degenerated, skip it, but reuse the memory region.  */
      qpoly->flags = 0;
    } else if (qpoly->flags & (POLY_CONCAVE|POLY_NONFLAT)) {
      /* we need to split it */
      split_quad_poly(concave, qpoly, plistp, tagged_app, scratch);
      qpoly = NULL;
    } else {
      PolyListNode *new_pn;
      
      new_pn = new_poly_list_node(tagged_app, scratch);
      new_pn->poly = qpoly;
      ListPush(*plistp, new_pn);
      qpoly = NULL;
    }
  }
  
  return *plistp;
}

/* Convert a Mesh into linked list of PolyListNodes, subdivide
 * non-flat or concave quadrilaterals.
 */
static PolyListNode *
MeshToLinkedPolyList(Transform T, Transform Tdual, Transform TxT,
		     const void **tagged_app, PolyListNode **plistp,
		     Mesh *mesh, struct obstack *scratch)
{
  PolyListNode *plist = NULL;
  Poly *qpoly;
  int v0 = 1, prev0v = 0;
  int u0 = 1, prev0u = 0;
  int u, v, prevu, prevv;
  int concave;
  int i;

  if (!plistp) {
    plistp = &plist;
  }

  MeshComputeNormals(mesh, MESH_N);

  if(mesh->geomflags & MESH_UWRAP) {
    v0 = 0, prev0v = mesh->nv-1;
  }
  if(mesh->geomflags & MESH_VWRAP) {
    u0 = 0, prev0u = mesh->nu-1;
  }

#define MESHIDX(u, v, mesh) ((v)*(mesh)->nu + (u))
  qpoly = NULL;
  for(prevv = prev0v, v = v0; v < mesh->nv; prevv = v, v++) {
    for(prevu = prev0u, u = u0; u < mesh->nu; prevu = u, u++) {
      /* First try to create a single polygon, if that mesh-cell is
       * non-flat or concave, then sub-divide it.
       */
      if (!qpoly) {
	qpoly = new_poly(4, NULL, scratch);
	for (i = 0; i < 4; i++) {
	  qpoly->v[i] = obstack_alloc(scratch, sizeof(Vertex));
	}
      }
      if (T && T != TM_IDENTITY) {
	meshv_to_polyv_trans(T, Tdual, TxT, qpoly->v[0], mesh,
			     MESHIDX(prevu, prevv, mesh));
	meshv_to_polyv_trans(T, Tdual, TxT, qpoly->v[1], mesh,
			     MESHIDX(u, prevv, mesh));
	meshv_to_polyv_trans(T, Tdual, TxT, qpoly->v[2], mesh,
			     MESHIDX(u, v, mesh));
	meshv_to_polyv_trans(T, Tdual, TxT, qpoly->v[3], mesh,
			     MESHIDX(prevu, v, mesh));
      } else {
	meshv_to_polyv(qpoly->v[0], mesh, MESHIDX(prevu, prevv, mesh));
	meshv_to_polyv(qpoly->v[1], mesh, MESHIDX(u, prevv, mesh));
	meshv_to_polyv(qpoly->v[2], mesh, MESHIDX(u, v, mesh));
	meshv_to_polyv(qpoly->v[3], mesh, MESHIDX(prevu, v, mesh));
      }

      if (mesh->geomflags & MESH_C) {
	qpoly->flags |= PL_HASVCOL;
      }
      if (mesh->geomflags & COLOR_ALPHA) {
	qpoly->flags |= COLOR_ALPHA;
      }
      if (mesh->geomflags & MESH_N) {
	qpoly->flags |= PL_HASVN;
      }
      if (mesh->geomflags & MESH_U) {
	qpoly->flags |= PL_HASST;
      }

      PolyNormal(qpoly, &qpoly->pn, mesh->geomflags & VERT_4D, false,
		 &qpoly->flags, &concave);
      qpoly->flags |= PL_HASPN;
      if (qpoly->flags & POLY_NOPOLY) {
	/* polygon is degenerated, we just do NOT draw it, edges are
	 * drawn by other methods. Just do nothing, qpoly will be
	 * re-used for the next quad.
	 */
	qpoly->flags = 0;
      } else if (qpoly->flags & (POLY_CONCAVE|POLY_NONFLAT)) {
	/* we need to split it */
	split_quad_poly(concave, qpoly, plistp, tagged_app, scratch);
	qpoly = NULL;
      } else {
	PolyListNode *new_pn;
	
	new_pn = new_poly_list_node(tagged_app, scratch);
	new_pn->poly = qpoly;
	ListPush(*plistp, new_pn);
	qpoly = NULL;
      }
    }
  }

#undef MESHIDX
  
  return *plistp;
}

/* convert a PolyList into a linked list of PolyListNodes, subdivide
 * non-flat or concave polgons
 */
static PolyListNode *
PolyListToLinkedPoyList(Transform T, Transform Tdual, Transform TxT,
			const void **tagged_app,
			PolyListNode **plistp,
			PolyList *pl, struct obstack *scratch)
{
  PolyListNode *plist = NULL;
  int pnr;
  
  if (!plistp) {
    plistp = &plist;
  }

  PolyListComputeNormals(pl, PL_HASVN|PL_HASPN|PL_HASPFL);
  for (pnr = 0; pnr < pl->n_polys; pnr++) {
    PolyListNode *new_pn;
    Poly *poly;

    if (pl->p[pnr].flags & POLY_NOPOLY) {
      /* degenerated, just skip it */
      continue;
    }

    poly = &pl->p[pnr];
    poly->flags |= pl->geomflags;

    if (T && T != TM_IDENTITY) {
      poly = transform_poly(T, Tdual, TxT, poly, scratch);
    }

    switch (pl->p[pnr].n_vertices) {
    case 3: /* ok */
      new_pn = new_poly_list_node(tagged_app, scratch);
      new_pn->poly = poly;
      ListPush(*plistp, new_pn);
      break;
    case 4: /* supported */
      if (pl->p[pnr].flags & (POLY_NONFLAT|POLY_CONCAVE)) {
	/* split this polygon along a diagonal, if the polygon is
	 * concave: split across the unique concave vertex.
	 */
	int concave;

	if (pl->p[pnr].flags & POLY_CONCAVE) {
	  Point3 nu;

	  /* We need to determine the concave vertex */
	  PolyNormal(poly, &nu, pl->geomflags & VERT_4D, false, NULL,
		     &concave);
	} else {
	  concave= 0;
	}
	split_quad_poly(concave, poly, plistp, tagged_app, scratch);
      } else {
	new_pn = new_poly_list_node(tagged_app, scratch);
	new_pn->poly = poly;
	ListPush(*plistp, new_pn);
      }
      break;
    default:
      if (pl->p[pnr].flags & (POLY_NONFLAT|POLY_CONCAVE)) {
	static int was_here;
	
	if (!was_here ) {
	  GeomError(1, "Non-flat or concave polygons not supported yet.\n");
	  was_here = 1;
	}
      }
      new_pn = new_poly_list_node(tagged_app, scratch);
      new_pn->poly = poly;
      ListPush(*plistp, new_pn);
      break;
    }
  }
  return *plistp;
}

/* Create an empty BSP-tree and attach "object" to it. */
BSPTree *BSPTreeCreate(BSPTree *tree, Geom *object)
{
  if (tree) {
    BSPTreeFreeTree(tree);
  } else {
    tree = OOGLNewE(BSPTree, "new BSP tree root");
    memset(tree, 0, sizeof(BSPTree));
    obstack_init(&tree->obst);
  }

  tree->geom       = object;
  tree->T          = TM_IDENTITY;
  tree->TxT        = TM_IDENTITY;
  tree->tagged_app = NULL;

  return tree;
}

/* Add "object" to "bsptree"'s _INITIAL_ list of polygons;
 * bsptree->tree has to be NULL at this point, otherwise this is a
 * no-op.
 *
 * BUG: this should probably converted into a "method" which is
 * attached to each instance of a Geom. Do that later. If I find the
 * time. Never do that ...
 */
void BSPTreeAddObject(BSPTree *bsptree, Geom *object)
{
  if (bsptree->tree) {
    static int was_here;
    
    if (!was_here ) {
      GeomError(1, "Adding polygons to a finalized BSP-tree is "
		"not implemented.\n");
      was_here = 1;
    }
    return; /* Cannot add to finalized tree */
  }

  /* Make a linked list of polyhedrons from object */
  switch (object->magic) {
  case PLMAGIC:
    PolyListToLinkedPoyList(bsptree->T,
			    bsptree->Tdual,
			    bsptree->TxT,
			    bsptree->tagged_app,
			    &bsptree->init_lpl,
			    (PolyList *)object,
			    &bsptree->obst);
    break;
  case MESHMAGIC:
    MeshToLinkedPolyList(bsptree->T,
			 bsptree->Tdual,
			 bsptree->TxT,
			 bsptree->tagged_app,
			 &bsptree->init_lpl,
			 (Mesh *)object, &bsptree->obst);
    break;
  case QUADMAGIC:
    QuadToLinkedPolyList(bsptree->T,
			 bsptree->Tdual,
			 bsptree->TxT,
			 bsptree->tagged_app,
			 &bsptree->init_lpl,
			 (Quad *)object,
			 &bsptree->obst);
    break;
  default:
    /* Do nothing */
    break;
  }
}

/* The work-horse: really generate the tree, sub-divide as necessary.
 * We build a complete bsp-tree, i.e. each leaf-node contains only
 * co-planar polygons.
 */
void BSPTreeFinalize(BSPTree *bsptree)
{
  if (bsptree->tree) {
    return; 
  }

  /* After bsptree->tree has been allocated adding to the tree is no
   * longer possible.
   */
  bsptree->tree = obstack_alloc(&bsptree->obst, sizeof(BSPTreeNode));

  if (!bsptree->init_lpl) {
    /* create an empty tree */
    memset(bsptree->tree, 0, sizeof(BSPTreeNode));
    return;
  }
  
  /* Do it. */
  BSPTreeCreateRecursive(bsptree->tree, bsptree->init_lpl, &bsptree->obst);

  /* We build a complete tree, and discard all degenerated polygons,
   * so the polygon list is empty after creating the tree.
   */
  bsptree->init_lpl = NULL;
}

void BSPTreeFreeTree(BSPTree *tree)
{
  if (tree->tree != NULL || tree->init_lpl != NULL) {
    obstack_free(&tree->obst, NULL);
    obstack_init(&tree->obst);
    tree->tree       = NULL;
    tree->init_lpl   = NULL;
    tree->Tid        = NULL;
    tree->Tidinv     = NULL;
    tree->tagged_app = NULL;
    tree->oneshot    = false;
  }
}

/* Geometries attached to INSTs or LISTs can be deleted underneath the
 * BSP-tree; so INSTs and LISTs must register a callback with their
 * handles.
 */
void BSPTreeInvalidate(Handle **chp, Ref *unused, BSPTree *self)
{
  BSPTreeFreeTree(self);
}

/* This is really easy, 'cause we are using an obstack. */
void BSPTreeFree(BSPTree *tree)
{
  if (tree != NULL) {
    obstack_free(&tree->obst, NULL);
    OOGLFree(tree);
  }
}

BSPTree *BSPTreeSet(BSPTree *tree, int attr1, ...)
{
  int attr;
  va_list alist;
#undef NEXT
#define NEXT(type) va_arg(alist, type)

  va_start(alist, attr1);
  for (attr = attr1; attr != BSPTREE_END; attr = NEXT(int)) {
    switch (attr) {
    case BSPTREE_ONESHOT:
      tree->oneshot = NEXT(int);
      break;
    default:
      OOGLError(1, "BSPTreeSet: unknown attribute %d", attr);
      break;
    }
  }
  va_end(alist);

  return tree;
}

BSPTree *GeomBSPTree(Geom *geom, BSPTree *tree, int action)
{
  const void **tagged_app = NULL;

  if (!geom) {
    return NULL;
  }

  if (!geom->Class->bsptree) {
    return NULL;
  }

  switch (action) {
  case BSPTREE_CREATE:
    if (tree == NULL) {
      geom->bsptree = tree = BSPTreeCreate(geom->bsptree, geom);
    }
    break;
  case BSPTREE_ADDGEOM:
    if (geom == tree->geom) {
      Transform T;
      
      /* make sure the top-level geom has per-node data */
      GeomNodeDataCreate(geom, NULL);

      mggettransform(T);
      if (memcmp(T, TM_IDENTITY, sizeof(Transform)) != 0) {
	tree->Tid = obstack_alloc(&tree->obst, sizeof(Transform));
	TmCopy(T, tree->Tid);
      } else {
	tree->Tid = TM_IDENTITY;
      }
      tree->Tidinv = NULL;
    }
    tagged_app = BSPTreePushAppearance(tree, geom);
    break;
  case BSPTREE_DELETE:
    if (tree == NULL || (tree = geom->bsptree) == NULL) {
      return NULL;
    }
    break;
  default:
    break;
  }
  
  (*geom->Class->bsptree)(geom, tree, action);
  
  switch (action) {    
  case BSPTREE_DELETE:
    if (tree->geom == geom) {
      BSPTreeFree(tree);
      tree = geom->bsptree = NULL;
    }
    break;
  case BSPTREE_ADDGEOM:
    BSPTreePopAppearance(tree, tagged_app);
    break;
  default:
    break;
  }

  return tree;
}

Geom *GeomBSPTreeDraw(Geom *geom)
{
  BSPTree *tree = geom->bsptree;
  
  if (tree == NULL || !(geom->geomflags & GEOM_ALPHA)) {
    return NULL;
  }

  /* 3d-view: tree might be persistent, but is only created on
   * demand. Do that now.
   *
   * Nd-view: tree is not persistent, but tree->init_lpl should be !=
   * NULL at this stage.
   */
  if (tree->tree == NULL) {
    if (tree->init_lpl == NULL) {
      GeomBSPTree((Geom *)geom, tree, BSPTREE_ADDGEOM);
    }
    BSPTreeFinalize(tree);
  }

  mgbsptree(tree);

  if (tree->oneshot) {
    /* This means we have an INST with location/origin != LOCAL; or
     * ND-drawing. Free the tree now.
     */
    BSPTreeFreeTree(tree);
  }

  return geom;
}

static inline void CoLinSum(float a, ColorA *ca, float b, ColorA *cb,
			    ColorA *cres)
{
  cres->r = a*ca->r + b*cb->r;
  cres->g = a*ca->g + b*cb->g;
  cres->b = a*ca->b + b*cb->b;
  cres->a = a*ca->a + b*cb->a;
}

/* Split plnode along plane. We know that plane really intersects
 * plnode->poly. We also know that plnode->poly is planar and convex.
 *
 * Given this assumptions we know that plnode->poly has to be split
 * into exactly two pieces. 
 */
static inline void SplitPolyNode(PolyListNode *plnode,
				 PolyListNode **front, PolyListNode **back,
				 EdgeIntersection edges[2],
				 struct obstack *scratch)
{
  const void **tagged_app = plnode->tagged_app;
  Poly *poly, savedp;
  Vertex *v0, *v1, **vpos, **savedv;
  int istart[2], iend[2], i, nv[2];
  Vertex *vstart[2], *vend[2];
  
  poly = plnode->poly;

  vstart[0] = vstart[1] = vend[0] = vend[1] = NULL;
  istart[0] = istart[1] = iend[0] = iend[1] = -1;

  /* first point of intersection */
  if (fzero(edges[0].scp[0])) {
    v0 = poly->v[edges[0].v[0]];
    if (fpos(edges[0].scp[1])) {
      istart[0] = edges[0].v[0];
      iend[1]   = edges[0].v[0];
    } else {
      istart[1] = edges[0].v[0];
      iend[0]   = edges[0].v[0]; 
    }
  } else if (fzero(edges[0].scp[1])) {
    v0 = poly->v[edges[0].v[1]];
    if (fpos(edges[0].scp[0])) {
      istart[1] = edges[0].v[1];
      iend[0]   = edges[0].v[1];
    } else {
      istart[0] = edges[0].v[1];
      iend[1]   = edges[0].v[1]; 
    }
  } else {
    HPt3Coord mu0, mu1;
    Vertex *V0 = poly->v[edges[0].v[0]];
    Vertex *V1 = poly->v[edges[0].v[1]];
    
    v0 = obstack_alloc(scratch, sizeof(Vertex));
    mu0 = edges[0].scp[1]/(edges[0].scp[1]-edges[0].scp[0]);
#if 0
    mu1 = edges[0].scp[0]/(edges[0].scp[0]-edges[0].scp[1]);
#else
    mu1 = 1.0 - mu0;
#endif
    /* Use denormalized variant; otherwise textures may come out wrong
     * because the homogeneous divisor is used for perspective
     * corrections.
     */
    HPt3LinSumDenorm(mu0, &V0->pt, mu1, &V1->pt, &v0->pt);
    if (!finite(v0->pt.x + v0->pt.y + v0->pt.z)){
      abort();
    }
    CoLinSum(mu0, &V0->vcol, mu1, &V1->vcol, &v0->vcol);
    /* The averaged vertex normals do not have an orientation, so try
     * so orient them w.r.t. the polygon normal before computing the
     * linear combination.
     */
    if (Pt3Dot(&V0->vn, &poly->pn)*Pt3Dot(&V1->vn, &poly->pn) < 0) {
      Pt3Comb(-mu0, &V0->vn, mu1, &V1->vn, &v0->vn);
    } else {
      Pt3Comb(mu0, &V0->vn, mu1, &V1->vn, &v0->vn);
    }
    Pt3Unit(&v0->vn);
    v0->st.s = mu0 * V0->st.s + mu1 * V1->st.s;
    v0->st.t = mu0 * V0->st.t + mu1 * V1->st.t;

    if (fpos(edges[0].scp[0])) {
      vstart[1] = vend[0] = v0;
      istart[1] = edges[0].v[1];
      iend[0]   = edges[0].v[0];
    } else {
      vstart[0] = vend[1] = v0;
      istart[0] = edges[0].v[1];
      iend[1]   = edges[0].v[0];
    }
  }

  /* second point of intersection */
  if (fzero(edges[1].scp[0])) {
    v1 = poly->v[edges[1].v[0]];
    if (fpos(edges[1].scp[1])) {
      istart[0] = edges[1].v[0];
      iend[1]   = edges[1].v[0];
    } else {
      istart[1] = edges[1].v[0];
      iend[0]   = edges[1].v[0]; 
    }
  } else if (fzero(edges[1].scp[1])) {
    v1 = poly->v[edges[1].v[1]];
    if (fpos(edges[1].scp[0])) {
      istart[1] = edges[1].v[1];
      iend[0]   = edges[1].v[1];
    } else {
      istart[0] = edges[1].v[1];
      iend[1]   = edges[1].v[1]; 
    }
  } else {
    HPt3Coord mu0, mu1;
    Vertex *V0 = poly->v[edges[1].v[0]];
    Vertex *V1 = poly->v[edges[1].v[1]];
    
    v1 = obstack_alloc(scratch, sizeof(Vertex));
    mu0 = edges[1].scp[1]/(edges[1].scp[1]-edges[1].scp[0]);
#if 0
    mu1 = edges[1].scp[0]/(edges[1].scp[0]-edges[1].scp[1]);
#else
    mu1 = 1.0 - mu0;
#endif
    HPt3LinSumDenorm(mu0, &V0->pt, mu1, &V1->pt, &v1->pt);
    if (!finite(v1->pt.x + v1->pt.y + v1->pt.z))
      abort();
    CoLinSum(mu0, &V0->vcol, mu1, &V1->vcol, &v1->vcol);
    if (Pt3Dot(&V0->vn, &poly->pn)*Pt3Dot(&V1->vn, &poly->pn) < 0) {
      Pt3Comb(-mu0, &V0->vn, mu1, &V1->vn, &v1->vn);
    } else {
      Pt3Comb(mu0, &V0->vn, mu1, &V1->vn, &v1->vn);
    }
    Pt3Unit(&v1->vn);
    v1->st.s = mu0 * V0->st.s + mu1 * V1->st.s;
    v1->st.t = mu0 * V0->st.t + mu1 * V1->st.t;

    if (fpos(edges[1].scp[0])) {
      vstart[1] = vend[0] = v1;
      istart[1] = edges[1].v[1];
      iend[0]   = edges[1].v[0];
    } else {
      vstart[0] = vend[1] = v1;
      istart[0] = edges[1].v[1];
      iend[1]   = edges[1].v[0];
    }
  }

  ListPush(*front, plnode);
  ListPush(*back, new_poly_list_node(tagged_app, scratch));

  if ((poly->flags & POLY_NONFLAT)) {
    if (!(*front)->pn) {
      /* Compute the normal on the parent element to avoid numerical
       * instabilities on increasingly degenerated polygons.
       */
      (*front)->pn = obstack_alloc(scratch, sizeof(Point3));
      PolyNormal(poly, (*front)->pn,
		 true /* 4d */, false /* evert */, NULL, NULL);
    }
    (*back)->pn = (*front)->pn;
  }

  for (i = 0; i < 2; i++) {
    nv[i] = iend[i] - istart[i] + 1;
    if (nv[i] < 0) {
      nv[i] += poly->n_vertices;
    }
    nv[i] += (vstart[i] != NULL) + (vend[i] != NULL);
  }

  if (poly->flags & POLY_SCRATCH) {
    savedp = *poly;
    savedv = alloca(poly->n_vertices*sizeof(Vertex *));
    memcpy(savedv, poly->v, poly->n_vertices*sizeof(Vertex *));

    if (nv[0] <= poly->n_vertices) {
      poly->n_vertices = nv[0];
      (*front)->poly = poly;
      (*back)->poly  = new_poly(nv[1], NULL, scratch);
    } else {
      if (nv[1] > poly->n_vertices) {
	abort();
      }
      poly->n_vertices = nv[1];
      (*back)->poly = poly;
      (*front)->poly  = new_poly(nv[0], NULL, scratch);
    }
    
    /* Attention: gcc had problems with this code snippet with
     * -fstrict-aliasing, the "#if 1" stuff seems to work. In the
     * "#else" version gcc somehow lost the "savedp.v = savedv"
     * assignment. I think this is a compiler bug.
     */
    poly = &savedp;
#if 1
    poly->v = savedv;
#else
    savedp.v = savedv;
#endif
  } else {
    (*front)->poly  = new_poly(nv[0], NULL, scratch);
    (*back)->poly  = new_poly(nv[1], NULL, scratch);
  }

  for (i = 0; i < 2; i++) {
    PolyListNode *half = (i == 0) ? *front : *back;
    int j;

    vpos = half->poly->v;
    if (vstart[i] != NULL) {
      *vpos++ = vstart[i];
    }
    if (istart[i] <= iend[i]) {
      for (j = istart[i]; j <= iend[i] && j < poly->n_vertices; j++) {
	*vpos++ = poly->v[j];
      }
    } else {
      for (j = istart[i]; j < poly->n_vertices; j++) {
	*vpos++ = poly->v[j];
      }
      for (j = 0; j <= iend[i]; j++) {
	*vpos++ = poly->v[j];
      }
    }
    if (vend[i] != NULL) {
      *vpos++ = vend[i];
    }
    half->poly->pcol  = poly->pcol;
    half->poly->pn    = poly->pn;
    half->poly->flags = poly->flags|POLY_SCRATCH;

    check_poly(half->poly);
  }
}

/* Compute the plane equation for this polygon; copy the normal for
 * originally flat polygons, really compute it for polygons created by
 * sub-division from non-flat polygons.
 */
static inline void PolyPlane(PolyListNode *plnode, HPoint3 *plane)
{
  if (plnode->pn) {
    *(Point3 *)plane = *plnode->pn;
  } else if (plnode->poly->flags & POLY_NONFLAT) {
    /* The polygon actually _IS_ flat -- BSPTreeCreate() is
     * responsible to make that sure, but we have to compute the
     * normal ourselves. We do not need to remember the normal because
     * when this function is called, then plnode has found its "home"
     * tree-node.
     */
    PolyNormal(plnode->poly, (Point3 *)(void *)plane,
	       true /* fourd */, false /* evert */, NULL, NULL);
  } else {
    *(Point3 *)plane = plnode->poly->pn;
  }
  plane->w = HPt3DotPt3(&plnode->poly->v[0]->pt, (Point3 *)(void *)plane);
}

static inline HPt3Coord PlaneDistance(HPoint3 *plane, HPoint3 *v)
{
  HPt3Coord dist;
  
  dist = HPt3DotPt3(v, (Point3 *)plane) - plane->w;
  if (!finite(dist)) {
    dist = 1e8; /* infinity */
  }
  return dist;
}

/* Loop over all vertices and determine on which side of the plane
 * defined by "n" we live, if not on both sides. We assume at this
 * point that the polygon is _FLAT_. The main entry-point
 * BSPTreeCreate() has to split polygons such that this is true. Maybe
 * we will allow for non-convex polygons here, but _FLAT_ is a must.
 */
static inline PolyPos ClassifyPoly(HPoint3 *plane, Poly *poly,
				   EdgeIntersection edges[2])
{
  HPt3Coord scp0, scp1, scp2, scp3;
  PolyPos sign0, sign1, sign2, sign3;
  int i, i0, i2;

  scp0 = PlaneDistance(plane, &poly->v[0]->pt);
  sign0 = fpos(scp0) - fneg(scp0);
  if (sign0 == COPLANAR) {
    for (i = 1; i < poly->n_vertices; i++) {
      scp1 = PlaneDistance(plane, &poly->v[i]->pt);
      sign1 = fpos(scp1) - fneg(scp1);
      if (sign1 != COPLANAR) {
	break;
      }
      scp0 = scp1;
      sign0 = sign1;
    }
    if (i >= 2) {
      return sign1;
    }
    /* at this point: sign0 == 0 and sign1 != 0, loop until we find
     * the next zero crossing.
     */
    i0 = 0;
    sign2 = sign1;
    scp2 = scp1;
    for (++i; i < poly->n_vertices; i++) {
      scp3 = PlaneDistance(plane, &poly->v[i]->pt);
      sign3 = fpos(scp3) - fneg(scp3);
      if (sign3 != sign2) {
	break;
      }
      scp2 = scp3;
      sign2 = sign3;
    }
    if (i == poly->n_vertices) {
      return sign1;
    }
    /* At this point we have sign0 == 0 != sign1 == sign2 != sign3. If
     * sign3 == 0, then the next vertex may also be located on the
     * plane => sign1 == sign2 determine the side we are located on.
     *
     * Otherwise sign3 must be != sign1 and we have to sub-divide this
     * polygon.
     */
    if (sign3 == COPLANAR) {
      if (i == poly->n_vertices-1) {
	return sign1;
      }
      scp2 = scp3;
      sign2 = sign3;
      scp3 = PlaneDistance(plane, &poly->v[++i]->pt);
      sign3 = fpos(scp3) - fneg(scp3);
      if (sign3 == COPLANAR) {
	return sign1;
      } else if (sign3 == sign1) {
	/* impossible case with exact arithmetic; this should mean
	 * that we are in the COPLANAR case. Assume that? Or retry
	 * with increased telerance?
	 */
	return COPLANAR; /* FIXME */
      }
    }
    /* At this point we have sign0 == 0 != sign1. sign2 may be 0, then
     * sign3 != sign1. Or 0 == sign0 != sign1 == sign2 != sign 3. At
     * any rate we are located on both sides.
     */
    i2 = i-1;
  } else {
    /* Loop until we find a change of sign */
    for (i = 1; i < poly->n_vertices; i++) {
      scp1 = PlaneDistance(plane, &poly->v[i]->pt);
      sign1 = fpos(scp1) - fneg(scp1);
      if (sign1 != sign0) {
	break;
      }
      scp0 = scp1;
      sign0 = sign1;
    }
    if (i == poly->n_vertices) {
      return sign1;
    }
    i0 = i-1;
    sign2 = sign1;
    scp2 = scp1;
    if (sign2 == COPLANAR) {
      /* if sign2 is accidentally 0, then the next point must be != 0,
       * or sign0 determines the proper side.
       */
      ++i;
      i %= poly->n_vertices;

      scp3 = PlaneDistance(plane, &poly->v[i]->pt);
      sign3 = fpos(scp3) - fneg(scp3);
      if (sign3 == COPLANAR || sign3 == sign0) {
	return sign0;
      }
      scp2 = scp3;
      sign2 = sign3;
    }
    /* loop until we find the next zero crossing, at this point we
     * have 0 != sign0 != sign1, 0 != sign2 != sign0, the polygon
     * cannot be on one side of the plane.
     */
    for (++i; i < poly->n_vertices; i++) {
      scp3 = PlaneDistance(plane, &poly->v[i]->pt);
      sign3 = fpos(scp3) - fneg(scp3);
      if (sign3 != sign2) {
	break;
      }
      scp2 = scp3;
      sign2 = sign3;
    }
    if (i == poly->n_vertices) {
      if (i0 == 0) {
	scp3 = scp0;
	sign3 = sign0;
      } else {
	scp3 = PlaneDistance(plane, &poly->v[0]->pt);
	sign3 = fpos(scp3) - fneg(scp3);
      }
    }
    i2 = i-1;
  }
  /* points of intersection between [i0,i0+1], [i2,i2+1] */
  edges[0].v[0] = i0;
  edges[0].v[1] = i0+1;
  edges[0].scp[0] = scp0;
  edges[0].scp[1] = scp1;

  edges[1].v[0] = i2;
  edges[1].v[1] = (i2+1) % poly->n_vertices;
  edges[1].scp[0] = scp2;
  edges[1].scp[1] = scp3;

  return BOTH_SIDES;
}

static void BSPTreeCreateRecursive(BSPTreeNode *tree,
				   PolyListNode *pllist,
				   struct obstack *scratch)
{
  PolyListNode *plnode, *front, *back;
  EdgeIntersection edges[2];

  tree->front = tree->back = NULL;
  ListPop(pllist, plnode);
  tree->polylist = plnode;
  check_poly(plnode->poly);
  PolyPlane(plnode, &tree->plane);

  front = back = NULL;
  while (pllist) {    
    ListPop(pllist, plnode);
    check_poly(plnode->poly);
    switch (ClassifyPoly(&tree->plane, plnode->poly, edges)) {
    case BACKOF:
      check_poly(plnode->poly);
      ListPush(back, plnode);
      break;
    case COPLANAR:
      check_poly(plnode->poly);
      ListPush(tree->polylist, plnode);
      break;
    case INFRONTOF:
      check_poly(plnode->poly);
      ListPush(front, plnode);
      break;
    case BOTH_SIDES:
      check_poly(plnode->poly);
      SplitPolyNode(plnode, &front, &back, edges, scratch);
      break;
    }
  }
  if (front) {
    tree->front = obstack_alloc(scratch, sizeof(*tree->front));
    BSPTreeCreateRecursive(tree->front, front, scratch);
  }
  if (back) {
    tree->back = obstack_alloc(scratch, sizeof(*tree->back));
    BSPTreeCreateRecursive(tree->back, back, scratch);
  }
}

/*
 * Local Variables: ***
 * c-basic-offset: 2 ***
 * End: ***
 */

	
