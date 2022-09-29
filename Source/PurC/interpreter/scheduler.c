/*
 * @file scheduler.c
 * @author XueShuming
 * @date 2022/07/11
 * @brief The scheduler for coroutine.
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
 */

#include "purc.h"

#include "config.h"

#include "internal.h"

#include "private/var-mgr.h"
#include "private/errors.h"
#include "private/instance.h"
#include "private/utils.h"
#include "private/variant.h"
#include "private/ports.h"
#include "private/msg-queue.h"

#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#define SCHEDULE_SLEEP          10 * 1000       // usec
#define IDLE_EVENT_TIMEOUT      100             // ms
#define TIME_SLIECE             0.005           // s

#define BUILTIN_VAR_CRTN        PURC_PREDEF_VARNAME_CRTN

#define YIELD_EVENT_HANDLER     "_yield_event_handler"

static inline time_t
timespec_to_ms(const struct timespec *ts)
{
    return ts->tv_sec * 1000 + ts->tv_nsec * 1.0E-6;
}

static time_t
pcintr_monotonic_time_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return timespec_to_ms(&ts);
}

static void
broadcast_idle_event(struct pcinst *inst)
{
    struct pcintr_heap *heap = inst->intr_heap;
    struct list_head *crtns = &heap->crtns;
    pcintr_coroutine_t p, q;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        pcintr_stack_t stack = &co->stack;
        if (stack->observe_idle) {
            purc_variant_t hvml = pcintr_get_coroutine_variable(stack->co,
                    BUILTIN_VAR_CRTN);
            pcintr_coroutine_post_event(stack->co->cid,
                    PCRDR_MSG_EVENT_REDUCE_OPT_OVERLAY,
                    hvml, MSG_TYPE_IDLE, NULL,
                    PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
        }
    }

    crtns = &heap->stopped_crtns;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        pcintr_stack_t stack = &co->stack;
        if (stack->observe_idle) {
            purc_variant_t hvml = pcintr_get_coroutine_variable(stack->co,
                    BUILTIN_VAR_CRTN);
            pcintr_coroutine_post_event(stack->co->cid,
                    PCRDR_MSG_EVENT_REDUCE_OPT_OVERLAY,
                    hvml, MSG_TYPE_IDLE, NULL,
                    PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
        }
    }
}

static void
handle_rdr_conn_lost(struct pcinst *inst)
{
    struct pcintr_heap *heap = inst->intr_heap;
    struct list_head *crtns = &heap->crtns;
    pcintr_coroutine_t p, q;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        pcintr_stack_t stack = &co->stack;
        purc_variant_t hvml = pcintr_get_coroutine_variable(stack->co,
                BUILTIN_VAR_CRTN);

        stack->co->target_workspace_handle = 0;
        stack->co->target_page_handle = 0;
        stack->co->target_dom_handle = 0;

        // broadcast rdrState:connLost;
        pcintr_coroutine_post_event(stack->co->cid,
                PCRDR_MSG_EVENT_REDUCE_OPT_OVERLAY,
                hvml, MSG_TYPE_RDR_STATE, MSG_SUB_TYPE_CONN_LOST,
                PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
    }

    crtns = &heap->stopped_crtns;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        pcintr_stack_t stack = &co->stack;
        purc_variant_t hvml = pcintr_get_coroutine_variable(stack->co,
                BUILTIN_VAR_CRTN);

        stack->co->target_workspace_handle = 0;
        stack->co->target_page_handle = 0;
        stack->co->target_dom_handle = 0;

        // broadcast rdrState:connLost;
        pcintr_coroutine_post_event(stack->co->cid,
                PCRDR_MSG_EVENT_REDUCE_OPT_OVERLAY,
                hvml, MSG_TYPE_RDR_STATE, MSG_SUB_TYPE_CONN_LOST,
                PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
    }

    // FIXME:
    // pcrdr_disconnect(inst->conn_to_rdr);
    pcrdr_free_connection(inst->conn_to_rdr);
    inst->conn_to_rdr = NULL;
}


