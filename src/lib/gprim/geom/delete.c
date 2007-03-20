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

#include "geomclass.h"
#include "handleP.h"
#include "mg.h"

int PoolDoCacheFiles;

DBLLIST(GeomNodeDataPool);

static inline void GeomNodeDataPrune(Geom *geom)
{
  NodeData *data, *data_next;

  DblListIterate(&geom->pernode, NodeData, node, data, data_next) {
    DblListDelete(&data->node);
    if (data->tagged_ap) {
      mguntagappearance(data->tagged_ap);
    }
    if (data->ppath) {
	free(data->ppath);
	data->ppath = NULL;
    }
    DblListAdd(&GeomNodeDataPool, &data->node);
  }
}

void GeomDelete(Geom *object)
{
    Handle *h;
    Pool *p;
    int np;

    if (object == NULL) {
	return;
    }

    if (!GeomIsMagic(object->magic)) {
	OOGLWarn("Internal warning: GeomDelete of non-Geom %x (%x !~ %xxxxx)",
	    object, object->magic, (GeomMagic(0,0)>>16)&0xFFFF);
	return;
    }
    /* If we're not caching contents of files, and this object was loaded
     * from a file, and the sole reference to it is from the Handle,
     * delete it now (and delete the handle and possibly close the file).
     */
    for (np = 0, h = HandleRefIterate((Ref *)object, NULL);
	 h;
	 h = HandleRefIterate((Ref *)object, h)) {
	if ((p = HandlePool(h)) != NULL && !PoolDoCacheFiles) {
	    np++;
	}
	REFPUT(h);
    }
    if (REFPUT(object) == np && np > 0) {
	/* can this happen??? at all ??? */
	for (h = HandleRefIterate((Ref *)object, NULL);
	     h;
	     h = HandleRefIterate((Ref *)object, h)) {
	    REFPUT(h);
	    if ((p = HandlePool(h)) != NULL && !PoolDoCacheFiles) {
		HandleDelete(h);
	    }
	}
	return;
    } else if (REFCNT(object) < 0 || REFCNT(object) > 100000) {
	/* XXX debug */
	OOGLError(1, "GeomDelete(%x) -- ref count %d?", object, REFCNT(object));
	return;
    } else if (REFCNT(object) > 0) {
	return;
    }

    /* Actually delete it */;

    /* we may need to iterate over a list, or access INST->geom, so
     * call the destructor for the BSP-tree before calling
     * Class->Delete()
     */
    GeomBSPTree(object, NULL, BSPTREE_DELETE);
    GeomNodeDataPrune(object);

    if(object->aphandle) {
	HandlePDelete(&object->aphandle);
    }
    if(object->ap) {
	ApDelete(object->ap);
	object->ap = NULL;
    }
    if(object->Class->Delete) {
	(*object->Class->Delete)(object);
    }

    object->magic ^= 0x80000000;
    OOGLFree(object);
}

/*
 * Local Variables: ***
 * mode: c ***
 * c-basic-offset: 4 ***
 * End: ***
 */
