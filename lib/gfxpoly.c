/* gfxpoly.c 

   Various boolean polygon functions.

   Part of the swftools package.

   Copyright (c) 2005 Matthias Kramm <kramm@quiss.org> 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "../config.h"
#include "gfxdevice.h"
#include "gfxtools.h"
#include "gfxpoly.h"
#include "art/libart.h"
#include <assert.h>
#include <memory.h>
#include <math.h>

static ArtVpath* gfxline_to_ArtVpath(gfxline_t*line, char fill)
{
    ArtVpath *vec = NULL;
    int pos=0,len=0;
    gfxline_t*l2;
    double x=0,y=0;

    /* factor which determines into how many line fragments a spline is converted */
    double subfraction = 2.4;//0.3

    l2 = line;
    while(l2) {
	if(l2->type == gfx_moveTo) {
	    pos ++;
	} if(l2->type == gfx_lineTo) {
	    pos ++;
	} if(l2->type == gfx_splineTo) {
            int parts = (int)(sqrt(fabs(l2->x-2*l2->sx+x) + fabs(l2->y-2*l2->sy+y))*subfraction);
            if(!parts) parts = 1;
            pos += parts + 1;
	}
	x = l2->x;
	y = l2->y;
	l2 = l2->next;
    }
    pos++;
    len = pos;

    vec = art_new (ArtVpath, len+1);

    pos = 0;
    l2 = line;
    while(l2) {
	if(l2->type == gfx_moveTo) {
	    vec[pos].code = ART_MOVETO;
	    vec[pos].x = l2->x;
	    vec[pos].y = l2->y;
	    pos++; 
	    assert(pos<=len);
	} else if(l2->type == gfx_lineTo) {
	    vec[pos].code = ART_LINETO;
	    vec[pos].x = l2->x;
	    vec[pos].y = l2->y;
	    pos++; 
	    assert(pos<=len);
	} else if(l2->type == gfx_splineTo) {
	    int i;
            int parts = (int)(sqrt(fabs(l2->x-2*l2->sx+x) + fabs(l2->y-2*l2->sy+y))*subfraction);
	    double stepsize = parts?1.0/parts:0;
	    for(i=0;i<=parts;i++) {
		double t = (double)i*stepsize;
		vec[pos].code = ART_LINETO;
		vec[pos].x = l2->x*t*t + 2*l2->sx*t*(1-t) + x*(1-t)*(1-t);
		vec[pos].y = l2->y*t*t + 2*l2->sy*t*(1-t) + y*(1-t)*(1-t);
		pos++;
		assert(pos<=len);
	    }
	}
	x = l2->x;
	y = l2->y;
	l2 = l2->next;
    }
    vec[pos].code = ART_END;
   
    if(!fill) {
	/* Fix "dotted" lines. Those are lines where singular points are created
	   by a moveto x,y lineto x,y combination. We "fix" these by shifting the
	   point in the lineto a little bit to the right 
	   These should only occur in strokes, not in fills, so do this only
	   when we know we're not filling.
	 */
	int t;
	for(t=0;vec[t].code!=ART_END;t++) {
	    if(t>0 && vec[t-1].code==ART_MOVETO && vec[t].code==ART_LINETO 
		    && vec[t+1].code!=ART_LINETO
		&& vec[t-1].x == vec[t].x
		&& vec[t-1].y == vec[t].y) {
		vec[t].x += 0.01;
	    }
	    x = vec[t].x;
	    y = vec[t].y;
	}
    }

    /* Find adjacent identical points. If an ajdacent pair of identical
       points is found, the moveto is removed (unless both are movetos).
       So moveto x,y lineto x,y becomes lineto x,y
          lineto x,y lineto x,y becomes lineto x,y
          lineto x,y moveto x,y becomes lineto x,y
          moveto x,y moveto x,y becomes moveto x,y
     */
    int t;
    while(t < pos)
    {
	int t = 1;
	if ((vec[t-1].x == vec[t].x) && (vec[t-1].y == vec[t].y)) {
	    // adjacent identical points; remove one
	    int type = ART_MOVETO;
	    if(vec[t-1].code==ART_LINETO || vec[t].code==ART_LINETO)
		type = ART_LINETO;
	    memcpy(&vec[t], &vec[t+1], sizeof(vec[0]) * (pos - t));
	    vec[t].code = type;
	    pos--;
	} else {
	    t++;
	}
    }

    /* adjacency remover disabled for now, pending code inspection */
    return vec;

    // Check for further non-adjacent identical points. We don't want any
    // points other than the first and last points to exactly match.
    //
    // If we do find matching points, move the second point slightly. This
    // currently moves the duplicate 2% towards the midpoint of its neighbours
    // (easier to calculate than 2% down the perpendicular to the line joining
    // the neighbours) but limiting the change to 0.1 twips to avoid visual
    // problems when the shapes are large. Note that there is no minimum
    // change: if the neighbouring points are colinear and equally spaced,
    // e.g. they were generated as part of a straight spline, then the
    // duplicate point may not actually move.
    //
    // The scan for duplicates algorithm is quadratic in the number of points:
    // there's probably a better method but the numbers of points is generally
    // small so this will do for now.
    {
	int i = 1, j;
	for(; i < (pos - 1); ++i)
	{
	    for (j = 0; j < i; ++j)
	    {
		if ((vec[i].x == vec[j].x)
		    && (vec[i].y == vec[j].y))
		{
		    // points match; shuffle point
		    double dx = (vec[i-1].x + vec[i+1].x - (vec[i].x * 2.0)) / 100.0;
		    double dy = (vec[i-1].y + vec[i+1].y - (vec[i].y * 2.0)) / 100.0;
		    double dxxyy = (dx*dx) + (dy*dy);
		    if (dxxyy > 0.01)
		    {
			// This is more than 0.1 twip's distance; scale down
			double dscale = sqrt(dxxyy) * 10.0;
			dx /= dscale;
			dy /= dscale;
		    };
		    vec[i].x += dx;
		    vec[i].y += dy;
		    break;
		}
	    }
	}
    }

    return vec;
}

