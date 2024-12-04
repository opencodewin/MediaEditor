#include <math.h>

#include "backend.h"
#include "potracelib.h"
#include "lists.h"
#include "greymap.h"
#include "render.h"
#include "auxiliary.h"
#include "trans.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void mem_path(potrace_curve_t *curve, trans_t t, render_t *rm)
{
    dpoint_t *c, c1[3];
    int i;
    int m = curve->n;

    c = curve->c[m - 1];
    c1[2] = trans(c[2], t);
    render_moveto(rm, c1[2].x, c1[2].y);

    for (i = 0; i < m; i++)
    {
        c = curve->c[i];
        switch (curve->tag[i])
        {
        case POTRACE_CORNER:
            c1[1] = trans(c[1], t);
            c1[2] = trans(c[2], t);
            render_lineto(rm, c1[1].x, c1[1].y);
            render_lineto(rm, c1[2].x, c1[2].y);
            break;
        case POTRACE_CURVETO:
            c1[0] = trans(c[0], t);
            c1[1] = trans(c[1], t);
            c1[2] = trans(c[2], t);
            render_curveto(rm, c1[0].x, c1[0].y, c1[1].x, c1[1].y, c1[2].x, c1[2].y);
            break;
        }
    }
}

static void mem_point(potrace_curve_t *curve, trans_t t, render_t *rm)
{
    dpoint_t *c, c1[3];
    int i;
    int m = curve->n;

    c = curve->c[m - 1];
    c1[2] = trans(c[2], t);
    render_dot(rm, c1[2].x, c1[2].y);

    for (i = 0; i < m; i++)
    {
        c = curve->c[i];
        switch (curve->tag[i])
        {
        case POTRACE_CORNER:
            c1[1] = trans(c[1], t);
            c1[2] = trans(c[2], t);
            render_dot(rm, c1[1].x, c1[1].y);
            render_dot(rm, c1[2].x, c1[2].y);
            break;
        case POTRACE_CURVETO:
            c1[0] = trans(c[0], t);
            c1[1] = trans(c[1], t);
            c1[2] = trans(c[2], t);
            render_dot(rm, c1[0].x, c1[0].y);
            render_dot(rm, c1[1].x, c1[1].y);
            render_dot(rm, c1[2].x, c1[2].y);
            break;
        }
    }
}

int page_mem(void *out, potrace_path_t *plist, imginfo_t *imginfo)
{
    potrace_path_t *p;
    greymap_t *gm;
    render_t *rm;
    int w, h;
    trans_t t;
    int mode;
    t.bb[0] = imginfo->trans.bb[0] + imginfo->lmar + imginfo->rmar;
    t.bb[1] = imginfo->trans.bb[1] + imginfo->tmar + imginfo->bmar;
    t.orig[0] = imginfo->trans.orig[0] + imginfo->lmar;
    t.orig[1] = imginfo->trans.orig[1] + imginfo->bmar;
    t.x[0] = imginfo->trans.x[0];
    t.x[1] = imginfo->trans.x[1];
    t.y[0] = imginfo->trans.y[0];
    t.y[1] = imginfo->trans.y[1];

    w = (int)ceil(t.bb[0]);
    h = (int)ceil(t.bb[1]);

    if (!out) return 1;

    unsigned char* data = (unsigned char*)out; 

    gm = gm_new(w, h);
    if (!gm)
    {
        return 1;
    }
    rm = render_new(gm);
    if (!rm)
    {
        return 1;
    }

    
    if (imginfo->invert)
    {
        gm_clear(gm, 0); /* black */
        for (int y = gm->h - 1; y >= 0; y--)
        {
            for (int x = 0; x < gm->w; x++)
            {
                if (x > imginfo->lmar && x < w - imginfo->rmar &&
                    y > imginfo->tmar && y < h - imginfo->bmar)
                    GM_UPUT(gm, x, y, 255);
            }
        }
    }
    else
    {
        gm_clear(gm, 255); /* white */
    }
    list_forall(p, plist)
    {
        if (info.draw_dot)
            mem_point(&p->curve, t, rm);
        else
            mem_path(&p->curve, t, rm);
    }

    render_close(rm);

    mode = imginfo->width * imginfo->height < 0 ? GM_MODE_NEGATIVE : GM_MODE_POSITIVE;
    int gammatable[256];
    if (info.gamma != 1.0)
    {
        gammatable[0] = 0;
        for (int v = 1; v < 256; v++)
        {
            gammatable[v] = (int)(255 * exp(log(v / 255.0) / info.gamma) + 0.5);
        }
    }
    else
    {
        for (int v = 0; v < 256; v++)
        {
            gammatable[v] = v;
        }
    }

    for (int y = gm->h - 1; y >= 0; y--)
    {
        for (int x = 0; x < gm->w; x++)
        {
            int v = GM_UGET(gm, x, y);
            switch (mode)
            {
                case GM_MODE_NONZERO: if (v > 255) v = 510 - v; if (v < 0) v = 0; break;
                case GM_MODE_ODD: v = mod(v, 510); if (v > 255) v = 510 - v; break;
                case GM_MODE_POSITIVE: if (v < 0) v = 0; else if (v > 255) v = 255; break;
                case GM_MODE_NEGATIVE: v = 510 - v; if (v < 0) v = 0; else if (v > 255) v = 255; break;
                default: break;
            }
            v = gammatable[v];
            switch (imginfo->channels)
            {
                case 1: 
                {
                    size_t index = (gm->h - y - 1) * gm->w + x;
                    data[index] = v;
                }
                break;
                case 3: 
                {
                    size_t index = 3 * ((gm->h - y - 1) * gm->w + x);
                    data[index + 0] = 
                    data[index + 1] = 
                    data[index + 2] = v;
                }
                break;
                case 4:
                {
                    size_t index = 4 * ((gm->h - y - 1) * gm->w + x);
                    data[index + 0] = 
                    data[index + 1] = 
                    data[index + 2] = v;
                    data[index + 3] = 255;
                }break;
                default: break;
            }
        }
    }

    render_free(rm);
    gm_free(gm);

    return 0;
}