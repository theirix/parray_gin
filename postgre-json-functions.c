/*-------------------------------------------------------------------------
 *
 * postgre-json-functions.c
 *    several functions for key-based JSON access
 *
 * Copyright (c) 2012, Con Certeza
 * Author: irix <theirix@concerteza.ru>
 *
 * Extension is based on a slightly modified GSON parser
 * (https://sites.google.com/site/gson/)
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "access/skey.h"
#include "utils/fmgroids.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/int8.h"
#include "utils/timestamp.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/formatting.h"

/*
 * Note. cJSON should be patched for this extension.
 * Of course we should contribute a patch to upstream
 */
#include "cJSON.h"

PG_MODULE_MAGIC;

/* log level, usually DEBUG5 (silent) or NOTICE (messages are sent to client side) */
#define PGJSON_TRACE_LEVEL NOTICE

/* 
 * Strategy */
/* @> operator strategy */
#define PGJSON_STRATEGY_CONTAINS 7
/* <@ operator strategy */
#define PGJSON_STRATEGY_CONTAINED_BY 8

/* TODO a little hackish format string */
#define NUMERIC_FMT "99999999999999999999999999999999999999.99999999999999999999999999999999999999"

/*
 * Internal functions declarations
 */
typedef bool (*pextract_type_from_json)(cJSON *elem, DatumPtr result);

Datum json_object_get_generic(text *argJson, text *argKey, int json_type, pextract_type_from_json extractor);
Datum json_object_get_generic_args(PG_FUNCTION_ARGS, int json_type, pextract_type_from_json extractor);
Datum json_array_to_array_generic(text *argJson, int json_type, Oid elem_oid, pextract_type_from_json extractor);
Datum json_array_to_array_generic_args(PG_FUNCTION_ARGS, int json_type, Oid elem_oid, pextract_type_from_json extractor);

bool match_json_types (int type1, int type2);
ArrayType* construct_typed_array(Datum *elems, int nelems, Oid elmtype);

bool extract_json_string(cJSON *elem, DatumPtr result);
bool extract_json_boolean(cJSON *elem, DatumPtr result);
bool extract_json_int(cJSON *elem, DatumPtr result);
bool extract_json_bigint(cJSON *elem, DatumPtr result);
bool extract_json_numeric(cJSON *elem, DatumPtr result);
bool extract_json_timestamp(cJSON *elem, DatumPtr result);

/*
 * Exported functions
 */
Datum json_object_get_text(PG_FUNCTION_ARGS);
Datum json_object_get_boolean(PG_FUNCTION_ARGS);
Datum json_object_get_int(PG_FUNCTION_ARGS);
Datum json_object_get_bigint(PG_FUNCTION_ARGS);
Datum json_object_get_numeric(PG_FUNCTION_ARGS);
Datum json_object_get_timestamp(PG_FUNCTION_ARGS);
Datum json_array_to_text_array(PG_FUNCTION_ARGS);
Datum json_array_to_boolean_array(PG_FUNCTION_ARGS);
Datum json_array_to_int_array(PG_FUNCTION_ARGS);
Datum json_array_to_bigint_array(PG_FUNCTION_ARGS);
Datum json_array_to_numeric_array(PG_FUNCTION_ARGS);
Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS);

