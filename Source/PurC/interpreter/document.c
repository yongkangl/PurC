/**
 * @file document.c
 * @author Xu Xiaohong
 * @date 2021/12/06
 * @brief
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of PurC (short for Purring Cat), an HVML interpreter.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "purc.h"

#include "internal.h"

#include "private/debug.h"
#include "private/runloop.h"

#include "ops.h"

#include <pthread.h>
#include <unistd.h>
#include <libgen.h>

struct ctxt_for_document {
    struct pcvdom_node           *curr;
};

static void
ctxt_for_document_destroy(struct ctxt_for_document *ctxt)
{
    if (ctxt) {
        free(ctxt);
    }
}

static void
ctxt_destroy(void *ctxt)
{
    ctxt_for_document_destroy((struct ctxt_for_document*)ctxt);
}

static void*
after_pushed(pcintr_stack_t stack, pcvdom_element_t pos)
{
    D("");

    PC_ASSERT(stack);
    PC_ASSERT(stack == purc_get_stack());

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);

    frame->pos = pos; // ATTENTION!!

    struct ctxt_for_document *ctxt;
    ctxt = (struct ctxt_for_document*)calloc(1, sizeof(*ctxt));
    if (!ctxt) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    frame->ctxt = ctxt;
    frame->ctxt_destroy = ctxt_destroy;
    purc_clr_error();

    return ctxt;
}

static bool
on_popping(pcintr_stack_t stack, void* ctxt)
{
    PC_ASSERT(stack);
    PC_ASSERT(stack == purc_get_stack());

    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(ctxt == frame->ctxt);

    struct ctxt_for_document *document_ctxt;
    document_ctxt = (struct ctxt_for_document*)frame->ctxt;
    if (document_ctxt) {
        ctxt_for_document_destroy(ctxt);
        frame->ctxt = NULL;
    }

    D("");
    return true;
}

static void
on_element(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        struct pcvdom_element *element)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(element);
}

static void
on_content(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        struct pcvdom_content *content)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    PC_ASSERT(content);
    char *text = content->text;
    D("content: [%s]", text);
}

static void
on_comment(pcintr_coroutine_t co, struct pcintr_stack_frame *frame,
        struct pcvdom_comment *comment)
{
    UNUSED_PARAM(co);
    UNUSED_PARAM(frame);
    PC_ASSERT(comment);
    char *text = comment->text;
    D("comment: [%s]", text);
}

static pcvdom_element_t
select_child(pcintr_stack_t stack, void* ctxt)
{
    D("");

    PC_ASSERT(stack);
    PC_ASSERT(stack == purc_get_stack());

    pcintr_coroutine_t co = &stack->co;
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(ctxt == frame->ctxt);

    struct ctxt_for_document *document_ctxt;
    document_ctxt = (struct ctxt_for_document*)frame->ctxt;

    struct pcvdom_node *curr;

again:
    curr = document_ctxt->curr;

    if (curr == NULL) {
        struct pcvdom_document *document = stack->vdom->document;
        curr = pcvdom_node_first_child(&document->node);
    }
    else {
        curr = pcvdom_node_next_sibling(curr);
    }

    document_ctxt->curr = curr;

    if (curr == NULL) {
        purc_clr_error();
        return NULL;
    }

    switch (curr->type) {
        case PCVDOM_NODE_DOCUMENT:
            PC_ASSERT(0); // Not implemented yet
            break;
        case PCVDOM_NODE_ELEMENT:
            {
                pcvdom_element_t element = PCVDOM_ELEMENT_FROM_NODE(curr);
                on_element(co, frame, element);
                if (element->tag_id == PCHVML_TAG_HVML)
                    return element;
                goto again;
            }
        case PCVDOM_NODE_CONTENT:
            on_content(co, frame, PCVDOM_CONTENT_FROM_NODE(curr));
            goto again;
        case PCVDOM_NODE_COMMENT:
            on_comment(co, frame, PCVDOM_COMMENT_FROM_NODE(curr));
            goto again;
        default:
            PC_ASSERT(0); // Not implemented yet
    }

    PC_ASSERT(0);
    return NULL; // NOTE: never reached here!!!
}

static struct pcintr_element_ops
ops = {
    .after_pushed       = after_pushed,
    .on_popping         = on_popping,
    .rerun              = NULL,
    .select_child       = select_child,
};

struct pcintr_element_ops* pcintr_get_document_ops(void)
{
    return &ops;
}

