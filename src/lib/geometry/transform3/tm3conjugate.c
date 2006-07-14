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


/* Authors: Charlie Gunn, Pat Hanrahan, Stuart Levy, Tamara Munzner, Mark Phillips */

#include "transform3.h"

/*-----------------------------------------------------------------------
 * Function:	Tm3Conjugate
 * Description:	conjugate one transform by another
 * Args:	T: one transform ("conjugatee") (INPUT)
 *		Tcon: another transform ("conjugater") (INPUT)
 *		Tres: the result
 * Returns:	nothing
 * Author:	mbp
 * Date:	Thu Aug  8 15:42:41 1991
 * Notes:	Sets Tres = Inverse[Tcon} * T * Tcon
 */
void
Tm3Conjugate( T, Tcon, Tres )
    Transform3 T, Tcon, Tres;
{
    Transform3 Tconinv;

    /* We actually use adjoint instead of inverse, because the det
       factors would cancel out anyway */
    Tm3Adjoint( Tcon, Tconinv );
    Tm3Concat( Tconinv, T, Tres );
    Tm3Concat( Tres, Tcon, Tres );
}