void
pcintr_check_after_execution_full(struct pcinst *inst, pcintr_coroutine_t co)
{
    bool one_run = false;
    pcintr_stack_t stack = &co->stack;
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);

    switch (co->state) {
        case CO_STATE_READY:
            break;
        case CO_STATE_RUNNING:
            break;
        case CO_STATE_STOPPED:
            PC_ASSERT(frame && frame->type == STACK_FRAME_TYPE_NORMAL);
            PC_ASSERT(inst->errcode == 0);
            return;
        default:
            break;
    }

    if (inst->errcode) {
        //PC_ASSERT(stack->except == 0);
        pcintr_exception_copy(&stack->exception);
        stack->except = 1;
        pcinst_clear_error(inst);
        PC_ASSERT(inst->errcode == 0);
#ifndef NDEBUG                     /* { */
        pcintr_dump_stack(stack);
#endif                             /* } */
        stack->terminated = 1;
        if (co->owner->cond_handler) {
            struct purc_cor_term_info term_info;
            term_info.except = stack->exception.error_except;
            term_info.doc = stack->doc;
            co->owner->cond_handler(PURC_COND_COR_TERMINATED, co, &term_info);
            /* Call purc_coroutine_dump_stack may set inst->errcode */
            purc_clr_error();
        }
        PC_ASSERT(inst->errcode == 0);
    }

    if (frame) {
        if (frame->next_step != NEXT_STEP_ON_POPPING) {
            pcintr_coroutine_set_state(co, CO_STATE_READY);
            return;
        }

        pcvdom_element_t elem = frame->pos;
        enum pchvml_tag_id tag_id = elem->tag_id;
        if (tag_id != PCHVML_TAG_HVML) {
            pcintr_coroutine_set_state(co, CO_STATE_READY);
            return;
        }
        // CO_STAGE_FIRST_RUN or
        // observing finished (only HVML tag in stack)
        one_run = true;
    }

    /* send doc to rdr */
    if (!stack->inherit && stack->co->stage == CO_STAGE_FIRST_RUN &&
            !pcintr_rdr_page_control_load(stack))
    {
        PC_ASSERT(0); // TODO:
        // stack->exited = 1;
        return;
    }


    if (one_run) {
        // repeat this call when an observing finished.
        if (co->owner->cond_handler) {
            struct purc_cor_run_info run_info = { 0, };
            run_info.run_idx = co->run_idx;
            run_info.doc = stack->doc;
            run_info.result = pcintr_coroutine_get_result(co);
            co->owner->cond_handler(PURC_COND_COR_ONE_RUN, co, &run_info);
        }
        co->run_idx++;
    }

    if (stack->co->stage != CO_STAGE_OBSERVING) {
        stack->co->stage = CO_STAGE_OBSERVING;
        // POST corState:observing
        if (co->curator) {
            purc_variant_t request_id =  purc_variant_make_ulongint(co->cid);
            pcintr_coroutine_post_event(co->curator, // target->cid,
                    PCRDR_MSG_EVENT_REDUCE_OPT_KEEP,
                    request_id,
                    MSG_TYPE_CORSTATE, MSG_SUB_TYPE_OBSERVING,
                    PURC_VARIANT_INVALID, request_id);
            int err = purc_get_last_error();
            if (err == PURC_ERROR_INVALID_VALUE) {
                purc_clr_error();
            }
            purc_variant_unref(request_id);
        }
    }
    pcintr_coroutine_set_state(co, CO_STATE_OBSERVING);

    if (co->stack.except) {
        const char *error_except = NULL;
        purc_atom_t atom;
        atom = co->stack.exception.error_except;
        PC_ASSERT(atom);
        error_except = purc_atom_to_string(atom);

        PC_ASSERT(co->error_except == NULL);
        co->error_except = error_except;

#ifndef NDEBUG
        pcintr_dump_c_stack(co->stack.exception.bt);
#endif
        co->stack.except = 0;

        if (!co->stack.exited) {
            co->stack.exited = 1;
            pcintr_notify_to_stop(co);
        }
    }

    if (!list_empty(&co->children)) {
        return;
    }

    if (co->stack.exited) {
        pcintr_revoke_all_hvml_observers(&co->stack);
        PC_ASSERT(list_empty(&co->stack.hvml_observers));

        pcintr_coroutine_set_state(co, CO_STATE_EXITED);
    }

    bool still_observed = pcintr_co_is_observed(co);
    if (!still_observed) {
        if (!co->stack.exited) {
            co->stack.exited = 1;
            pcintr_notify_to_stop(co);
        }
    }

    if (still_observed) {
        return;
    }

    if (!co->stack.exited) {
        co->stack.exited = 1;
        pcintr_notify_to_stop(co);
    }

 //#define PRINT_DEBUG
    if (co->stack.last_msg_sent == 0) {
        co->stack.last_msg_sent = 1;

#ifdef PRINT_DEBUG              /* { */
        PC_DEBUGX("last msg was sent");
#endif                          /* } */
        pcintr_coroutine_post_event(co->cid,
                PCRDR_MSG_EVENT_REDUCE_OPT_KEEP,
                PURC_VARIANT_INVALID,
                MSG_TYPE_LAST_MSG, NULL,
                PURC_VARIANT_INVALID, PURC_VARIANT_INVALID);
        return;
    }

    if (co->stack.last_msg_read == 0) {
        return;
    }