Datum json_gin_compare(PG_FUNCTION_ARGS);
Datum json_gin_extract_value(PG_FUNCTION_ARGS);
Datum json_gin_extract_query(PG_FUNCTION_ARGS);
Datum json_gin_consistent(PG_FUNCTION_ARGS);
Datum json_gin_compare_partial(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(json_object_get_text);
PG_FUNCTION_INFO_V1(json_object_get_boolean);
PG_FUNCTION_INFO_V1(json_object_get_int);
PG_FUNCTION_INFO_V1(json_object_get_bigint);
PG_FUNCTION_INFO_V1(json_object_get_numeric);
PG_FUNCTION_INFO_V1(json_object_get_timestamp);
PG_FUNCTION_INFO_V1(json_array_to_text_array);
PG_FUNCTION_INFO_V1(json_array_to_boolean_array);
PG_FUNCTION_INFO_V1(json_array_to_int_array);
PG_FUNCTION_INFO_V1(json_array_to_bigint_array);
PG_FUNCTION_INFO_V1(json_array_to_numeric_array);
PG_FUNCTION_INFO_V1(json_array_to_timestamp_array);

PG_FUNCTION_INFO_V1(json_gin_compare);
PG_FUNCTION_INFO_V1(json_gin_extract_value);
PG_FUNCTION_INFO_V1(json_gin_extract_query);
PG_FUNCTION_INFO_V1(json_gin_consistent);
PG_FUNCTION_INFO_V1(json_gin_compare_partial);

/**
 *
 * Internal functions
 *
 */

/*
 * Check json type equivalence
 * Should handle true/false as a single boolean type
 */
bool match_json_types (int type1, int type2)
{
	return (type1 == cJSON_True || type1 == cJSON_False)
		? (type2 == cJSON_True || type2 == cJSON_False)
		: type1 == type2;
}

/*
 * Shortcut array construction call
 */
ArrayType* construct_typed_array(Datum *elems, int nelems, Oid elmtype)
{
	int16   elmlen;
	bool    elmbyval;
	char    elmalign;
	get_typlenbyvalalign(elmtype, &elmlen, &elmbyval, &elmalign);
	return construct_array(elems, nelems, elmtype, elmlen, elmbyval, elmalign);
}

/*
 * Generic json scalar value converter
 * Returns a datum with pg value (value or reference, depends on type)
 * Args:
 * - argJson is a json string
 * - argKey is a string key
 * - json_type a json type to check element for. error occured if element is not passed a check.
 * - extractor actual json data extractor
 */
Datum json_object_get_generic(text *argJson, text *argKey, int json_type, pextract_type_from_json extractor)
{
	bool status = false;
	Datum result;
	char *strJson, *strKey;
	cJSON *root, *sel;

	strJson = text_to_cstring(argJson);
	strKey = text_to_cstring(argKey);

	root = cJSON_Parse(strJson);
	if (root)
	{
		sel = cJSON_GetObjectItem(root, strKey);
		if (sel)
		{
			if (match_json_types(json_type, sel->type))
			{
				if (extractor(sel, &result))
					status = true;
			}
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
	pfree(strKey);

	if (!status)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is of a wrong type")));

	PG_RETURN_DATUM(result);
}

/*
 * Generic json scalar value converter (PG_FUNCTION_ARGS version)
 */
Datum json_object_get_generic_args(PG_FUNCTION_ARGS, int json_type, pextract_type_from_json extractor)
{
	return json_object_get_generic(PG_GETARG_TEXT_P(0), PG_GETARG_TEXT_P(1), json_type, extractor);
}

/*
 * Generic json array converter
 * Returns an array datum
 * Args:
 * - argJson is a json string
 * - json_type a json type to check element for. error occured if element is not passed a check.
 * - elem_oid a pg element type
 * - extractor actual json data extractor
 */
Datum json_array_to_array_generic(text *argJson, int json_type, Oid elem_oid, pextract_type_from_json extractor)
{
	Datum *items = NULL;
	ArrayType *array = NULL;
	char *strJson;
	cJSON *root, *elem;
	int count = 0, ind;

	strJson = text_to_cstring(argJson);

	root = cJSON_Parse(strJson);
	if (root)
	{
		for (elem = root->child; elem; elem = elem->next)
		{
			if (elem->child)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("no childs allowed")));
			if (!match_json_types(json_type, elem->type))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected XXX int value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				if (!extractor(elem, &items[ind]))
					ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("error converting json type XXX at %d position", ind)));
			}
			array = construct_typed_array(items, count, elem_oid);

			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);

	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with XXX")));

	PG_RETURN_ARRAYTYPE_P(array);
}

/*
 * Generic json array converter (PG_FUNCTION_ARGS version)
 */
Datum json_array_to_array_generic_args(PG_FUNCTION_ARGS, int json_type, Oid elem_oid, pextract_type_from_json extractor)
{
	return json_array_to_array_generic(PG_GETARG_TEXT_P(0), json_type, elem_oid, extractor);
}

/*
 * Concrete json data extractors
 */

bool extract_json_string(cJSON *elem, DatumPtr result)
{
	*result = PointerGetDatum(cstring_to_text(elem->valuestring));
	return true;
}

bool extract_json_boolean(cJSON *elem, DatumPtr result)
{
	if (elem->type == cJSON_True)
	{
		*result = BoolGetDatum(1);
		return true;
	}
	else if (elem->type == cJSON_False)
	{
		*result = BoolGetDatum(0);
		return true;
	}
	return false;
}

bool extract_json_int(cJSON *elem, DatumPtr result)
{
	*result = Int32GetDatum(elem->valueint);
	return true;
}

bool extract_json_bigint(cJSON *elem, DatumPtr result)
{
	*result = DirectFunctionCall1(int8in, CStringGetDatum(elem->valuestring));
	return true;
}

bool extract_json_numeric(cJSON *elem, DatumPtr result)
{
	*result = DirectFunctionCall2(numeric_to_number,
			PointerGetDatum(cstring_to_text(elem->valuestring)),
			PointerGetDatum(cstring_to_text(NUMERIC_FMT)));
	return true;
}

bool extract_json_timestamp(cJSON *elem, DatumPtr result)
{
	Datum timestampWithTz;
	/* format: yyyy-MM-dd HH:mm:ss */
	timestampWithTz = DirectFunctionCall2(to_timestamp,
			PointerGetDatum(cstring_to_text(elem->valuestring)),
			PointerGetDatum(cstring_to_text("YYYY-MM-DD HH:MI:SS")));
	Assert(timestampWithTz);
	*result = DirectFunctionCall1(timestamptz_timestamp,
			timestampWithTz);
	return true;
}

/**
 *
 * Exported functions
 *
 */


/*
 * Scalar functions
 */
Datum json_object_get_text(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_String, extract_json_string);
}