void show_path(ArtSVP*path)
{
    int t;
    printf("Segments: %d\n", path->n_segs);
    for(t=0;t<path->n_segs;t++) {
	ArtSVPSeg* seg = &path->segs[t];
	printf("Segment %d: %d points, %s, BBox: (%f,%f,%f,%f)\n", 
		t, seg->n_points, seg->dir==0?"UP  ":"DOWN",
		seg->bbox.x0, seg->bbox.y0, seg->bbox.x1, seg->bbox.y1);
	int p;
	for(p=0;p<seg->n_points;p++) {
	    ArtPoint* point = &seg->points[p];
	    printf("        (%f,%f)\n", point->x, point->y);
	}
    }
    printf("\n");
}

ArtSVP* gfxfillToSVP(gfxline_t*line, int perturb)
{
    ArtVpath* vec = gfxline_to_ArtVpath(line, 1);
    if(perturb) {
	ArtVpath* vec2 = art_vpath_perturb(vec);
	free(vec);
	vec = vec2;
    }
    ArtSVP *svp = art_svp_from_vpath(vec);
    free(vec);

    // We need to make sure that the SVP we now have bounds an area (i.e. the
    // source line wound anticlockwise) rather than excludes an area (i.e. the
    // line wound clockwise). It seems that PDF (or xpdf) is less strict about
    // this for bitmaps than it is for fill areas.
    //
    // To check this, we'll sum the cross products of all pairs of adjacent
    // lines. If the result is positive, the direction is correct; if not, we
    // need to reverse the sense of the SVP generated. The easiest way to do
    // this is to flip the up/down flags of all the segments.
    //
    // This is approximate; since the gfxline_t structure can contain any
    // combination of moveTo, lineTo and splineTo in any order, not all pairs
    // of lines in the shape that share a point need be described next to each
    // other in the sequence. For ease, we'll consider only pairs of lines
    // stored as lineTos and splineTos without intervening moveTos.
    //
    // TODO is this a valid algorithm? My vector maths is rusty.
    //
    // It may be more correct to instead reverse the line before we feed it
    // into gfxfilltoSVP. However, this seems equivalent and is easier to
    // implement!
    double total_cross_product = 0.0;
    gfxline_t* cursor = line;
    if (cursor != NULL)
    {
	double x_last = cursor->x;
	double y_last = cursor->y;
	cursor = cursor->next;

	while((cursor != NULL) && (cursor->next != NULL))
	{
	    if (((cursor->type == gfx_lineTo) || (cursor->type == gfx_splineTo))
		&& ((cursor->next->type == gfx_lineTo) || (cursor->next->type == gfx_splineTo)))
	    {
		// Process these lines
		//
		// In this space (x right, y down) the cross-product is
		// (x1 * y0) - (x0 * y1)
		double x0 = cursor->x - x_last;
		double y0 = cursor->y - y_last;
		double x1 = cursor->next->x - cursor->x;
		double y1 = cursor->next->y - cursor->y;
		total_cross_product += (x1 * y0) - (x0 * y1);
	    }

	    x_last = cursor->x;
	    y_last = cursor->y;
	    cursor = cursor->next;
	}
    }
    if (total_cross_product < 0.0)
    {
	int i = 0;
	for(; i < svp->n_segs; ++i)
	{
	    if (svp->segs[i].dir != 0)
		svp->segs[i].dir = 0;
	    else
		svp->segs[i].dir = 1;
	}
    }
    return svp;
}