#ifdef PRINT_DEBUG              /* { */
    PC_DEBUGX("last msg was processed");
#endif                          /* } */

    if (co->curator) {
        purc_variant_t request_id =  purc_variant_make_ulongint(co->cid);
        purc_variant_t result = pcintr_coroutine_get_result(co);
        if (co->error_except) {
            // TODO: which is error, which is except?
            // currently, we treat all as except
            // XXX: curator may live in another thread!
            purc_variant_t payload = purc_variant_make_string(
                    co->error_except, false);
            pcintr_coroutine_post_event(co->curator,
                    PCRDR_MSG_EVENT_REDUCE_OPT_KEEP,
                    request_id,
                    MSG_TYPE_CALL_STATE, MSG_SUB_TYPE_EXCEPT,
                    payload, request_id);
            purc_variant_unref(payload);
        }
        else {
            // XXX: curator may live in another thread!
            pcintr_coroutine_post_event(co->curator, // target->cid,
                    PCRDR_MSG_EVENT_REDUCE_OPT_KEEP,
                    request_id,
                    MSG_TYPE_CALL_STATE, MSG_SUB_TYPE_SUCCESS,
                    result, request_id);
        }
        pcintr_coroutine_post_event(co->curator, // target->cid,
                PCRDR_MSG_EVENT_REDUCE_OPT_KEEP,
                request_id,
                MSG_TYPE_CORSTATE, MSG_SUB_TYPE_EXITED,
                result, request_id);
        purc_variant_unref(request_id);
    }
}

static void
execute_one_step_for_ready_co(struct pcinst *inst, pcintr_coroutine_t co)
{
    UNUSED_PARAM(inst);
    UNUSED_PARAM(co);

    pcintr_set_current_co(co);

    pcintr_coroutine_set_state(co, CO_STATE_RUNNING);
    pcintr_execute_one_step_for_ready_co(co);

    int err = purc_get_last_error();
    if (err != PURC_ERROR_AGAIN) {
        pcintr_check_after_execution_full(inst, co);
    }
    else {
        purc_clr_error();
    }

    pcintr_set_current_co(NULL);
}

// execute one step for all ready coroutines of the inst
// return whether busy
static bool
execute_one_step(struct pcinst *inst)
{
    bool busy = false;
    struct pcintr_heap *heap = inst->intr_heap;

    pcintr_coroutine_t p, q;
    pcintr_coroutine_t co;
    struct list_head *crtns;

    time_t now = pcintr_monotonic_time_ms();

    pcutils_array_t *cos = pcutils_array_create();
    if (!cos) {
        purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        return false;
    }

    size_t nr = pcutils_sorted_array_count(heap->wait_timeout_crtns);
    for (size_t i = 0; i < nr; i++) {
        pcutils_sorted_array_get(heap->wait_timeout_crtns, i, (void **)&co);
        if (now < co->stopped_timeout) {
            break;
        }
        co->stack.timeout = true;
        pcutils_array_push(cos, co);
    }

    nr = pcutils_array_length(cos);
    for (size_t i = 0; i < nr; i++) {
        co = pcutils_array_get(cos, i);
        pcintr_resume_coroutine(co);
    }
    pcutils_array_destroy(cos, true);


    crtns = &heap->crtns;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        if (co->state != CO_STATE_READY) {
            continue;
        }

#if 1
        struct timespec begin;
        clock_gettime(CLOCK_MONOTONIC, &begin);
        struct pcintr_stack_frame *frame;
        while (co->state == CO_STATE_READY) {
            frame = pcintr_stack_get_bottom_frame(&co->stack);
            bool must_yield = frame ? frame->must_yield : false;
            execute_one_step_for_ready_co(inst, co);
            if (must_yield) {
                break;
            }
            double diff = purc_get_elapsed_seconds(&begin, NULL);
            if (diff > TIME_SLIECE) {
                break;
            }
        }
#else
            execute_one_step_for_ready_co(inst, co);
#endif
        busy = true;
    }

    return busy;
}


