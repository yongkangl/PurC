/**
 * @file hvml.h
 * @author Xu Xiaohong
 * @date 2021/08/23
 * @brief The internal interfaces for hvml parser.
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

#ifndef PURC_PRIVATE_HVML_H
#define PURC_PRIVATE_HVML_H

#include "purc-rwstream.h"
#include "private/stack.h"
#include "private/vcm.h"

enum pchvml_state {
    PCHVML_FIRST_STATE = 0,

    PCHVML_DATA_STATE = PCHVML_FIRST_STATE,
    PCHVML_RCDATA_STATE,
    PCHVML_RAWTEXT_STATE,
    PCHVML_PLAINTEXT_STATE,
    PCHVML_TAG_OPEN_STATE,
    PCHVML_END_TAG_OPEN_STATE,
    PCHVML_TAG_NAME_STATE,
    PCHVML_RCDATA_LESS_THAN_SIGN_STATE,
    PCHVML_RCDATA_END_TAG_OPEN_STATE,
    PCHVML_RCDATA_END_TAG_NAME_STATE,
    PCHVML_RAWTEXT_LESS_THAN_SIGN_STATE,
    PCHVML_RAWTEXT_END_TAG_OPEN_STATE,
    PCHVML_RAWTEXT_END_TAG_NAME_STATE,
    PCHVML_BEFORE_ATTRIBUTE_NAME_STATE,
    PCHVML_ATTRIBUTE_NAME_STATE,
    PCHVML_AFTER_ATTRIBUTE_NAME_STATE,
    PCHVML_BEFORE_ATTRIBUTE_VALUE_STATE,
    PCHVML_ATTRIBUTE_VALUE_DOUBLE_QUOTED_STATE,
    PCHVML_ATTRIBUTE_VALUE_SINGLE_QUOTED_STATE,
    PCHVML_ATTRIBUTE_VALUE_UNQUOTED_STATE,
    PCHVML_AFTER_ATTRIBUTE_VALUE_QUOTED_STATE,
    PCHVML_SELF_CLOSING_START_TAG_STATE,
    PCHVML_BOGUS_COMMENT_STATE,
    PCHVML_MARKUP_DECLARATION_OPEN_STATE,
    PCHVML_COMMENT_START_STATE,
    PCHVML_COMMENT_START_DASH_STATE,
    PCHVML_COMMENT_STATE,
    PCHVML_COMMENT_LESS_THAN_SIGN_STATE,
    PCHVML_COMMENT_LESS_THAN_SIGN_BANG_STATE,
    PCHVML_COMMENT_LESS_THAN_SIGN_BANG_DASH_STATE,
    PCHVML_COMMENT_LESS_THAN_SIGN_BANG_DASH_DASH_STATE,
    PCHVML_COMMENT_END_DASH_STATE,
    PCHVML_COMMENT_END_STATE,
    PCHVML_COMMENT_END_BANG_STATE,
    PCHVML_DOCTYPE_STATE,
    PCHVML_BEFORE_DOCTYPE_NAME_STATE,
    PCHVML_DOCTYPE_NAME_STATE,
    PCHVML_AFTER_DOCTYPE_NAME_STATE,
    PCHVML_AFTER_DOCTYPE_PUBLIC_KEYWORD_STATE,
    PCHVML_BEFORE_DOCTYPE_PUBLIC_ID_STATE,
    PCHVML_DOCTYPE_PUBLIC_ID_DOUBLE_QUOTED_STATE,
    PCHVML_DOCTYPE_PUBLIC_ID_SINGLE_QUOTED_STATE,
    PCHVML_AFTER_DOCTYPE_PUBLIC_ID_STATE,
    PCHVML_BETWEEN_DOCTYPE_PUBLIC_ID_AND_SYSTEM_INFO_STATE,
    PCHVML_AFTER_DOCTYPE_SYSTEM_KEYWORD_STATE,
    PCHVML_BEFORE_DOCTYPE_SYSTEM_STATE,
    PCHVML_DOCTYPE_SYSTEM_DOUBLE_QUOTED_STATE,
    PCHVML_DOCTYPE_SYSTEM_SINGLE_QUOTED_STATE,
    PCHVML_AFTER_DOCTYPE_SYSTEM_STATE,
    PCHVML_BOGUS_DOCTYPE_STATE,
    PCHVML_CDATA_SECTION_STATE,
    PCHVML_CDATA_SECTION_BRACKET_STATE,
    PCHVML_CDATA_SECTION_END_STATE,
    PCHVML_CHARACTER_REFERENCE_STATE,
    PCHVML_NAMED_CHARACTER_REFERENCE_STATE,
    PCHVML_AMBIGUOUS_AMPERSAND_STATE,
    PCHVML_NUMERIC_CHARACTER_REFERENCE_STATE,
    PCHVML_HEXADECIMAL_CHARACTER_REFERENCE_START_STATE,
    PCHVML_DECIMAL_CHARACTER_REFERENCE_START_STATE,
    PCHVML_HEXADECIMAL_CHARACTER_REFERENCE_STATE,
    PCHVML_DECIMAL_CHARACTER_REFERENCE_STATE,
    PCHVML_NUMERIC_CHARACTER_REFERENCE_END_STATE,
    PCHVML_SPECIAL_ATTRIBUTE_OPERATOR_IN_ATTRIBUTE_NAME_STATE,
    PCHVML_SPECIAL_ATTRIBUTE_OPERATOR_AFTER_ATTRIBUTE_NAME_STATE,
    PCHVML_EJSON_DATA_STATE,
    PCHVML_EJSON_FINISHED_STATE,
    PCHVML_EJSON_CONTROL_STATE,
    PCHVML_EJSON_LEFT_BRACE_STATE,
    PCHVML_EJSON_RIGHT_BRACE_STATE,
    PCHVML_EJSON_LEFT_BRACKET_STATE,
    PCHVML_EJSON_RIGHT_BRACKET_STATE,
    PCHVML_EJSON_LEFT_PARENTHESIS_STATE,
    PCHVML_EJSON_RIGHT_PARENTHESIS_STATE,
    PCHVML_EJSON_DOLLAR_STATE,
    PCHVML_EJSON_AFTER_VALUE_STATE,
    PCHVML_EJSON_BEFORE_NAME_STATE,
    PCHVML_EJSON_AFTER_NAME_STATE,
    PCHVML_EJSON_NAME_UNQUOTED_STATE,
    PCHVML_EJSON_NAME_SINGLE_QUOTED_STATE,
    PCHVML_EJSON_NAME_DOUBLE_QUOTED_STATE,
    PCHVML_EJSON_VALUE_SINGLE_QUOTED_STATE,
    PCHVML_EJSON_VALUE_DOUBLE_QUOTED_STATE,
    PCHVML_EJSON_AFTER_VALUE_DOUBLE_QUOTED_STATE,
    PCHVML_EJSON_VALUE_TWO_DOUBLE_QUOTED_STATE,
    PCHVML_EJSON_VALUE_THREE_DOUBLE_QUOTED_STATE,
    PCHVML_EJSON_KEYWORD_STATE,
    PCHVML_EJSON_AFTER_KEYWORD_STATE,
    PCHVML_EJSON_BYTE_SEQUENCE_STATE,
    PCHVML_EJSON_AFTER_BYTE_SEQUENCE_STATE,
    PCHVML_EJSON_HEX_BYTE_SEQUENCE_STATE,
    PCHVML_EJSON_BINARY_BYTE_SEQUENCE_STATE,
    PCHVML_EJSON_BASE64_BYTE_SEQUENCE_STATE,
    PCHVML_EJSON_VALUE_NUMBER_STATE,
    PCHVML_EJSON_AFTER_VALUE_NUMBER_STATE,
    PCHVML_EJSON_VALUE_NUMBER_INTEGER_STATE,
    PCHVML_EJSON_VALUE_NUMBER_FRACTION_STATE,
    PCHVML_EJSON_VALUE_NUMBER_EXPONENT_STATE,
    PCHVML_EJSON_VALUE_NUMBER_EXPONENT_INTEGER_STATE,
    PCHVML_EJSON_VALUE_NUMBER_SUFFIX_INTEGER_STATE,
    PCHVML_EJSON_VALUE_NUMBER_INFINITY_STATE,
    PCHVML_EJSON_VALUE_NAN_STATE,
    PCHVML_EJSON_STRING_ESCAPE_STATE,
    PCHVML_EJSON_STRING_ESCAPE_FOUR_HEXADECIMAL_DIGITS_STATE,
    PCHVML_EJSON_JSONEE_VARIABLE_STATE,
    PCHVML_EJSON_JSONEE_FULL_STOP_SIGN_STATE,
    PCHVML_EJSON_JSONEE_KEYWORD_STATE,
    PCHVML_EJSON_JSONEE_STRING_STATE,
    PCHVML_EJSON_AFTER_JSONEE_STRING_STATE,
    PCHVML_EJSON_TEMPLATE_DATA_STATE,
    PCHVML_EJSON_TEMPLATE_DATA_LESS_THAN_SIGN_STATE,
    PCHVML_EJSON_TEMPLATE_DATA_END_TAG_OPEN_STATE,
    PCHVML_EJSON_TEMPLATE_DATA_END_TAG_NAME_STATE,
    PCHVML_EJSON_TEMPLATE_FINISHED_STATE,

    PCHVML_LAST_STATE = PCHVML_EJSON_TEMPLATE_FINISHED_STATE,
};

#define PCHVML_STATE_NR \
        (PCHVML_LAST_STATE - PCHVML_FIRST_STATE + 1)

struct pchvml_buffer;
struct pchvml_token;
struct pchvml_rwswrap;
struct pchvml_sbst;

struct pchvml_parser {
    enum pchvml_state state;
    enum pchvml_state return_state;
    struct pchvml_rwswrap* rwswrap;
    struct pchvml_buffer* temp_buffer;
    struct pchvml_buffer* tag_name;
    struct pchvml_buffer* string_buffer;
    struct pchvml_buffer* quoted_buffer;
    struct pchvml_token* token;
    struct pchvml_sbst* sbst;
    struct pcvcm_node* vcm_node;
    struct pcvcm_stack* vcm_stack;
    struct pcutils_stack* ejson_stack;
    uint64_t char_ref_code;
    uint32_t prev_separator;
    bool tag_is_operation;
};

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void pchvml_init_once (void);

struct pchvml_parser* pchvml_create(uint32_t flags, size_t queue_size);

void pchvml_reset(struct pchvml_parser* parser, uint32_t flags,
        size_t queue_size);

void pchvml_destroy(struct pchvml_parser* parser);

struct pchvml_token* pchvml_next_token (struct pchvml_parser* hvml,
                                          purc_rwstream_t rws);

const char* pchvml_get_state_name(enum pchvml_state state);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* PURC_PRIVATE_HVML_H */

