/*
 * @file test_parser_xml.c
 * @author: Radek Krejci <rkrejci@cesnet.cz>
 * @brief unit tests for functions from parser_xml.c
 *
 * Copyright (c) 2019 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include "../../src/libyang.h"

#define BUFSIZE 1024
char logbuf[BUFSIZE] = {0};
int store = -1; /* negative for infinite logging, positive for limited logging */

struct state_s {
    void *func;
    struct ly_ctx *ctx;
};

/* set to 0 to printing error messages to stderr instead of checking them in code */
#define ENABLE_LOGGER_CHECKING 1

#if ENABLE_LOGGER_CHECKING
static void
logger(LY_LOG_LEVEL level, const char *msg, const char *path)
{
    (void) level; /* unused */
    if (store) {
        if (path && path[0]) {
            snprintf(logbuf, BUFSIZE - 1, "%s %s", msg, path);
        } else {
            strncpy(logbuf, msg, BUFSIZE - 1);
        }
        if (store > 0) {
            --store;
        }
    }
}
#endif

static int
setup(void **state)
{
    struct state_s *s;
    const char *schema_a = "module defs {namespace urn:tests:defs;prefix d;yang-version 1.1;"
            "identity crypto-alg; identity interface-type; identity ethernet {base interface-type;} identity fast-ethernet {base ethernet;}}";
    const char *schema_b = "module types {namespace urn:tests:types;prefix t;yang-version 1.1; import defs {prefix defs;}"
            "feature f; identity gigabit-ethernet { base defs:ethernet;}"
            "leaf binary {type binary {length 5 {error-message \"This base64 value must be of length 5.\";}}}"
            "leaf binary-norestr {type binary;}"
            "leaf int8 {type int8 {range 10..20;}}"
            "leaf uint8 {type uint8 {range 150..200;}}"
            "leaf int16 {type int16 {range -20..-10;}}"
            "leaf uint16 {type uint16 {range 150..200;}}"
            "leaf int32 {type int32;}"
            "leaf uint32 {type uint32;}"
            "leaf int64 {type int64;}"
            "leaf uint64 {type uint64;}"
            "leaf bits {type bits {bit zero; bit one {if-feature f;} bit two;}}"
            "leaf enums {type enumeration {enum white; enum yellow {if-feature f;}}}"
            "leaf dec64 {type decimal64 {fraction-digits 1; range 1.5..10;}}"
            "leaf dec64-norestr {type decimal64 {fraction-digits 18;}}"
            "leaf str {type string {length 8..10; pattern '[a-z ]*';}}"
            "leaf str-norestr {type string;}"
            "leaf bool {type boolean;}"
            "leaf empty {type empty;}"
            "leaf ident {type identityref {base defs:interface-type;}}}";

    s = calloc(1, sizeof *s);
    assert_non_null(s);

#if ENABLE_LOGGER_CHECKING
    ly_set_log_clb(logger, 1);
#endif

    assert_int_equal(LY_SUCCESS, ly_ctx_new(NULL, 0, &s->ctx));
    assert_non_null(lys_parse_mem(s->ctx, schema_a, LYS_IN_YANG));
    assert_non_null(lys_parse_mem(s->ctx, schema_b, LYS_IN_YANG));

    *state = s;

    return 0;
}

static int
teardown(void **state)
{
    struct state_s *s = (struct state_s*)(*state);

#if ENABLE_LOGGER_CHECKING
    if (s->func) {
        fprintf(stderr, "%s\n", logbuf);
    }
#endif

    ly_ctx_destroy(s->ctx, NULL);
    free(s);

    return 0;
}

void
logbuf_clean(void)
{
    logbuf[0] = '\0';
}

#if ENABLE_LOGGER_CHECKING
#   define logbuf_assert(str) assert_string_equal(logbuf, str)
#else
#   define logbuf_assert(str)
#endif

