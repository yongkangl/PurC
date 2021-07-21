/*
 * Copyright (C) 2018 Alexander Borisov
 *
 * Author: Alexander Borisov <borisov@lexbor.com>
 */

#ifndef LEXBOR_DOM_CDATA_SECTION_H
#define LEXBOR_DOM_CDATA_SECTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "html/dom/interfaces/document.h"
#include "html/dom/interfaces/text.h"


struct lxb_dom_cdata_section {
    lxb_dom_text_t text;
};


LXB_API lxb_dom_cdata_section_t *
lxb_dom_cdata_section_interface_create(lxb_dom_document_t *document);

LXB_API lxb_dom_cdata_section_t *
lxb_dom_cdata_section_interface_destroy(lxb_dom_cdata_section_t *cdata_section);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LEXBOR_DOM_CDATA_SECTION_H */