gfxline_t* gfxpoly_to_gfxline(gfxpoly_t*poly)
{
    ArtSVP*svp = (ArtSVP*)poly;
    int size = 0;
    int t;
    int pos = 0;
    for(t=0;t<svp->n_segs;t++) {
	size += svp->segs[t].n_points + 1;
    }
    gfxline_t* lines = (gfxline_t*)rfx_alloc(sizeof(gfxline_t)*size);

    for(t=0;t<svp->n_segs;t++) {
	ArtSVPSeg* seg = &svp->segs[t];
	int p;
	for(p=0;p<seg->n_points;p++) {
	    lines[pos].type = p==0?gfx_moveTo:gfx_lineTo;
	    ArtPoint* point = &seg->points[p];
	    lines[pos].x = point->x;
	    lines[pos].y = point->y;
	    lines[pos].next = &lines[pos+1];
	    pos++;
	}
    }
    if(pos) {
	lines[pos-1].next = 0;
	return lines;
    } else {
	return 0;
    }
}

gfxpoly_t* gfxpoly_fillToPoly(gfxline_t*line)
{
    ArtSVP* svp = gfxfillToSVP(line, 1);
    if (svp->n_segs > 500)
    {
	int lineParts = 0;
	gfxline_t* lineCursor = line;
	while(lineCursor != NULL)
	{
	    if(lineCursor->type != gfx_moveTo) ++lineParts;
	    lineCursor = lineCursor->next;
	}
	fprintf(stderr, "arts_fill abandonning shape with %d segments (%d line parts)\n", svp->n_segs, lineParts);
	art_svp_free(svp);
	return (gfxpoly_t*)gfxpoly_strokeToPoly(0, 0, gfx_capButt, gfx_joinMiter, 0);
    }
    ArtSVP* svp2 = art_svp_rewind_uncrossed(art_svp_uncross(svp),ART_WIND_RULE_ODDEVEN);
    free(svp);svp=svp2;
    return (gfxpoly_t*)svp;
}

gfxpoly_t* gfxpoly_intersect(gfxpoly_t*poly1, gfxpoly_t*poly2)
{
    ArtSVP* svp1 = (ArtSVP*)poly1;
    ArtSVP* svp2 = (ArtSVP*)poly2;
	
    ArtSVP* svp = art_svp_intersect(svp1, svp2);
    return (gfxpoly_t*)svp;
}

gfxpoly_t* gfxpoly_union(gfxpoly_t*poly1, gfxpoly_t*poly2)
{
    ArtSVP* svp1 = (ArtSVP*)poly1;
    ArtSVP* svp2 = (ArtSVP*)poly2;
	
    ArtSVP* svp = art_svp_union(svp1, svp2);
    return (gfxpoly_t*)svp;
}

gfxpoly_t* gfxpoly_strokeToPoly(gfxline_t*line, gfxcoord_t width, gfx_capType cap_style, gfx_joinType joint_style, double miterLimit)
{
    ArtVpath* vec = gfxline_to_ArtVpath(line, 0);

    ArtSVP *svp = art_svp_vpath_stroke (vec,
			(joint_style==gfx_joinMiter)?ART_PATH_STROKE_JOIN_MITER:
			((joint_style==gfx_joinRound)?ART_PATH_STROKE_JOIN_ROUND:
			 ((joint_style==gfx_joinBevel)?ART_PATH_STROKE_JOIN_BEVEL:ART_PATH_STROKE_JOIN_BEVEL)),
			(cap_style==gfx_capButt)?ART_PATH_STROKE_CAP_BUTT:
			((cap_style==gfx_capRound)?ART_PATH_STROKE_CAP_ROUND:
			 ((cap_style==gfx_capSquare)?ART_PATH_STROKE_CAP_SQUARE:ART_PATH_STROKE_CAP_SQUARE)),
			width, //line_width
			miterLimit, //miter_limit
			0.05 //flatness
			);
    free(vec);
    return (gfxpoly_t*)svp;
}

gfxline_t* gfxline_circularToEvenOdd(gfxline_t*line)
{
    ArtSVP* svp = gfxfillToSVP(line, 1);
    ArtSVP* svp2 = art_svp_rewind_uncrossed(art_svp_uncross(svp),ART_WIND_RULE_POSITIVE);
    gfxline_t* result = gfxpoly_to_gfxline((gfxpoly_t*)svp2);
    art_svp_free(svp);
    art_svp_free(svp2);
    return result;
}

gfxpoly_t* gfxpoly_createbox(double x1, double y1,double x2, double y2)
{
    ArtVpath *vec = art_new (ArtVpath, 5+1);
    vec[0].code = ART_MOVETO;
    vec[0].x = x1;
    vec[0].y = y1;
    vec[1].code = ART_LINETO;
    vec[1].x = x1;
    vec[1].y = y2;
    vec[2].code = ART_LINETO;
    vec[2].x = x2;
    vec[2].y = y2;
    vec[3].code = ART_LINETO;
    vec[3].x = x2;
    vec[3].y = y1;
    vec[4].code = ART_LINETO;
    vec[4].x = x1;
    vec[4].y = y1;
    vec[5].code = ART_END;
    vec[5].x = 0;
    vec[5].y = 0;
    ArtSVP *svp = art_svp_from_vpath(vec);
    free(vec);
    return (gfxpoly_t*)svp;
}

void gfxpoly_free(gfxpoly_t*poly)
{
    ArtSVP*svp = (ArtSVP*)poly;
    free(svp);
}