static void
test_int(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_int;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<int8 xmlns=\"urn:tests:types\">\n 15 \t\n  </int8>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("int8", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("15", leaf->value.canonized);
    assert_int_equal(15, leaf->value.int8);
    lyd_free_all(tree);

    /* invalid range */
    data = "<int8 xmlns=\"urn:tests:types\">1</int8>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"1\" does not satisfy the range constraint. /");

    data = "<int16 xmlns=\"urn:tests:types\">100</int16>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"100\" does not satisfy the range constraint. /");

    /* invalid value */
    data = "<int32 xmlns=\"urn:tests:types\">0x01</int32>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid int32 value \"0x01\". /");

    data = "<int64 xmlns=\"urn:tests:types\"></int64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty int64 value. /");

    data = "<int64 xmlns=\"urn:tests:types\">   </int64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty int64 value. /");

    data = "<int64 xmlns=\"urn:tests:types\">-10  xxx</int64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid int64 value \"-10  xxx\". /");

    s->func = NULL;
}

static void
test_uint(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_uint;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<uint8 xmlns=\"urn:tests:types\">\n 150 \t\n  </uint8>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("uint8", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("150", leaf->value.canonized);
    assert_int_equal(150, leaf->value.uint8);
    lyd_free_all(tree);

    /* invalid range */
    data = "<uint8 xmlns=\"urn:tests:types\">\n 15 \t\n  </uint8>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"15\" does not satisfy the range constraint. /");

    data = "<uint16 xmlns=\"urn:tests:types\">\n 1500 \t\n  </uint16>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"1500\" does not satisfy the range constraint. /");

    /* invalid value */
    data = "<uint32 xmlns=\"urn:tests:types\">-10</uint32>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid uint32 value \"-10\". /");

    data = "<uint64 xmlns=\"urn:tests:types\"/>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty uint64 value. /");

    data = "<uint64 xmlns=\"urn:tests:types\">   </uint64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty uint64 value. /");

    data = "<uint64 xmlns=\"urn:tests:types\">10  xxx</uint64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid 5. character of uint64 value \"10  xxx\". /");

    s->func = NULL;
}

static void
test_dec64(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_dec64;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<dec64 xmlns=\"urn:tests:types\">\n +8 \t\n  </dec64>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("dec64", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("8.0", leaf->value.canonized);
    assert_int_equal(80, leaf->value.dec64);
    lyd_free_all(tree);

    data = "<dec64 xmlns=\"urn:tests:types\">8.00</dec64>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("dec64", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("8.0", leaf->value.canonized);
    assert_int_equal(80, leaf->value.dec64);
    lyd_free_all(tree);

    data = "<dec64-norestr xmlns=\"urn:tests:types\">-9.223372036854775808</dec64-norestr>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("dec64-norestr", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("-9.223372036854775808", leaf->value.canonized);
    assert_int_equal(INT64_C(-9223372036854775807) - INT64_C(1), leaf->value.dec64);
    lyd_free_all(tree);

    data = "<dec64-norestr xmlns=\"urn:tests:types\">9.223372036854775807</dec64-norestr>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("dec64-norestr", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("9.223372036854775807", leaf->value.canonized);
    assert_int_equal(INT64_C(9223372036854775807), leaf->value.dec64);
    lyd_free_all(tree);

    /* invalid range */
    data = "<dec64 xmlns=\"urn:tests:types\">\n 15 \t\n  </dec64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"15.0\" does not satisfy the range constraint. /");

    data = "<dec64 xmlns=\"urn:tests:types\">\n 0 \t\n  </dec64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"0.0\" does not satisfy the range constraint. /");

    /* invalid value */
    data = "<dec64 xmlns=\"urn:tests:types\">xxx</dec64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid 1. character of decimal64 value \"xxx\". /");

    data = "<dec64 xmlns=\"urn:tests:types\"/>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty decimal64 value. /");

    data = "<dec64 xmlns=\"urn:tests:types\">   </dec64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty decimal64 value. /");

    data = "<dec64 xmlns=\"urn:tests:types\">8.5  xxx</dec64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid 6. character of decimal64 value \"8.5  xxx\". /");

    data = "<dec64 xmlns=\"urn:tests:types\">8.55  xxx</dec64>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Value \"8.55\" of decimal64 type exceeds defined number (1) of fraction digits. /");

    s->func = NULL;
}