void
check_and_dispatch_event_from_conn(struct pcinst *inst)
{
    struct pcrdr_conn *conn =  purc_get_conn_to_renderer();

    if (conn) {
        pcrdr_event_handler handle = pcrdr_conn_get_event_handler(conn);
        if (!handle) {
            pcrdr_conn_set_event_handler(conn, pcintr_conn_event_handler);
        }

        int last_err = purc_get_last_error();
        purc_clr_error();

        pcrdr_wait_and_dispatch_message(conn, 0);

        int err = purc_get_last_error();
        if (err == PCRDR_ERROR_IO || err == PCRDR_ERROR_PEER_CLOSED) {
            handle_rdr_conn_lost(inst);
        }
        purc_set_error(last_err);
    }
}

static int
handle_event_by_observer_list(purc_coroutine_t co, struct list_head *list,
        pcrdr_msg *msg, purc_atom_t event_type,
        const char *event_sub_type, bool *event_observed, bool *busy)
{
    int ret = PURC_ERROR_INCOMPLETED;
    purc_variant_t observed = msg->elementValue;
    struct pcintr_observer *observer, *next;
    list_for_each_entry_safe(observer, next, list, node) {
        bool match = observer->is_match(observer, msg, observed, event_type,
                event_sub_type);
        if ((co->stage & observer->cor_stage) &&
                (co->state & observer->cor_state) && match) {
            ret = observer->handle(co, observer, msg, event_type,
                    event_sub_type, observer->handle_data);
            if (observer->auto_remove) {
                pcintr_revoke_observer(observer);
            }
            *busy = true;
        }
        if (match) {
            *event_observed = true;
        }
    }
    return ret;
}

bool
handle_coroutine_event(pcintr_coroutine_t co)
{
    int handle_ret = PURC_ERROR_INCOMPLETED;
    bool busy = false;
    bool msg_observed = false;
    char *type = NULL;
    purc_atom_t event_type = 0;
    const char *event_sub_type = NULL;

    if (co->state == CO_STATE_READY || co->state == CO_STATE_RUNNING) {
        goto out;
    }

    pcrdr_msg *msg = pcinst_msg_queue_get_msg(co->mq);

    if (msg && msg->eventName) {
        const char *event = purc_variant_get_string_const(msg->eventName);
        const char *separator = strchr(event, MSG_EVENT_SEPARATOR);
        if (separator) {
            event_sub_type = separator + 1;
        }

        size_t nr_type = separator - event;
        if (nr_type) {
            type = strndup(event, separator - event);
            if (!type) {
                purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
                goto out;
            }

            event_type = purc_atom_try_string_ex(ATOM_BUCKET_MSG, type);
            if (!event_type) {
                purc_set_error(PURC_ERROR_INVALID_VALUE);
                PC_WARN("unknown event '%s'\n", event);
                goto out;
            }
        }
    }

    // observer
    if (msg) {
        handle_ret = handle_event_by_observer_list(co,
                &co->stack.intr_observers, msg, event_type, event_sub_type,
                &msg_observed, &busy);

        if (handle_ret == PURC_ERROR_OK) {
            pcrdr_release_message(msg);
            msg = NULL;
        }
        else {
            handle_ret = handle_event_by_observer_list(co,
                    &co->stack.hvml_observers, msg, event_type, event_sub_type,
                    &msg_observed, &busy);

            if (handle_ret == PURC_ERROR_OK) {
                pcrdr_release_message(msg);
                msg = NULL;
            }
        }
    }

    if (!list_empty(&co->tasks)) {
        struct pcintr_observer_task *task =
            list_first_entry(&co->tasks, struct pcintr_observer_task, ln);
        if (task && (co->stage & task->cor_stage) != 0  &&
                (co->state & task->cor_state) != 0) {
            list_del(&task->ln);
            pcintr_handle_task(task);
        }
    }

    if (!msg) {
        goto out;
    }

    if (msg_observed) {
        pcinst_msg_queue_append(co->mq, msg);
    }
    else {
        pcrdr_release_message(msg);
    }

out:
    if (type) {
        free(type);
    }
    return busy;
}

