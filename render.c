/*
 * render.c
 * kirk johnson
 * october 1993
 *
 * Copyright (C) 1989, 1990, 1993-1995, 1999 Kirk Lauritz Johnson
 *
 * Parts of the source code (as marked) are:
 *   Copyright (C) 1989, 1990, 1991 by Jim Frost
 *   Copyright (C) 1992 by Jamie Zawinski <jwz@lucid.com>
 *
 * Permission to use, copy, modify and freely distribute xearth for
 * non-commercial and not-for-profit purposes is hereby granted
 * without fee, provided that both the above copyright notice and this
 * permission notice appear in all copies and in supporting
 * documentation.
 *
 * The author makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT
 * OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "xearth.h"

#define LABEL_LEFT_FLUSH (1<<0)
#define LABEL_TOP_FLUSH  (1<<1)

static void new_stars _P ((double));
static void new_grid _P ((int, int));
static void new_grid_dot _P ((double *, double *));
static void new_label _P ((void));
static int dot_comp _P ((const void *, const void *));
static void render_rows_setup _P ((void));
static void render_next_row _P ((s8or32 *, int));
static void no_shade_row _P ((s8or32 *, u_char *));
static void compute_sun_vector _P ((double *));
static void orth_compute_inv_x _P ((double *));
static void orth_shade_row _P ((int, s8or32 *, double *, double *, u_char *));
static void merc_shade_row _P ((int, s8or32 *, double *, u_char *));
static void cyl_shade_row _P ((int, s8or32 *, double *, u_char *));

static int scanbitcnt;
static ScanBit *scanbit;
static s8or32 scan_to_pix[256];

static int night_val;
static int day_val_base;
static double day_val_delta;

static ExtArr dots = NULL;
static int dotcnt;
static ScanDot *dot;

static int
dot_comp (a, b)
    const void *a;
    const void *b;
{
    return (((const ScanDot *) a)->y - ((const ScanDot *) b)->y);
}

static void
render_rows_setup ()
{
    int i;

    scanbitcnt = scanbits->count;
    scanbit = (ScanBit *) scanbits->body;
    dotcnt = dots->count;
    dot = (ScanDot *) dots->body;

    /* precompute table for translating between
     * scan buffer values and pixel types
     */
    for (i = 0; i < 256; i++)
        if (i == 0)
            scan_to_pix[i] = PixTypeSpace;
        else if (i > 64)
            scan_to_pix[i] = PixTypeLand;
        else
            scan_to_pix[i] = PixTypeWater;
}

static void
inverse_project (y, x, lat, lon)
    int y, x;
    double *lat, *lon;
{
    double ix = INV_XPROJECT (x);
    double iy = INV_YPROJECT (y);
    double q[3];
    double t;

    if (proj_type == ProjTypeOrthographic) {
        q[0] = ix;
        q[1] = iy;
        q[2] = sqrt (1 - (ix * ix + iy * iy));
    } else if (proj_type == ProjTypeMercator) {
        q[1] = INV_MERCATOR_Y (iy);
        t = sqrt (1 - q[1] * q[1]);
        q[0] = sin (ix) * t;
        q[2] = cos (ix) * t;
    } else {            /* (proj_type == ProjTypeCylindrical) */

        q[1] = INV_CYLINDRICAL_Y (iy);
        t = sqrt (1 - q[1] * q[1]);
        q[0] = sin (ix) * t;
        q[2] = cos (ix) * t;
    }
    /* inverse of XFORM_ROTATE */
    {
        double _p0_, _p1_, _p2_;
        double _c_, _s_, _t_;

        _p0_ = q[0];
        _p1_ = q[1];
        _p2_ = q[2];
        _c_ = view_pos_info.cos_rot;
        _s_ = -view_pos_info.sin_rot;
        _t_ = (_c_ * _p0_) - (_s_ * _p1_);
        _p1_ = (_s_ * _p0_) + (_c_ * _p1_);
        _p0_ = _t_;
        _c_ = view_pos_info.cos_lat;
        _s_ = -view_pos_info.sin_lat;
        _t_ = (_c_ * _p1_) - (_s_ * _p2_);
        _p2_ = (_s_ * _p1_) + (_c_ * _p2_);
        _p1_ = _t_;
        _c_ = view_pos_info.cos_lon;
        _s_ = -view_pos_info.sin_lon;
        _t_ = (_c_ * _p0_) - (_s_ * _p2_);
        _p2_ = (_s_ * _p0_) + (_c_ * _p2_);
        _p0_ = _t_;
        q[0] = _p0_;
        q[1] = _p1_;
        q[2] = _p2_;
    }
    *lat = asin (q[1]);
    *lon = atan2 (q[0], q[2]);
}