static void
test_string(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_string;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<str xmlns=\"urn:tests:types\">teststring</str>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("str", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("teststring", leaf->value.canonized);
    lyd_free_all(tree);

    /* invalid length */
    data = "<str xmlns=\"urn:tests:types\">short</str>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Length \"5\" does not satisfy the length constraint. /");

    data = "<str xmlns=\"urn:tests:types\">tooooo long</str>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Length \"11\" does not satisfy the length constraint. /");

    /* invalid pattern */
    data = "<str xmlns=\"urn:tests:types\">string15</str>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("String \"string15\" does not conforms to the 1. pattern restriction of its type. /");

    s->func = NULL;
}

static void
test_bits(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_bits;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<bits xmlns=\"urn:tests:types\">\n two    \t\nzero\n  </bits>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("bits", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("zero two", leaf->value.canonized);
    lyd_free_all(tree);

    data = "<bits xmlns=\"urn:tests:types\">zero  two</bits>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("bits", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("zero two", leaf->value.canonized);
    lyd_free_all(tree);

    /* disabled feature */
    data = "<bits xmlns=\"urn:tests:types\"> \t one \n\t </bits>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Bit \"one\" is disabled by its 1. if-feature condition. /");

    /* enable that feature */
    assert_int_equal(LY_SUCCESS, lys_feature_enable(ly_ctx_get_module(s->ctx, "types", NULL), "f"));
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("bits", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("one", leaf->value.canonized);
    lyd_free_all(tree);

    /* multiple instances of the bit */
    data = "<bits xmlns=\"urn:tests:types\">one zero one</bits>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Bit \"one\" used multiple times. /");

    /* invalid bit value */
    data = "<bits xmlns=\"urn:tests:types\">one xero one</bits>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid bit value \"xero\". /");

    s->func = NULL;
}

static void
test_enums(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_enums;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<enums xmlns=\"urn:tests:types\">white</enums>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("enums", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("white", leaf->value.canonized);
    lyd_free_all(tree);

    /* disabled feature */
    data = "<enums xmlns=\"urn:tests:types\">yellow</enums>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Enumeration \"yellow\" is disabled by its 1. if-feature condition. /");

    /* enable that feature */
    assert_int_equal(LY_SUCCESS, lys_feature_enable(ly_ctx_get_module(s->ctx, "types", NULL), "f"));
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("enums", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("yellow", leaf->value.canonized);
    lyd_free_all(tree);

    /* leading/trailing whitespaces are not valid */
    data = "<enums xmlns=\"urn:tests:types\"> white</enums>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid enumeration value \" white\". /");
    data = "<enums xmlns=\"urn:tests:types\">white\n</enums>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid enumeration value \"white\n\". /");

    /* invalid enumeration value */
    data = "<enums xmlns=\"urn:tests:types\">black</enums>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid enumeration value \"black\". /");

    s->func = NULL;
}

static void
test_binary(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_binary;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<binary xmlns=\"urn:tests:types\">\n   aGVs\nbG8=  \t\n  </binary>"
                       "<binary-norestr xmlns=\"urn:tests:types\">TQ==</binary-norestr>";

    /* valid data (hello) */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("binary", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("aGVs\nbG8=", leaf->value.canonized);
    assert_non_null(tree = tree->next);
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("binary-norestr", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("TQ==", leaf->value.canonized);
    lyd_free_all(tree);

    /* no data */
    data = "<binary-norestr xmlns=\"urn:tests:types\">\n    \t\n  </binary-norestr>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("binary-norestr", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("", leaf->value.canonized);
    lyd_free_all(tree);
    data = "<binary-norestr xmlns=\"urn:tests:types\"></binary-norestr>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("binary-norestr", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("", leaf->value.canonized);
    lyd_free_all(tree);
    data = "<binary-norestr xmlns=\"urn:tests:types\"/>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("binary-norestr", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("", leaf->value.canonized);
    lyd_free_all(tree);

    /* invalid base64 character */
    data = "<binary-norestr xmlns=\"urn:tests:types\">a@bcd=</binary-norestr>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid Base64 character (@). /");

    /* missing data */
    data = "<binary-norestr xmlns=\"urn:tests:types\">aGVsbG8</binary-norestr>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Base64 encoded value length must be divisible by 4. /");
    data = "<binary-norestr xmlns=\"urn:tests:types\">VsbG8=</binary-norestr>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Base64 encoded value length must be divisible by 4. /");

    /* invalid binary length */
    data = "<binary xmlns=\"urn:tests:types\">aGVsbG93b3JsZA==</binary>"; /* helloworld */
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("This base64 value must be of length 5. /");
    data = "<binary xmlns=\"urn:tests:types\">TQ==</binary>"; /* M */
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("This base64 value must be of length 5. /");

    s->func = NULL;
}

