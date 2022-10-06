/*
** @file udom.c
** @author Vincent Wei
** @date 2022/10/06
** @brief The implementation of uDOM (the rendering tree).
**
** Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>
**
** This file is a part of purc, which is an HVML interpreter with
** a command line interface (CLI).
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <assert.h>

#include "foil.h"
#include "udom.h"
#include "util/sorted-array.h"
#include "util/list.h"

struct pcth_rdr_segment {
    struct list_head ln;

    /* the rendering box where this segment locates in */
    struct purcth_rdrbox *rdrbox;

    unsigned i; // the index of first character
    unsigned n; // number of characters in this segment

    /* position of this segment in the rendering box */
    int x, y;

    /* rows taken by this segment (always be 1). */
    unsigned rows;
    /* columns taken by this segment. */
    unsigned cols;

    /* text color and attributes */
    int color;
};

struct purcth_rdrbox {
    struct purcth_rdrbox* parent;
    struct purcth_rdrbox* first;
    struct purcth_rdrbox* last;

    struct purcth_rdrbox* prev;
    struct purcth_rdrbox* next;

    unsigned nr_children;

    /* position in parent */
    int left, top;
    /* size of the box */
    unsigned width, height;

    /* the code points of text in Unicode (should be in visual order) */
    uint32_t *ucs;

    struct list_head segs_head;
};

struct purcth_udom {
    /* the sorted array of eDOM element and the corresponding rendering box. */
    struct sorted_array *elem2rdrbox;

    /* position of viewport */
    int left, top;

    /* size of whole page */
    unsigned width, height;

    /* size of viewport */
    unsigned cols, rows;
};

purcth_udom *foil_udom_new(purcth_page *page)
{
    purcth_udom* udom = calloc(1, sizeof(purcth_udom));

    udom->elem2rdrbox = sorted_array_create(SAFLAG_DEFAULT, 8, NULL, NULL);
    if (udom->elem2rdrbox == NULL) {
        goto failed;
    }

    /* TODO */
    (void)page;
    return udom;

failed:

    if (udom->elem2rdrbox)
        sorted_array_destroy(udom->elem2rdrbox);

    free(udom);
    return NULL;
}

void foil_udom_delete(purcth_udom *udom)
{
    sorted_array_destroy(udom->elem2rdrbox);
    free(udom);
}

purcth_rdrbox *foil_udom_find_rdrbox(purcth_udom *udom,
        uint64_t element_handle)
{
    void *data;

    if (!sorted_array_find(udom->elem2rdrbox, element_handle, &data)) {
        return NULL;
    }

    return data;
}

purcth_rdrbox *foil_udom_load_edom(purcth_udom *udom, purc_variant_t edom)
{
    (void)udom;
    (void)edom;

    /* TODO */
    return NULL;
}

int foil_udom_update_rdrbox(purcth_udom *udom, purcth_rdrbox *rdrbox,
        int op, const char *property, purc_variant_t ref_info)
{
    (void)udom;
    (void)rdrbox;
    (void)op;
    (void)property;
    (void)ref_info;

    /* TODO */
    return PCRDR_SC_NOT_IMPLEMENTED;
}

purc_variant_t foil_udom_call_method(purcth_udom *udom, purcth_rdrbox *rdrbox,
        const char *method, purc_variant_t arg)
{
    (void)udom;
    (void)rdrbox;
    (void)method;
    (void)arg;

    /* TODO */
    return PURC_VARIANT_INVALID;
}

purc_variant_t foil_udom_get_property(purcth_udom *udom, purcth_rdrbox *rdrbox,
        const char *property)
{
    (void)udom;
    (void)rdrbox;
    (void)property;

    /* TODO */
    return PURC_VARIANT_INVALID;
}

purc_variant_t foil_udom_set_property(purcth_udom *udom, purcth_rdrbox *rdrbox,
        const char *property, purc_variant_t value)
{
    (void)udom;
    (void)rdrbox;
    (void)property;
    (void)value;

    /* TODO */
    return PURC_VARIANT_INVALID;
}