static void
render_next_row (buf, idx)
    s8or32 *buf;
    int idx;
{
    int i, i_lim;
    int tmp;
    int _scanbitcnt;
    ScanBit *_scanbit;
    double lat, lon;
    int p;

    xearth_bzero ((char *) buf, (unsigned) (sizeof (s8or32) * wdth));

    if (overlayfile == NULL) {
        /* explicitly copy scanbitcnt and scanbit to local variables
         * to help compilers figure out that they can be registered
         */
        _scanbitcnt = scanbitcnt;
        _scanbit = scanbit;

        while ((_scanbitcnt > 0) && (_scanbit->y == idx)) {
            /* use i_lim to encourage compilers to register loop limit
             */
            i_lim = _scanbit->hi_x;
            tmp = _scanbit->val;
            for (i = _scanbit->lo_x; i <= i_lim; i++)
                buf[i] += tmp;

            _scanbit += 1;
            _scanbitcnt -= 1;
        }

        /* copy changes to scanbitcnt and scanbit out to memory
         */
        scanbitcnt = _scanbitcnt;
        scanbit = _scanbit;
    } else {
        for (i = 0; i <= wdth; i++) {
            inverse_project (idx, i, &lat, &lon);
            p = overlay_pixel (lat, lon);
            if (p != -1) {
                buf[i] = 0x40000000 | p;
            }
        }
    }

    /* use i_lim to encourage compilers to register loop limit
     */
    i_lim = wdth;
    for (i = 0; i < i_lim; i++) {
        if ((buf[i] & 0x40000000) == 0)
            buf[i] = scan_to_pix[(int) (buf[i] & 0xff)];

        if (cloudfile != NULL) {
            inverse_project (idx, i, &lat, &lon);
            buf[i] = cloud_pixel (lat, lon, buf[i]);
        }
    }

    while ((dotcnt > 0) && (dot->y == idx)) {
        tmp = dot->x;

        if (dot->type == DotTypeStar) {
            if (buf[tmp] == PixTypeSpace)
                buf[tmp] = PixTypeStar;
        } else {
            buf[tmp] = PixTypeGridLand;
        }

        dot += 1;
        dotcnt -= 1;
    }
}

static void
no_shade_row (scanbuf, rslt)
    s8or32 *scanbuf;
    u_char *rslt;
{
    int i, i_lim;

    /* use i_lim to encourage compilers to register loop limit
     */
    i_lim = wdth;
    for (i = 0; i < i_lim; i++) {
        rslt[0] = PixRed (scanbuf[i]);
        rslt[1] = PixGreen (scanbuf[i]);
        rslt[2] = PixBlue (scanbuf[i]);

        rslt += 3;
    }
}

static void
compute_sun_vector (rslt)
    double *rslt;
{
    rslt[0] = sin (sun_lon * (M_PI / 180)) * cos (sun_lat * (M_PI / 180));
    rslt[1] = sin (sun_lat * (M_PI / 180));
    rslt[2] = cos (sun_lon * (M_PI / 180)) * cos (sun_lat * (M_PI / 180));

    XFORM_ROTATE (rslt, view_pos_info);
}

static void
orth_compute_inv_x (inv_x)
    double *inv_x;
{
    int i, i_lim;

    i_lim = wdth;
    for (i = 0; i < i_lim; i++)
        inv_x[i] = INV_XPROJECT (i);
}

static void
orth_shade_row (idx, scanbuf, sol, inv_x, rslt)
    int idx;
    s8or32 *scanbuf;
    double *sol;
    double *inv_x;
    u_char *rslt;
{
    int i, i_lim;
    int scanbuf_val;
    int val;
    double x, y, z;
    double scale;
    double tmp;
    double y_sol_1;

    y = INV_YPROJECT (idx);

    /* save a little computation in the inner loop
     */
    tmp = 1 - (y * y);
    y_sol_1 = y * sol[1];

    /* use i_lim to encourage compilers to register loop limit
     */
    i_lim = wdth;
    for (i = 0; i < i_lim; i++) {
        scanbuf_val = scanbuf[i];

        switch (scanbuf_val) {
        case PixTypeSpace:
        case PixTypeStar:
        case PixTypeGridLand:
        case PixTypeGridWater:
            rslt[0] = PixRed (scanbuf_val);
            rslt[1] = PixGreen (scanbuf_val);
            rslt[2] = PixBlue (scanbuf_val);
            break;

        default:
            x = inv_x[i];
            z = tmp - (x * x);
            z = SQRT (z);
            scale = (x * sol[0]) + y_sol_1 + (z * sol[2]);
            if (scale < 0) {
                val = night_val;
            } else {
                val = day_val_base + (scale * day_val_delta);
                if (val > 255)
                    val = 255;
                else
                    assert (val >= 0);
            }

            rslt[0] = PixRed (scanbuf_val) * val / 255;
            rslt[1] = PixGreen (scanbuf_val) * val / 255;
            rslt[2] = PixBlue (scanbuf_val) * val / 255;
            break;
        }

        rslt += 3;
    }
}