static bool
dispatch_event(struct pcinst *inst)
{
    UNUSED_PARAM(inst);

    struct timespec begin;
    bool is_busy = false;

#if 1
again:
    is_busy = false;
    clock_gettime(CLOCK_MONOTONIC, &begin);
#endif
    check_and_dispatch_event_from_conn(inst);

    bool co_is_busy = false;
    struct pcintr_heap *heap = inst->intr_heap;
    struct list_head *crtns = &heap->crtns;
    pcintr_coroutine_t p, q;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        co_is_busy = handle_coroutine_event(co);

        if (co->stack.exited && co->stack.last_msg_read) {
            pcintr_run_exiting_co(co);
        }

        if (co_is_busy) {
            is_busy = true;
        }
    }

    crtns = &heap->stopped_crtns;
    list_for_each_entry_safe(p, q, crtns, ln) {
        pcintr_coroutine_t co = p;
        co_is_busy = handle_coroutine_event(co);

        if (co->stack.exited && co->stack.last_msg_read) {
            pcintr_run_exiting_co(co);
        }

        if (co_is_busy) {
            is_busy = true;
        }
    }

#if 1
    double diff = purc_get_elapsed_seconds(&begin, NULL);
    if (diff < TIME_SLIECE && is_busy) {
        goto again;
    }
#endif
    return is_busy;
}

void
pcintr_schedule(void *ctxt)
{
    bool step_is_busy;
    bool event_is_busy;
    struct pcinst *inst = (struct pcinst *)ctxt;
    if (!inst) {
        goto out_sleep;
    }

    struct pcintr_heap *heap = inst->intr_heap;
    if (!heap) {
        goto out_sleep;
    }

again:

    // 1. exec one step for all ready coroutines and
    // return whether step is busy
    step_is_busy = execute_one_step(inst);

    // 2. dispatch event for observing / stopped coroutines
    event_is_busy = dispatch_event(inst);

    // 3. its busy, goto next scheduler without sleep
    if (step_is_busy || event_is_busy) {
        pcintr_update_timestamp(inst);
        goto again;
    }

    // 5. broadcast idle event
    double now = pcintr_get_current_time();
    if (now - IDLE_EVENT_TIMEOUT > heap->timestamp) {
        broadcast_idle_event(inst);
        pcintr_update_timestamp(inst);
    }

out_sleep:
    pcutils_usleep(SCHEDULE_SLEEP);

    return;
}

int pcintr_yield(
        int                       cor_stage,
        int                       cor_state,
        purc_variant_t            observed,
        const char               *event_type,
        const char               *event_sub_type,
        observer_match_fn         observer_is_match,
        observer_handle_fn        observer_handle,
        void                     *observer_handle_data,
        bool                      observer_auto_remove
        )
{
    pcintr_coroutine_t co = pcintr_get_coroutine();
    pcintr_stack_t stack = &co->stack;

    struct pcintr_observer *observer = pcintr_register_inner_observer(
            stack, cor_stage, cor_state,
            observed, event_type, event_sub_type,
            observer_is_match, observer_handle, observer_handle_data,
            observer_auto_remove
            );
    if (observer == NULL) {
        return -1;
    }

    pcintr_stop_coroutine(co, NULL);
    return 0;
}

