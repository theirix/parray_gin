/*-------------------------------------------------------------------------
 *
 * json_accessors.c
 *    several functions for key-based JSON access
 *
 * Copyright (c) 2012, Con Certeza
 * Author: irix <theirix@concerteza.ru>
 *
 * Extension is based on a slightly modified GSON parser
 * (http://sourceforge.net/projects/cjson/)
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
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

/* Log level, usually DEBUG5 (silent) or NOTICE (messages are sent to client side) */
#define PGJSON_TRACE_LEVEL NOTICE

/* TODO a little hackish format string */
#define NUMERIC_FMT "99999999999999999999999999999999999999.99999999999999999999999999999999999999"

/* Choosen to not override with any of cJSON types */
#define CJSON_TYPE_ANY 99

/*
 * Internal functions declarations
 */
typedef bool (*pextract_type_from_json)(cJSON *elem, DatumPtr result);

Datum json_object_get_generic(text *argJson, text *argKey, int json_type, pextract_type_from_json extractor);
Datum json_object_get_generic_args(PG_FUNCTION_ARGS, int json_type, pextract_type_from_json extractor);
Datum json_array_to_array_generic_impl(cJSON *jsonArray, int json_type, Oid elem_oid, pextract_type_from_json extractor);
Datum json_array_to_array_generic(text *argJson, int json_type, Oid elem_oid, pextract_type_from_json extractor);
Datum json_array_to_array_generic_args(PG_FUNCTION_ARGS, int json_type, Oid elem_oid, pextract_type_from_json extractor);

bool match_json_types (int type1, int type2);
const char* json_type_str (int type);
ArrayType* construct_typed_array(Datum *elems, int nelems, Oid elmtype);

/*
 * Extractors
 */
bool extract_json_string(cJSON *elem, DatumPtr result);
bool extract_json_boolean(cJSON *elem, DatumPtr result);
bool extract_json_int(cJSON *elem, DatumPtr result);
bool extract_json_bigint(cJSON *elem, DatumPtr result);
bool extract_json_numeric(cJSON *elem, DatumPtr result);
bool extract_json_timestamp(cJSON *elem, DatumPtr result);

bool extract_text_array(cJSON *elem, DatumPtr result);
bool extract_boolean_array(cJSON *elem, DatumPtr result);
bool extract_int_array(cJSON *elem, DatumPtr result);
bool extract_bigint_array(cJSON *elem, DatumPtr result);
bool extract_numeric_array(cJSON *elem, DatumPtr result);
bool extract_timestamp_array(cJSON *elem, DatumPtr result);

bool extract_json_to_string(cJSON *elem, DatumPtr result);

bool extract_object_array(cJSON *elem, DatumPtr result);

/*
 * Exported functions
 */
Datum json_get_object(PG_FUNCTION_ARGS);
Datum json_get_text(PG_FUNCTION_ARGS);
Datum json_get_boolean(PG_FUNCTION_ARGS);
Datum json_get_int(PG_FUNCTION_ARGS);
Datum json_get_bigint(PG_FUNCTION_ARGS);
Datum json_get_numeric(PG_FUNCTION_ARGS);
Datum json_get_timestamp(PG_FUNCTION_ARGS);

Datum json_array_to_object_array(PG_FUNCTION_ARGS);
Datum json_array_to_multi_array(PG_FUNCTION_ARGS);
Datum json_array_to_text_array(PG_FUNCTION_ARGS);
Datum json_array_to_boolean_array(PG_FUNCTION_ARGS);
Datum json_array_to_int_array(PG_FUNCTION_ARGS);
Datum json_array_to_bigint_array(PG_FUNCTION_ARGS);
Datum json_array_to_numeric_array(PG_FUNCTION_ARGS);
Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS);

Datum json_get_object_array(PG_FUNCTION_ARGS);
Datum json_get_multi_array(PG_FUNCTION_ARGS);
Datum json_get_text_array(PG_FUNCTION_ARGS);
Datum json_get_boolean_array(PG_FUNCTION_ARGS);
Datum json_get_int_array(PG_FUNCTION_ARGS);
Datum json_get_bigint_array(PG_FUNCTION_ARGS);
Datum json_get_numeric_array(PG_FUNCTION_ARGS);
Datum json_get_timestamp_array(PG_FUNCTION_ARGS);