static void
merc_shade_row (idx, scanbuf, sol, rslt)
    int idx;
    s8or32 *scanbuf;
    double *sol;
    u_char *rslt;
{
    int i, i_lim;
    int scanbuf_val;
    int val;
    double x, y, z;
    double sin_theta;
    double cos_theta;
    double scale;
    double tmp;
    double y_sol_1;

    y = INV_YPROJECT (idx);
    y = INV_MERCATOR_Y (y);

    /* conceptually, on each iteration of the i loop, we want:
     *
     *   x = sin(INV_XPROJECT(i)) * sqrt(1 - (y*y));
     *   z = cos(INV_XPROJECT(i)) * sqrt(1 - (y*y));
     *
     * computing this directly is rather expensive, however, so we only
     * compute the first (i=0) pair of values directly; all other pairs
     * (i>0) are obtained through successive rotations of the original
     * pair (by inv_proj_scale radians).
     */

    /* compute initial (x, z) values
     */
    tmp = sqrt (1 - (y * y));
    x = sin (INV_XPROJECT (0)) * tmp;
    z = cos (INV_XPROJECT (0)) * tmp;

    /* compute rotation coefficients used
     * to find subsequent (x, z) values
     */
    tmp = proj_info.inv_proj_scale;
    sin_theta = sin (tmp);
    cos_theta = cos (tmp);

    /* save a little computation in the inner loop
     */
    y_sol_1 = y * sol[1];

    /* use i_lim to encourage compilers to register loop limit
     */
    i_lim = wdth;
    for (i = 0; i < i_lim; i++) {
        scanbuf_val = scanbuf[i];

        switch (scanbuf_val) {
        case PixTypeSpace:
        case PixTypeStar:
        case PixTypeGridLand:
        case PixTypeGridWater:
            rslt[0] = PixRed (scanbuf_val);
            rslt[1] = PixGreen (scanbuf_val);
            rslt[2] = PixBlue (scanbuf_val);
            break;

        default:
            scale = (x * sol[0]) + y_sol_1 + (z * sol[2]);
            if (scale < 0) {
                val = night_val;
            } else {
                val = day_val_base + (scale * day_val_delta);
                if (val > 255)
                    val = 255;
                else
                    assert (val >= 0);
            }

            rslt[0] = PixRed (scanbuf_val) * val / 255;
            rslt[1] = PixGreen (scanbuf_val) * val / 255;
            rslt[2] = PixBlue (scanbuf_val) * val / 255;
            break;
        }

        /* compute next (x, z) values via 2-d rotation
         */
        tmp = (cos_theta * z) - (sin_theta * x);
        x = (sin_theta * z) + (cos_theta * x);
        z = tmp;

        rslt += 3;
    }
}