Datum json_object_get_boolean(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_True, extract_json_boolean);
}

Datum json_object_get_int(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Number, extract_json_int);
}

Datum json_object_get_bigint(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Number, extract_json_bigint);
}

Datum json_object_get_numeric(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Number, extract_json_numeric);
}

Datum json_object_get_timestamp(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_String, extract_json_timestamp);
}

/*
 * Array functions
 */
Datum json_array_to_text_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_String, TEXTOID, extract_json_string);
}

Datum json_array_to_boolean_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_True, BOOLOID, extract_json_boolean);
}

Datum json_array_to_int_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_Number, INT4OID, extract_json_int);
}

Datum json_array_to_bigint_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_Number, INT8OID, extract_json_bigint);
}

Datum json_array_to_numeric_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_Number, NUMERICOID, extract_json_numeric);
}

Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_String, TIMESTAMPOID, extract_json_timestamp);
}

/**
 *
 * GIN support
 *
 */

Datum
json_gin_compare(PG_FUNCTION_ARGS)
{
	text *key1 = PG_GETARG_TEXT_P(0);
	text *key2 = PG_GETARG_TEXT_P(1);

	char *pstr1, *pstr2;
	int32 result;

	result = DatumGetInt32(DirectFunctionCall2(bttextcmp, PointerGetDatum(key1), PointerGetDatum(key2)));

	/* always log */
	pstr1 = text_to_cstring(key1);
	pstr2 = text_to_cstring(key2);
	elog(PGJSON_TRACE_LEVEL, "GIN compare: key=%s vs key=%s -> %d", pstr1, pstr2, result);
	pfree(pstr1);
	pfree(pstr2);

	PG_RETURN_INT32(result);
}

/* (Datum itemValue, int32 *nkeys, bool **nullFlags) */
Datum
json_gin_extract_value(PG_FUNCTION_ARGS)
{
	text *itemValue = PG_GETARG_TEXT_P(0);
	int32 *nkeys = (int32*)PG_GETARG_POINTER(1);

	char *pstr;
	ArrayType *keys;

	keys = DatumGetArrayTypeP(json_array_to_array_generic(itemValue, cJSON_String, TEXTOID, extract_json_string));
	if (keys)
		*nkeys = ARR_DIMS(keys)[0];
	else
		*nkeys = 0;
	
	/* always log */
	pstr = text_to_cstring(itemValue);
	elog(PGJSON_TRACE_LEVEL, "GIN extract_value: json=%s -> %d items", pstr, *nkeys);
	pfree(pstr);

	PG_RETURN_POINTER(keys);
}

/* (Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch, Pointer **extra_data, bool **nullFlags, int32 *searchMode) */
Datum
json_gin_extract_query(PG_FUNCTION_ARGS)
{
	Datum query = PG_GETARG_DATUM(0);
	int32 *nkeys = (int32*)PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool **nullFlags = (bool**)PG_GETARG_POINTER(5);
	
	ArrayType *queryArray;
	Datum *keys;

	int16 elmlen;
	bool elmbyval;
	char elmalign;
	int nelems;

	if (strategy == PGJSON_STRATEGY_CONTAINS)
	{
		queryArray = DatumGetArrayTypePCopy(query);
		/* query is an array of texts, parse it and return */
		get_typlenbyvalalign(ARR_ELEMTYPE(queryArray),
				&elmlen, &elmbyval, &elmalign);

		deconstruct_array(queryArray, ARR_ELEMTYPE(queryArray),
				elmlen, elmbyval, elmalign,
				&keys, nullFlags, &nelems);

		*nkeys = nelems;
	}
	else 
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}

	/* always log */
	elog(PGJSON_TRACE_LEVEL, "GIN extract_query: json= strategy=%d -> %d items", 
			strategy, *nkeys);
	
	PG_RETURN_POINTER(keys);
}

/* bool check[], StrategyNumber n, Datum query, int32 nkeys,
		Pointer extra_data[], bool *recheck, Datum queryKeys[], bool nullFlags[]*/
Datum
json_gin_consistent(PG_FUNCTION_ARGS)
{
	bool *check = (bool*)PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);
	/*Datum query = PG_GETARG_DATUM(2);*/
	int32 nkeys = PG_GETARG_INT32(3);
	bool *recheck = (bool*)PG_GETARG_POINTER(5);

	bool result = false;
	int i;

	if (strategy == PGJSON_STRATEGY_CONTAINS)
	{
		*recheck = true;
		result = true;
		for (i = 0; i < nkeys; ++i)
		{
			if (!check[i])
				result = false;
		}
	}
	else 
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}
	
	/* always log */
	elog(PGJSON_TRACE_LEVEL, "GIN consistent: %d keys, strategy=%d -> %s", 
			nkeys, strategy, result?"true":"false");

	PG_RETURN_BOOL(result);
}

Datum
json_gin_compare_partial(PG_FUNCTION_ARGS)
{
	/* not used */
	PG_RETURN_NULL();
}

/* vim: set noexpandtab tabstop=4 shiftwidth=4: */
