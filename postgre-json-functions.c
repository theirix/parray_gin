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
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
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

/* TODO a little hackish format string */
#define NUMERIC_FMT "99999999999999999999999999999999999999.99999999999999999999999999999999999999"

/*
 * Internal functions declarations
 */
typedef bool (*pextract_type_from_json)(cJSON *elem, DatumPtr result);

Datum json_object_get_generic(PG_FUNCTION_ARGS, int json_type, pextract_type_from_json extract_type_from_json);
Datum json_array_to_array_generic(PG_FUNCTION_ARGS, int json_type, Oid elem_oid, pextract_type_from_json extract_type_from_json);

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
 * - standard PG_FUNCTION_ARGS
 * - json_type a json type to check element for. error occured if element is not passed a check.
 * - extract_type_from_json actual json data extractor
 */
Datum json_object_get_generic(PG_FUNCTION_ARGS, int json_type, pextract_type_from_json extract_type_from_json)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	text *argKey = PG_GETARG_TEXT_P(1);
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
				if (extract_type_from_json(sel, &result))
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
 * Generic json array converter
 * Returns an array datum
 * Args:
 * - standard PG_FUNCTION_ARGS
 * - json_type a json type to check element for. error occured if element is not passed a check.
 * - elem_oid a pg element type
 * - extract_type_from_json actual json data extractor
 */
Datum json_array_to_array_generic(PG_FUNCTION_ARGS, int json_type, Oid elem_oid, pextract_type_from_json extract_type_from_json)
{
	text *argJson = PG_GETARG_TEXT_P(0);
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
				if (!extract_type_from_json(elem, &items[ind]))
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
 * Concrete json data extractors
 */

bool extract_json_string(cJSON *elem, DatumPtr result)
{
	*result = PointerGetDatum(cstring_to_text(elem->valuestring));
	return false;
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
	return json_object_get_generic(fcinfo, cJSON_String, extract_json_string);
}

Datum json_object_get_boolean(PG_FUNCTION_ARGS)
{
	return json_object_get_generic(fcinfo, cJSON_True, extract_json_boolean);
}

Datum json_object_get_int(PG_FUNCTION_ARGS)
{
	return json_object_get_generic(fcinfo, cJSON_Number, extract_json_int);
}

Datum json_object_get_bigint(PG_FUNCTION_ARGS)
{
	return json_object_get_generic(fcinfo, cJSON_Number, extract_json_bigint);
}

Datum json_object_get_numeric(PG_FUNCTION_ARGS)
{
	return json_object_get_generic(fcinfo, cJSON_Number, extract_json_numeric);
}

Datum json_object_get_timestamp(PG_FUNCTION_ARGS)
{
	return json_object_get_generic(fcinfo, cJSON_String, extract_json_timestamp);
}

/*
 * Array functions
 */
Datum json_array_to_text_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic(fcinfo, cJSON_String, TEXTOID, extract_json_string);
}

Datum json_array_to_boolean_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic(fcinfo, cJSON_True, BOOLOID, extract_json_boolean);
}

Datum json_array_to_int_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic(fcinfo, cJSON_Number, INT4OID, extract_json_int);
}

Datum json_array_to_bigint_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic(fcinfo, cJSON_Number, INT8OID, extract_json_bigint);
}

Datum json_array_to_numeric_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic(fcinfo, cJSON_Number, NUMERICOID, extract_json_numeric);
}

Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic(fcinfo, cJSON_String, TIMESTAMPOID, extract_json_timestamp);
}