static void
cyl_shade_row (idx, scanbuf, sol, rslt)
    int idx;
    s8or32 *scanbuf;
    double *sol;
    u_char *rslt;
{
    int i, i_lim;
    int scanbuf_val;
    int val;
    double x, y, z;
    double sin_theta;
    double cos_theta;
    double scale;
    double tmp;
    double y_sol_1;

    y = INV_YPROJECT (idx);
    y = INV_CYLINDRICAL_Y (y);

    /* conceptually, on each iteration of the i loop, we want:
     *
     *   x = sin(INV_XPROJECT(i)) * sqrt(1 - (y*y));
     *   z = cos(INV_XPROJECT(i)) * sqrt(1 - (y*y));
     *
     * computing this directly is rather expensive, however, so we only
     * compute the first (i=0) pair of values directly; all other pairs
     * (i>0) are obtained through successive rotations of the original
     * pair (by inv_proj_scale radians).
     */

    /* compute initial (x, z) values
     */
    tmp = sqrt (1 - (y * y));
    x = sin (INV_XPROJECT (0)) * tmp;
    z = cos (INV_XPROJECT (0)) * tmp;

    /* compute rotation coefficients used
     * to find subsequent (x, z) values
     */
    tmp = proj_info.inv_proj_scale;
    sin_theta = sin (tmp);
    cos_theta = cos (tmp);

    /* save a little computation in the inner loop
     */
    y_sol_1 = y * sol[1];

    /* use i_lim to encourage compilers to register loop limit
     */
    i_lim = wdth;
    for (i = 0; i < i_lim; i++) {
        scanbuf_val = scanbuf[i];

        switch (scanbuf_val) {
        case PixTypeSpace:
        case PixTypeStar:
        case PixTypeGridLand:
        case PixTypeGridWater:
            rslt[0] = PixRed (scanbuf_val);
            rslt[1] = PixGreen (scanbuf_val);
            rslt[2] = PixBlue (scanbuf_val);
            break;

        default:
            scale = (x * sol[0]) + y_sol_1 + (z * sol[2]);
            if (scale < 0) {
                val = night_val;
            } else {
                val = day_val_base + (scale * day_val_delta);
                if (val > 255)
                    val = 255;
                else
                    assert (val >= 0);
            }

            rslt[0] = PixRed (scanbuf_val) * val / 255;
            rslt[1] = PixGreen (scanbuf_val) * val / 255;
            rslt[2] = PixBlue (scanbuf_val) * val / 255;
            break;
        }

        /* compute next (x, z) values via 2-d rotation
         */
        tmp = (cos_theta * z) - (sin_theta * x);
        x = (sin_theta * z) + (cos_theta * x);
        z = tmp;

        rslt += 3;
    }
}

void
render (rowfunc)
    int (*rowfunc) _P ((u_char *));
{
    int i, i_lim;
    s8or32 *scanbuf;
    u_char *row;
    double *inv_x;
    double sol[3] = { 0, 0, 0 };        /* initialize to suppress spurious unused warning */
    double tmp;

    scanbuf = (s8or32 *) malloc ((unsigned) (sizeof (s8or32) * wdth));
    row = (u_char *) malloc ((unsigned) wdth * 3);
    assert ((scanbuf != NULL) && (row != NULL));
    overlay_init ();

    inv_x = NULL;
    render_rows_setup ();

    if (do_shade) {
        /* inv_x[] only gets used with orthographic projection
         */
        if (proj_type == ProjTypeOrthographic) {
            inv_x = (double *) malloc ((unsigned) sizeof (double) * wdth);
            assert (inv_x != NULL);
            orth_compute_inv_x (inv_x);
        }

        compute_sun_vector (sol);

        /* precompute shading parameters
         */
        night_val = night * (255.99 / 100.0);
        tmp = terminator / 100.0;
        day_val_base = ((tmp * day) + ((1 - tmp) * night)) * (255.99 / 100.0);
        day_val_delta = (day * (255.99 / 100.0)) - day_val_base;
    }

    /* main render loop
     * (use i_lim to encourage compilers to register loop limit)
     */
    i_lim = hght;
    for (i = 0; i < i_lim; i++) {
        render_next_row (scanbuf, i);

        if (!do_shade)
            no_shade_row (scanbuf, row);
        else if (proj_type == ProjTypeOrthographic)
            orth_shade_row (i, scanbuf, sol, inv_x, row);
        else if (proj_type == ProjTypeMercator)
            merc_shade_row (i, scanbuf, sol, row);
        else            /* (proj_type == ProjTypeCylindrical) */
            cyl_shade_row (i, scanbuf, sol, row);

        rowfunc (row);
    }

    overlay_close ();
    free (scanbuf);
    free (row);

    if (inv_x != NULL)
        free (inv_x);
}

void
do_dots ()
{
    if (dots == NULL)
        dots = extarr_alloc (sizeof (ScanDot));
    else
        dots->count = 0;

    if (do_stars)
        new_stars (star_freq);
    if (do_grid)
        new_grid (grid_big, grid_small);
    if (do_label)
        new_label ();

    qsort (dots->body, dots->count, sizeof (ScanDot), dot_comp);
}

static void
new_stars (freq)
    double freq;
{
    int i;
    int x, y;
    int max_stars;
    ScanDot *newdot;

    max_stars = wdth * hght * freq;

    for (i = 0; i < max_stars; i++) {
        x = random () % wdth;
        y = random () % hght;

        newdot = (ScanDot *) extarr_next (dots);
        newdot->x = x;
        newdot->y = y;
        newdot->type = DotTypeStar;

        if ((big_stars) && (x + 1 < wdth) && ((random () % 100) < big_stars)) {
            newdot = (ScanDot *) extarr_next (dots);
            newdot->x = x + 1;
            newdot->y = y;
            newdot->type = DotTypeStar;
        }
    }
}