/*
 * Declare V1 exports
 */
PG_FUNCTION_INFO_V1(json_get_object);
PG_FUNCTION_INFO_V1(json_get_text);
PG_FUNCTION_INFO_V1(json_get_boolean);
PG_FUNCTION_INFO_V1(json_get_int);
PG_FUNCTION_INFO_V1(json_get_bigint);
PG_FUNCTION_INFO_V1(json_get_numeric);
PG_FUNCTION_INFO_V1(json_get_timestamp);

PG_FUNCTION_INFO_V1(json_array_to_object_array);
PG_FUNCTION_INFO_V1(json_array_to_multi_array);
PG_FUNCTION_INFO_V1(json_array_to_text_array);
PG_FUNCTION_INFO_V1(json_array_to_boolean_array);
PG_FUNCTION_INFO_V1(json_array_to_int_array);
PG_FUNCTION_INFO_V1(json_array_to_bigint_array);
PG_FUNCTION_INFO_V1(json_array_to_numeric_array);
PG_FUNCTION_INFO_V1(json_array_to_timestamp_array);

PG_FUNCTION_INFO_V1(json_get_object_array);
PG_FUNCTION_INFO_V1(json_get_multi_array);
PG_FUNCTION_INFO_V1(json_get_text_array);
PG_FUNCTION_INFO_V1(json_get_boolean_array);
PG_FUNCTION_INFO_V1(json_get_int_array);
PG_FUNCTION_INFO_V1(json_get_bigint_array);
PG_FUNCTION_INFO_V1(json_get_numeric_array);
PG_FUNCTION_INFO_V1(json_get_timestamp_array);


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
 * Pretty print json type
 */
const char* json_type_str (int type)
{
	switch (type)
	{
		case cJSON_False:	return "bool";
		case cJSON_True:	return "bool";
		case cJSON_NULL:	return "null";
		case cJSON_Number:	return "number";
		case cJSON_String:	return "string";
		case cJSON_Array:	return "array";
		case cJSON_Object:	return "object";
		default:			return "unknown";
	}
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
			if (json_type == CJSON_TYPE_ANY || match_json_types(json_type, sel->type))
			{
				if (extractor(sel, &result))
				{
					status = true;
				}
				else
				{
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("type extractor refused to parse object type %s",
								 json_type_str(json_type))));
				}
			}
			else
			{
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("wrong json type found %s, expected %s",
							 json_type_str(sel->type), json_type_str(json_type))));
			}
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot extract from \"%s\" json object by the key \"%s\"",
						 	strJson, strKey)));
		}
		cJSON_Delete(root);
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot parse json string \"%s\", scalar parser", strJson)));
	}

	pfree(strJson);
	pfree(strKey);

	if (!result)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string cannot be parsed")));

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
 * Works with pre-parsed JSON object
 * Returns an array datum
 * Args:
 * - jsonArray is a json array object
* - json_type a json type to check element for. error occured if element is not passed a check.
 * - elem_oid a pg element type
 * - extractor actual json data extractor
 */
Datum json_array_to_array_generic_impl(cJSON *jsonArray, int json_type, Oid elem_oid, pextract_type_from_json extractor)
{
	Datum *items = NULL;
	ArrayType *array = NULL;
	cJSON *elem;
	int count = 0, ind;

	if (!jsonArray || jsonArray->type != cJSON_Array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("passed json object is not an array")));

	for (elem = jsonArray->child; elem; elem = elem->next)
	{
	    /*todo [alexey]: check this change*/
		if (elem->child)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("no childs allowed")));
		if (!(json_type == CJSON_TYPE_ANY || match_json_types(json_type, elem->type)))
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("expected value of type %s, actual %s at %d position",
						 json_type_str(json_type), json_type_str(elem->type), ind)));
		++count;
	}

	if (count)
	{
		items = (Datum*)palloc(count * sizeof(Datum));

		for (elem = jsonArray->child, ind = 0; elem; elem = elem->next, ++ind)
		{
			if (!extractor(elem, &items[ind]))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("error converting json type %s at %d position",
							 json_type_str(json_type), ind)));
		}
		array = construct_typed_array(items, count, elem_oid);

		pfree(items);
	}

	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array")));

	PG_RETURN_ARRAYTYPE_P(array);
}

