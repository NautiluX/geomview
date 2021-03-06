/* Copyright (C) 2007 Claus-Justus Heine
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

/* Implement Geomview's "apply = blend" for constant shading
 *
 * Maybe mgrib should set bgalpha to 0 if transparency is not enabled
 * (0 because this will leave the alpha channel of the surface
 * unaffected).
 *
 * The additional parameter At interpolates between 1 and Ot; the
 * effective alpha contribution from the texture will be (1-At) + At *
 * Ot.
 */
surface
GVreplaceconstant(string texturename = "";
		  float At = 1;)
{
  /* texture provided color (luminance) and alpha */
  color Ct;
  float Ot;
  /* number of texture channels,
   * < 3: liminance and possibly alpha, > 2: rgb and possibly alpha
   */
  float channels;
  
  if (texturename != "" &&
      textureinfo(texturename, "channels", channels) == 1.0) {
    if (channels < 3) {
      Ct = float texture (texturename[0], "width", 0.0);
      if (channels == 2) {
	Ot = float texture (texturename[1], "width", 0.0);
      }
    } else {
      Ct = color texture (texturename, "width", 0.0);
      if (channels == 4) {
	Ot = float texture (texturename[3], "width", 0.0);
      }
    }
    Ot = (1.0 - At) + At * Ot;
    Ci = Ct * Ot;
    Oi = Ot;
  } else {
    /* no texture: use ordinary constant shader */
    Ci = Cs;
    Oi = Os;
    Ci *= Os;
  }

}

/*
 * Local Variables: ***
 * mode: c ***
 * c-basic-offset: 2 ***
 * End: ***
 */