static void
new_grid (big, small)
    int big;
    int small;
{
    int i, j;
    int cnt;
    double lat, lon;
    double lat_scale, lon_scale;
    double cs_lat[2];
    double cs_lon[2];

    /* lines of longitude
     */
    lon_scale = M_PI / (2 * big);
    lat_scale = M_PI / (2 * big * small);
    for (i = (-2 * big); i < (2 * big); i++) {
        lon = i * lon_scale;
        cs_lon[0] = cos (lon);
        cs_lon[1] = sin (lon);

        for (j = (-(big * small) + 1); j < (big * small); j++) {
            lat = j * lat_scale;
            cs_lat[0] = cos (lat);
            cs_lat[1] = sin (lat);

            new_grid_dot (cs_lat, cs_lon);
        }
    }

    /* lines of latitude
     */
    lat_scale = M_PI / (2 * big);
    for (i = (1 - big); i < big; i++) {
        lat = i * lat_scale;
        cs_lat[0] = cos (lat);
        cs_lat[1] = sin (lat);
        cnt = 2 * ((int) ((cs_lat[0] * small) + 0.5)) * big;
        lon_scale = M_PI / cnt;

        for (j = (-cnt); j < cnt; j++) {
            lon = j * lon_scale;
            cs_lon[0] = cos (lon);
            cs_lon[1] = sin (lon);

            new_grid_dot (cs_lat, cs_lon);
        }
    }
}

static void
new_grid_dot (cs_lat, cs_lon)
    double *cs_lat;
    double *cs_lon;
{
    int x, y;
    double pos[3];
    ScanDot *new;

    pos[0] = cs_lon[1] * cs_lat[0];
    pos[1] = cs_lat[1];
    pos[2] = cs_lon[0] * cs_lat[0];

    XFORM_ROTATE (pos, view_pos_info);

    if (proj_type == ProjTypeOrthographic) {
        /* if the grid dot isn't visible, return immediately
         */
        if (pos[2] <= 0)
            return;
    } else if (proj_type == ProjTypeMercator) {
        /* apply mercator projection
         */
        pos[0] = MERCATOR_X (pos[0], pos[2]);
        pos[1] = MERCATOR_Y (pos[1]);
    } else {            /* (proj_type == ProjTypeCylindrical) */

        /* apply cylindrical projection
         */
        pos[0] = CYLINDRICAL_X (pos[0], pos[2]);
        pos[1] = CYLINDRICAL_Y (pos[1]);
    }

    x = XPROJECT (pos[0]);
    y = YPROJECT (pos[1]);

    if ((x >= 0) && (x < wdth) && (y >= 0) && (y < hght)) {
        new = (ScanDot *) extarr_next (dots);
        new->x = x;
        new->y = y;
        new->type = DotTypeGrid;
    }
}

static void
new_label ()
{
    int dy;
    int x, y;
    int label_orient = LABEL_LEFT_FLUSH;
    int label_xvalue = 5;
    int label_yvalue = 5;
    int height;
    int width;
    char buf[128];

    font_extent ("", &dy, &width);

    if (label_orient & LABEL_TOP_FLUSH) {
        y = label_yvalue;
    } else {
        y = (hght + label_yvalue) - dy;
        y -= 2 * dy;    /* 3 lines of text */
    }

    strftime (buf, sizeof (buf), "%d %b %Y %H:%M %Z",
              localtime (&current_time));
    font_extent (buf, &height, &width);
    if (label_orient & LABEL_LEFT_FLUSH)
        x = label_xvalue;
    else
        x = (wdth + label_xvalue) - width;
    font_draw (x, y, buf, dots);
    y += dy;

    sprintf (buf, "view %.1f %c %.1f %c",
             fabs (view_lat), ((view_lat < 0) ? 'S' : 'N'),
             fabs (view_lon), ((view_lon < 0) ? 'W' : 'E'));
    font_extent (buf, &height, &width);
    if (label_orient & LABEL_LEFT_FLUSH)
        x = label_xvalue;
    else
        x = (wdth + label_xvalue) - width;
    font_draw (x, y, buf, dots);
    y += dy;

    sprintf (buf, "sun %.1f %c %.1f %c",
             fabs (sun_lat), ((sun_lat < 0) ? 'S' : 'N'),
             fabs (sun_lon), ((sun_lon < 0) ? 'W' : 'E'));
    font_extent (buf, &height, &width);
    if (label_orient & LABEL_LEFT_FLUSH)
        x = label_xvalue;
    else
        x = (wdth + label_xvalue) - width;
    font_draw (x, y, buf, dots);
    y += dy;
}