static void
test_boolean(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_boolean;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<bool xmlns=\"urn:tests:types\">true</bool>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("bool", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("true", leaf->value.canonized);
    assert_int_equal(1, leaf->value.boolean);
    lyd_free_all(tree);

    data = "<bool xmlns=\"urn:tests:types\">false</bool>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("bool", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("false", leaf->value.canonized);
    assert_int_equal(0, leaf->value.boolean);
    lyd_free_all(tree);

    /* invalid value */
    data = "<bool xmlns=\"urn:tests:types\">unsure</bool>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid boolean value \"unsure\". /");

    data = "<bool xmlns=\"urn:tests:types\"> true</bool>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid boolean value \" true\". /");

    s->func = NULL;
}

static void
test_empty(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_empty;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<empty xmlns=\"urn:tests:types\"></empty>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("empty", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("", leaf->value.canonized);
    lyd_free_all(tree);

    data = "<empty xmlns=\"urn:tests:types\"/>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("empty", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("", leaf->value.canonized);
    lyd_free_all(tree);

    /* invalid value */
    data = "<empty xmlns=\"urn:tests:types\">x</empty>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty value \"x\". /");

    data = "<empty xmlns=\"urn:tests:types\"> </empty>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid empty value \" \". /");

    s->func = NULL;
}

static void
test_identityref(void **state)
{
    struct state_s *s = (struct state_s*)(*state);
    s->func = test_identityref;

    struct lyd_node *tree;
    struct lyd_node_term *leaf;

    const char *data = "<ident xmlns=\"urn:tests:types\">gigabit-ethernet</ident>";

    /* valid data */
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("ident", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("gigabit-ethernet", leaf->value.canonized);
    lyd_free_all(tree);

    data = "<ident xmlns=\"urn:tests:types\" xmlns:x=\"urn:tests:defs\">x:fast-ethernet</ident>";
    assert_non_null(tree = lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    assert_int_equal(LYS_LEAF, tree->schema->nodetype);
    assert_string_equal("ident", tree->schema->name);
    leaf = (struct lyd_node_term*)tree;
    assert_string_equal("fast-ethernet", leaf->value.canonized);
    lyd_free_all(tree);

    /* invalid value */
    data = "<ident xmlns=\"urn:tests:types\">fast-ethernet</ident>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid identityref \"fast-ethernet\" value - identity not found. /");

    data = "<ident xmlns=\"urn:tests:types\" xmlns:x=\"urn:tests:defs\">x:slow-ethernet</ident>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid identityref \"x:slow-ethernet\" value - identity not found. /");

    data = "<ident xmlns=\"urn:tests:types\" xmlns:x=\"urn:tests:defs\">x:crypto-alg</ident>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid identityref \"x:crypto-alg\" value - identity not accepted by the type specification. /");

    data = "<ident xmlns=\"urn:tests:types\" xmlns:x=\"urn:tests:unknown\">x:fast-ethernet</ident>";
    assert_null(lyd_parse_mem(s->ctx, data, LYD_XML, 0));
    logbuf_assert("Invalid identityref \"x:fast-ethernet\" value - unable to map prefix to YANG schema. /");

    s->func = NULL;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_int, setup, teardown),
        cmocka_unit_test_setup_teardown(test_uint, setup, teardown),
        cmocka_unit_test_setup_teardown(test_dec64, setup, teardown),
        cmocka_unit_test_setup_teardown(test_string, setup, teardown),
        cmocka_unit_test_setup_teardown(test_bits, setup, teardown),
        cmocka_unit_test_setup_teardown(test_enums, setup, teardown),
        cmocka_unit_test_setup_teardown(test_binary, setup, teardown),
        cmocka_unit_test_setup_teardown(test_boolean, setup, teardown),
        cmocka_unit_test_setup_teardown(test_empty, setup, teardown),
        cmocka_unit_test_setup_teardown(test_identityref, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}