void pcintr_resume(pcintr_coroutine_t co, pcrdr_msg *msg)
{
    UNUSED_PARAM(msg);
    PC_ASSERT(co);
    PC_ASSERT(co->state == CO_STATE_STOPPED);

    pcintr_stack_t stack = &co->stack;
    struct pcintr_stack_frame *frame;
    frame = pcintr_stack_get_bottom_frame(stack);
    PC_ASSERT(frame);

    pcintr_resume_coroutine(co);

    pcintr_coroutine_set_state(co, CO_STATE_RUNNING);
    pcintr_check_after_execution_full(pcinst_current(), co);
}

static int
serial_element(const char *buf, size_t len, void *ctxt)
{
    purc_rwstream_t rws = (purc_rwstream_t) ctxt;
    purc_rwstream_write(rws, buf, len);
    return 0;
}

static int
serial_symbol_vars(const char *symbol, int id,
        struct pcintr_stack_frame *frame, purc_rwstream_t stm)
{
    purc_rwstream_write(stm, symbol, strlen(symbol));
    size_t len_expected = 0;
    purc_variant_serialize(frame->symbol_vars[id],
            stm, 0,
            PCVARIANT_SERIALIZE_OPT_REAL_EJSON |
            PCVARIANT_SERIALIZE_OPT_BSEQUENCE_BASE64 |
            PCVARIANT_SERIALIZE_OPT_PLAIN,
            &len_expected);
    purc_rwstream_write(stm, "\n", 1);
    return 0;
}

#define DUMP_BUF_SIZE    128

static int
dump_stack_frame(pcintr_stack_t stack,
        struct pcintr_stack_frame *frame, purc_rwstream_t stm, int level)
{
    UNUSED_PARAM(stack);
    char buf[DUMP_BUF_SIZE];
    /* vcm_ctxt only dump once */
    bool dump_vcm_ctxt = (stack->vcm_ctxt && level == 0);
    pcvdom_element_t elem = frame->pos;
    if (!elem) {
        goto out;
    }

    snprintf(buf, DUMP_BUF_SIZE, "#%02d: ", level);
    purc_rwstream_write(stm, buf, strlen(buf));
    pcvdom_util_node_serialize_alone(&elem->node, serial_element, stm);

    if (frame->pos) {
        snprintf(buf, DUMP_BUF_SIZE, "  ATTRIBUTES:\n");
        purc_rwstream_write(stm, buf, strlen(buf));

        pcutils_array_t *attrs = frame->pos->attrs;
        struct pcvdom_attr *attr = NULL;
        size_t nr_params = pcutils_array_length(attrs);
        for (size_t i = 0; i < nr_params; i++) {
            attr = pcutils_array_get(attrs, i);
            if (dump_vcm_ctxt && ((size_t)stack->vcm_eval_pos == i)) {
                int err = pcvcm_eval_ctxt_error_code(stack->vcm_ctxt);
                purc_atom_t atom = purc_get_error_exception(err);
                snprintf(buf, DUMP_BUF_SIZE,
                        "    %s: `%s` raised when evaluating the experssion: ",
                        attr->key, purc_atom_to_string(atom));
                purc_rwstream_write(stm, buf, strlen(buf));
                pcvcm_dump_stack(stack->vcm_ctxt, stm, 2, true);
            }
            else {
                purc_variant_t val = pcutils_array_get(frame->attrs_result, i);
                if (val) {
                    char *val_buf = pcvariant_to_string(val);
                    snprintf(buf, DUMP_BUF_SIZE, "    %s: %s\n", attr->key, val_buf);
                    free(val_buf);
                }
                else {
                    snprintf(buf, DUMP_BUF_SIZE, "    %s:\n", attr->key);
                }
                purc_rwstream_write(stm, buf, strlen(buf));
            }

        }
    }