/*
 * Generic json array converter
 * Works with string json input
 * Returns an array datum
 * Args:
 * - argJson is a json string
 * - json_type a json type to check element for. error occured if element is not passed a check.
 * - elem_oid a pg element type
 * - extractor actual json data extractor
 */
Datum json_array_to_array_generic(text *argJson, int json_type, Oid elem_oid, pextract_type_from_json extractor)
{
	Datum result;
	char *strJson;
	cJSON *root;

	strJson = text_to_cstring(argJson);

	root = cJSON_Parse(strJson);
	if (root)
	{
		result = json_array_to_array_generic_impl(root, json_type, elem_oid, extractor);
		cJSON_Delete(root);
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot parse json string \"%s\", array parser", strJson)));
	}

	pfree(strJson);

	PG_RETURN_DATUM(result);
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
 * Parse given json object and returns needed datum
 * Datum is passed directly as a result of function
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

bool extract_text_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_String, TEXTOID, extract_json_string);
	return true;
}

bool extract_boolean_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_True, BOOLOID, extract_json_boolean);
	return true;
}

bool extract_int_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_Number, INT4OID, extract_json_int);
	return true;
}

bool extract_bigint_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_Number, INT8OID, extract_json_bigint);
	return true;
}

bool extract_numeric_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_Number, NUMERICOID, extract_json_numeric);
	return true;
}

bool extract_timestamp_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_String, TIMESTAMPOID, extract_json_timestamp);
	return true;
}

bool extract_json_to_string(cJSON *elem, DatumPtr result)
{
	char *pstr;
	pstr = cJSON_PrintUnformatted(elem);
	*result = PointerGetDatum(cstring_to_text(pstr));
	free(pstr);
	return true;
}

bool extract_object_array(cJSON *elem, DatumPtr result)
{
	*result = json_array_to_array_generic_impl(elem, cJSON_String, TEXTOID, extract_json_to_string);
	return true;
}


/**
 *
 * Exported functions
 *
 */


/* Get object by key and converts it back to text
 * Used as a proxy call
 */
Datum json_get_object(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, CJSON_TYPE_ANY, extract_json_to_string);
}

/*
 * Scalar functions
 */
Datum json_get_text(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_String, extract_json_string);
}

Datum json_get_boolean(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_True, extract_json_boolean);
}

Datum json_get_int(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Number, extract_json_int);
}

Datum json_get_bigint(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Number, extract_json_bigint);
}

Datum json_get_numeric(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Number, extract_json_numeric);
}

Datum json_get_timestamp(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_String, extract_json_timestamp);
}

/*
 * Array functions
 */

/**
 *
 * Extracts JSON objects, convert them back to text and return text array
 *
 */
Datum json_array_to_object_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_String, TEXTOID, extract_json_to_string);
}

/**
 *
 * Added to mirror java function, but this one doesn't check element types before converting them to string
 *
 */
Datum json_array_to_multi_array(PG_FUNCTION_ARGS)
{
	return json_array_to_array_generic_args(fcinfo, cJSON_String, TEXTOID, extract_json_to_string);
}

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


/*
 * Indirect array functions
 */

/**
 *
 * Extracts JSON objects, convert them back to text and return text array
 *
 */
Datum json_get_object_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_object_array);
}

/**
 *
 * Added to mirror java function, but this one doesn't check element types before converting them to string
 *
 */
Datum json_get_multi_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_object_array);
}

Datum json_get_text_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_text_array);
}

Datum json_get_boolean_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_boolean_array);
}

Datum json_get_int_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_int_array);
}

Datum json_get_bigint_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_bigint_array);
}

Datum json_get_numeric_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_numeric_array);
}

Datum json_get_timestamp_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_timestamp_array);
}

/* vim: set noexpandtab tabstop=4 shiftwidth=4: */