    struct pcvdom_node *child = pcvdom_node_first_child(&elem->node);
    if (child && child->type == PCVDOM_NODE_CONTENT) {
        if (dump_vcm_ctxt && stack->vcm_eval_pos == -1) {
            int err = pcvcm_eval_ctxt_error_code(stack->vcm_ctxt);
            purc_atom_t atom = purc_get_error_exception(err);
            snprintf(buf, DUMP_BUF_SIZE,
                    "  CONTENT: `%s` raised when evaluating the experssion: ",
                    purc_atom_to_string(atom));
            purc_rwstream_write(stm, buf, strlen(buf));
            pcvcm_dump_stack(stack->vcm_ctxt, stm, 1, true);
        }
        else {
            purc_variant_t val = pcintr_get_symbol_var(frame,
                    PURC_SYMBOL_VAR_CARET);
            if (val) {
                char *val_buf = pcvariant_to_string(val);
                snprintf(buf, DUMP_BUF_SIZE, "  CONTENT: %s\n", val_buf);
                free(val_buf);
            }
            else {
                snprintf(buf, DUMP_BUF_SIZE, "  CONTENT: undefined\n");
            }
            purc_rwstream_write(stm, buf, strlen(buf));
        }
    }
    else {
        snprintf(buf, DUMP_BUF_SIZE, "  CONTENT: undefined\n");
        purc_rwstream_write(stm, buf, strlen(buf));
    }



    snprintf(buf, DUMP_BUF_SIZE, "  CONTEXT VARIABLES:\n");
    purc_rwstream_write(stm, buf, strlen(buf));

    serial_symbol_vars("    < ", PURC_SYMBOL_VAR_LESS_THAN, frame, stm);
    serial_symbol_vars("    @ ", PURC_SYMBOL_VAR_AT_SIGN, frame, stm);
    serial_symbol_vars("    ! ", PURC_SYMBOL_VAR_EXCLAMATION, frame, stm);
    serial_symbol_vars("    : ", PURC_SYMBOL_VAR_COLON, frame, stm);
    serial_symbol_vars("    = ", PURC_SYMBOL_VAR_EQUAL, frame, stm);
    serial_symbol_vars("    % ", PURC_SYMBOL_VAR_PERCENT_SIGN, frame, stm);
    serial_symbol_vars("    ^ ", PURC_SYMBOL_VAR_CARET, frame, stm);

out:
    return 0;
}

int
purc_coroutine_dump_stack(purc_coroutine_t cor, purc_rwstream_t stm)
{
    int ret = 0;
    if (!cor || !stm) {
        purc_set_error(PURC_ERROR_INVALID_VALUE);
        goto out;
    }

    pcintr_stack_t stack = &cor->stack;
    struct pcintr_stack_frame *p = pcintr_stack_get_bottom_frame(stack);
    int level = 0;
    while (p && p->pos) {
        ret = dump_stack_frame(stack, p, stm, level);
        if (ret != 0) {
            goto out;
        }
        p = pcintr_stack_frame_get_parent(p);
        level++;
    }

out:
    return ret;
}

/* stop the specific coroutine */
void pcintr_stop_coroutine(pcintr_coroutine_t crtn,
        const struct timespec *timeout)
{
    pcintr_coroutine_set_state(crtn, CO_STATE_STOPPED);

    list_del(&crtn->ln);
    pcintr_heap_t heap = crtn->owner;
    list_add_tail(&crtn->ln, &heap->stopped_crtns);

    if (timeout) {
        time_t curr = pcintr_monotonic_time_ms();
        crtn->stopped_timeout = curr + timespec_to_ms(timeout);
    }
    else {
        crtn->stopped_timeout = -1;
    }
    if (crtn->stopped_timeout != -1) {
        if (pcutils_sorted_array_add(heap->wait_timeout_crtns,
                    (void *)(uintptr_t)crtn->stopped_timeout, crtn) < 0) {
            purc_set_error(PURC_ERROR_OUT_OF_MEMORY);
        }
    }

}

/* resume the specific coroutine */
void pcintr_resume_coroutine(pcintr_coroutine_t crtn)
{
    pcintr_coroutine_set_state(crtn, CO_STATE_READY);

    list_del(&crtn->ln);
    pcintr_heap_t heap = crtn->owner;
    list_add_tail(&crtn->ln, &heap->crtns);

    if (crtn->stopped_timeout != -1) {
        size_t nr = pcutils_sorted_array_count(heap->wait_timeout_crtns);
        for (size_t i = 0; i < nr; i++) {
            pcintr_coroutine_t co;
            pcutils_sorted_array_get(heap->wait_timeout_crtns, i, (void **)&co);
            if (co == crtn) {
                pcutils_sorted_array_delete(heap->wait_timeout_crtns, i);
                break;
            }
        }
    }

    crtn->stopped_timeout = -1;
}

