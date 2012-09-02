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
#include "access/tuptoaster.h"
#include "utils/fmgroids.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/int8.h"
#include "utils/timestamp.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/formatting.h"
#include "utils/fmgroids.h"

/*
 * Note. cJSON should be patched for this extension.
 * Of course we should contribute a patch to upstream
 */
#include "cJSON.h"

PG_MODULE_MAGIC;

/* log level, usually DEBUG5 (silent) or NOTICE (messages are sent to client side) */
#define PGJSON_TRACE_LEVEL NOTICE
#define TRACE_LIKE_HELL 1

/* 
 * Strategy */
/* @> operator strategy */
#define PGJSON_STRATEGY_CONTAINS 7
/* @@> operator strategy */
#define PGJSON_STRATEGY_CONTAINS_PARTIAL 8
/* <@@ operator strategy 
#define PGJSON_STRATEGY_CONTAINED_BY_PARTIAL 9 */

/* TODO a little hackish format string */
#define NUMERIC_FMT "99999999999999999999999999999999999999.99999999999999999999999999999999999999"

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

int32 gin_compare_string_partial(char *a, int lena, char *b, int lenb);

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

Datum json_object_get_text_array(PG_FUNCTION_ARGS);
Datum json_object_get_boolean_array(PG_FUNCTION_ARGS);
Datum json_object_get_int_array(PG_FUNCTION_ARGS);
Datum json_object_get_bigint_array(PG_FUNCTION_ARGS);
Datum json_object_get_numeric_array(PG_FUNCTION_ARGS);
Datum json_object_get_timestamp_array(PG_FUNCTION_ARGS);

Datum json_gin_compare(PG_FUNCTION_ARGS);
Datum json_gin_extract_value(PG_FUNCTION_ARGS);
Datum json_gin_extract_query(PG_FUNCTION_ARGS);
Datum json_gin_consistent(PG_FUNCTION_ARGS);
Datum json_gin_compare_partial(PG_FUNCTION_ARGS);

Datum json_op_text_array_contains_partial(PG_FUNCTION_ARGS);
Datum json_op_text_array_contained_partial(PG_FUNCTION_ARGS);
Datum text_equal_partial(PG_FUNCTION_ARGS);


/*
 * Declare V1 exports
 */
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

PG_FUNCTION_INFO_V1(json_object_get_text_array);
PG_FUNCTION_INFO_V1(json_object_get_boolean_array);
PG_FUNCTION_INFO_V1(json_object_get_int_array);
PG_FUNCTION_INFO_V1(json_object_get_bigint_array);
PG_FUNCTION_INFO_V1(json_object_get_numeric_array);
PG_FUNCTION_INFO_V1(json_object_get_timestamp_array);

PG_FUNCTION_INFO_V1(json_gin_compare);
PG_FUNCTION_INFO_V1(json_gin_extract_value);
PG_FUNCTION_INFO_V1(json_gin_extract_query);
PG_FUNCTION_INFO_V1(json_gin_consistent);
PG_FUNCTION_INFO_V1(json_gin_compare_partial);

PG_FUNCTION_INFO_V1(json_op_text_array_contains_partial);
PG_FUNCTION_INFO_V1(json_op_text_array_contained_partial);
PG_FUNCTION_INFO_V1(text_equal_partial);

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
			if (match_json_types(json_type, sel->type))
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

	// TODO leak
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
		if (elem->child)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("no childs allowed")));
		if (!match_json_types(json_type, elem->type))
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


/*
 * Indirect array functions
 */
Datum json_object_get_text_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_text_array);
}

Datum json_object_get_boolean_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_boolean_array);
}

Datum json_object_get_int_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_int_array);
}

Datum json_object_get_bigint_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_bigint_array);
}

Datum json_object_get_numeric_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_numeric_array);
}

Datum json_object_get_timestamp_array(PG_FUNCTION_ARGS)
{
	return json_object_get_generic_args(fcinfo, cJSON_Array, extract_timestamp_array);
}

/**
 *
 * Operator support
 *
 */

/*Datum
json_op_text_array_contains(PG_FUNCTION_ARGS)
{
	ArrayType *array_a = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType *array_b = PG_GETARG_ARRAYTYPE_P(1);

	bool result;
	text *a, *b;
	int acount, bcount;
	int i,j;

	if ((ARR_HASNULL(a) && array_contains_nulls(a)) || 
		(ARR_HASNULL(b) && array_contains_nulls(b)))
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("array must not contain nulls")));

	acount = ARR_DIMS(a)[0];
	bcount = ARR_DIMS(b)[0];

	for (i = 0; i < bcount; ++i)
	{
		for (j = 0; j < acount; ++j)
		{
			if (
		}

	}


	PG_RETURN_BOOL(result);
}*/


static bool
text_array_contains_partial(ArrayType *array1, ArrayType *array2, Oid collation, bool matchall, bool partial)
{
	bool		result = matchall;
	Oid			element_type = ARR_ELEMTYPE(array1);
	int			nelems1;
	Datum		*values2;
	bool		*nulls2;
	int			nelems2;
	int16		typlen16;
	int			typlen;
	bool		typbyval;
	char		typalign;
	char		*ptr1;
	bits8		*bitmap1;
	int			bitmask;
	int			i;
	int			j;

	if (element_type != ARR_ELEMTYPE(array2))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot compare arrays of different element types")));

	get_typlenbyvalalign(element_type, &typlen16, &typbyval, &typalign);
	typlen = typlen16;

	/*
	 * Since we probably will need to scan array2 multiple times, it's
	 * worthwhile to use deconstruct_array on it.  We scan array1 the hard way
	 * however, since we very likely won't need to look at all of it.
	 */
	deconstruct_array(array2, element_type, typlen, typbyval, typalign,
					  &values2, &nulls2, &nelems2);

	/* Loop over source data */
	nelems1 = ArrayGetNItems(ARR_NDIM(array1), ARR_DIMS(array1));
	ptr1 = ARR_DATA_PTR(array1);
	bitmap1 = ARR_NULLBITMAP(array1);
	bitmask = 1;

	for (i = 0; i < nelems1; i++)
	{
		Datum		elt1;
		bool		isnull1;

		/* Get element, checking for NULL */
		if (bitmap1 && (*bitmap1 & bitmask) == 0)
		{
			isnull1 = true;
			elt1 = (Datum) 0;
		}
		else
		{
			isnull1 = false;
			elt1 = fetch_att(ptr1, typbyval, typlen);
			ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
			ptr1 = (char *) att_align_nominal(ptr1, typalign);
		}

		/* advance bitmap pointer if any */
		bitmask <<= 1;
		if (bitmask == 0x100)
		{
			if (bitmap1)
				bitmap1++;
			bitmask = 1;
		}

		/*
		 * We assume that the comparison operator is strict, so a NULL can't
		 * match anything.	XXX this diverges from the "NULL=NULL" behavior of
		 * array_eq, should we act like that?
		 */
		if (isnull1)
		{
			if (matchall)
			{
				result = false;
				break;
			}
			continue;
		}

		for (j = 0; j < nelems2; j++)
		{
			Datum		elt2 = values2[j];
			bool		isnull2 = nulls2[j];
			bool		oprresult;

			if (isnull2)
				continue;		/* can't match */

			/*
			 * Apply the operator to the element pair
			 */
			if (partial)
				oprresult = DatumGetBool(DirectFunctionCall2Coll(text_equal_partial, collation, elt1, elt2));
			else
				oprresult = DatumGetBool(DirectFunctionCall2Coll(texteq, collation, elt1, elt2));
			if (oprresult)
				break;
		}

		if (j < nelems2)
		{
			/* found a match for elt1 */
			if (!matchall)
			{
				result = true;
				break;
			}
		}
		else
		{
			/* no match for elt1 */
			if (matchall)
			{
				result = false;
				break;
			}
		}
	}

	pfree(values2);
	pfree(nulls2);

	return result;
}


#if 0
Datum
json_op_text_array_contains(PG_FUNCTION_ARGS)
{
#if TRACE_LIKE_HELL
	char *pstr1, *pstr2;
	pstr1 = text_to_cstring(DatumGetTextP(OidFunctionCall2Coll(F_ARRAY_TO_TEXT, PG_GET_COLLATION(), PG_GETARG_DATUM(0), CStringGetTextDatum("#"))));
	pstr2 = text_to_cstring(DatumGetTextP(OidFunctionCall2Coll(F_ARRAY_TO_TEXT, PG_GET_COLLATION(), PG_GETARG_DATUM(1), CStringGetTextDatum("#"))));
	elog(PGJSON_TRACE_LEVEL, "GIN @@> operator: %s @@> %s", pstr1, pstr2);
	pfree(pstr1);
	pfree(pstr2);
#endif
	/* can't use directfunctioncall becase arraycontains deal with fmgrinfo */
	return OidFunctionCall2Coll(F_ARRAYCONTAINS, PG_GET_COLLATION(), PG_GETARG_DATUM(0), PG_GETARG_DATUM(1));
}

Datum
json_op_text_array_contained(PG_FUNCTION_ARGS)
{
	return DirectFunctionCall2Coll(json_op_text_array_contains, PG_GET_COLLATION(), PG_GETARG_DATUM(1), PG_GETARG_DATUM(0));
}
#else

Datum
json_op_text_array_contains_partial(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	Oid			collation = PG_GET_COLLATION();
	bool		result;

	result = text_array_contains_partial(array2, array1, collation, true, true);

	PG_FREE_IF_COPY(array1, 0);
	PG_FREE_IF_COPY(array2, 1);

	PG_RETURN_BOOL(result);
}

Datum
json_op_text_array_contained_partial(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	Oid			collation = PG_GET_COLLATION();
	bool		result;

	result = text_array_contains_partial(array1, array2, collation, true, true);

	PG_FREE_IF_COPY(array1, 0);
	PG_FREE_IF_COPY(array2, 1);

	PG_RETURN_BOOL(result);
}
#endif


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

	int32 result = DatumGetInt32(DirectFunctionCall2Coll(bttextcmp, DEFAULT_COLLATION_OID, PointerGetDatum(key1), PointerGetDatum(key2)));

#if TRACE_LIKE_HELL
	pstr1 = text_to_cstring(key1);
	pstr2 = text_to_cstring(key2);
	elog(PGJSON_TRACE_LEVEL, "GIN compare: %s vs %s -> %d", pstr1, pstr2, result);
	pfree(pstr1);
	pfree(pstr2);
#endif

	PG_RETURN_INT32(result);
}

/* (Datum itemValue, int32 *nkeys, bool **nullFlags) */
Datum
json_gin_extract_value(PG_FUNCTION_ARGS)
{
	ArrayType *itemValue = PG_GETARG_ARRAYTYPE_P_COPY(0);
	int32 *nkeys = (int32*)PG_GETARG_POINTER(1);
	bool **nullFlags = (bool**)PG_GETARG_POINTER(3);

	Datum *keys;

	text *tstr;
	char *pstr;
	int i;

	int16 elmlen;
	bool elmbyval;
	char elmalign;
	int nelems;

	get_typlenbyvalalign(ARR_ELEMTYPE(itemValue),
			&elmlen, &elmbyval, &elmalign);

	deconstruct_array(itemValue, ARR_ELEMTYPE(itemValue),
			elmlen, elmbyval, elmalign,
			&keys, nullFlags, &nelems);

	*nkeys = nelems;

/*	keys = DatumGetArrayTypeP(json_array_to_array_generic(itemValue, cJSON_String, TEXTOID, extract_json_string));
	if (keys)
		*nkeys = ARR_DIMS(keys)[0];
	else
		*nkeys = 0;*/
	
#if TRACE_LIKE_HELL
	tstr = DatumGetTextP(OidFunctionCall2Coll(F_ARRAY_TO_TEXT, PG_GET_COLLATION(), PointerGetDatum(itemValue), CStringGetTextDatum("#")));
	pstr = text_to_cstring(tstr);
	elog(PGJSON_TRACE_LEVEL, "GIN extract_value: %d items, %s", *nkeys, pstr);
	for (i = 0; i < *nkeys; ++i)
	{
		elog(PGJSON_TRACE_LEVEL, "  extract_value item %d = %s", i, text_to_cstring(DatumGetTextP(keys[i])));
	}
	pfree(pstr);
#endif

	PG_RETURN_POINTER(keys);
}

/* (Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch, Pointer **extra_data, bool **nullFlags, int32 *searchMode) */
Datum
json_gin_extract_query(PG_FUNCTION_ARGS)
{
	Datum query = PG_GETARG_DATUM(0);
	int32 *nkeys = (int32*)PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool **pmatch = (bool**)PG_GETARG_POINTER(3);
	bool **nullFlags = (bool**)PG_GETARG_POINTER(5);
	
	char *pstr;
	text *tstr;

	ArrayType *queryArray;
	Datum *keys;
	int i;

	int16 elmlen;
	bool elmbyval;
	char elmalign;
	int nelems;

	if (strategy != PGJSON_STRATEGY_CONTAINS &&
			strategy != PGJSON_STRATEGY_CONTAINS_PARTIAL)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}

	queryArray = DatumGetArrayTypePCopy(query);
	/* query is an array of texts, parse it and return */
	get_typlenbyvalalign(ARR_ELEMTYPE(queryArray),
			&elmlen, &elmbyval, &elmalign);

	deconstruct_array(queryArray, ARR_ELEMTYPE(queryArray),
			elmlen, elmbyval, elmalign,
			&keys, nullFlags, &nelems);

	*nkeys = nelems;

	if (strategy == PGJSON_STRATEGY_CONTAINS)
	{
		*pmatch = NULL;
	}
	else if (strategy == PGJSON_STRATEGY_CONTAINS_PARTIAL)
	{
		*pmatch = (bool*)palloc(sizeof(bool) * *nkeys);
		for (i = 0; i < *nkeys; ++i)
		{
			*pmatch[i] = TRUE;
		}
	}

#if TRACE_LIKE_HELL
	tstr = DatumGetTextP(OidFunctionCall2Coll(F_ARRAY_TO_TEXT, PG_GET_COLLATION(), PointerGetDatum(queryArray), CStringGetTextDatum("#")));
	pstr = text_to_cstring(tstr);
	elog(PGJSON_TRACE_LEVEL, "GIN extract_query: json=%s strategy=%d -> %d items", 
			pstr, (int)strategy, (int)*nkeys);
	for (i = 0; i < *nkeys; ++i)
	{
		elog(PGJSON_TRACE_LEVEL, "  extract_query item %d = %s", i, text_to_cstring(DatumGetTextP(keys[i])));
	}
	pfree(pstr);
#endif
	
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

	if (strategy != PGJSON_STRATEGY_CONTAINS && 
			strategy != PGJSON_STRATEGY_CONTAINS_PARTIAL)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}

	*recheck = true;
	result = true;
	for (i = 0; i < nkeys; ++i)
	{
		if (!check[i])
			result = false;
	}

#if TRACE_LIKE_HELL
	elog(PGJSON_TRACE_LEVEL, "GIN consistent: %d keys, strategy=%d -> %s", 
			(int)nkeys, (int)strategy, result?"true":"false");
#endif

	PG_RETURN_BOOL(result);
}

/* 
 * Prefix equal for two text arguments
 * Returns true iff arg2 has partial arg1 (so arg2 is larger)
 */
Datum
text_equal_partial(PG_FUNCTION_ARGS)
{
	Datum arg1 = PG_GETARG_DATUM(0);
	Datum arg2 = PG_GETARG_DATUM(1);
	bool result;
	Size len1, len2;
	text *targ1, *targ2;

	len1 = toast_raw_datum_size(arg1);
	len2 = toast_raw_datum_size(arg2);

	targ1 = DatumGetTextPP(arg1);
	targ2 = DatumGetTextPP(arg2);

	/*if (len1 == 0)
	{
		result = true;
	}
	else if (len2 == 0)
	{
		result = false;
	}
	else
	{
		cmp = memcmp(VARDATA_ANY(targ1), VARDATA_ANY(targ2), Min(len1,len2));
		result = cmp == 0 && len2 > len1;
	}*/
	result = gin_compare_string_partial(VARDATA_ANY(targ1), len1 - VARHDRSZ, VARDATA_ANY(targ2), len2 - VARHDRSZ) == 0;

	PG_FREE_IF_COPY(targ1, 0);
	PG_FREE_IF_COPY(targ2, 1);

	PG_RETURN_BOOL(result);
}

/*
 * Derived from tsearch2's tsCompareString
 * Compare two strings by tsvector rules.
 * returns zero value iff b has partial a (b is larger)
 * TODO XXX currently it's only a prefix search *
 */
int
gin_compare_string_partial(char *a, int lena, char *b, int lenb)
{
	int cmp;

	if (lena == 0)
		cmp = 0;            /* empty string is prefix of anything */
	else if (lenb == 0)
		cmp = 1;
	else
	{
		cmp = memcmp(a, b, Min(lena, lenb));
		if (cmp == 0 && lena > lenb)
			cmp = 1;        /* a is longer, so not a prefix of b */
	}

	return cmp;
}

Datum
json_gin_compare_partial(PG_FUNCTION_ARGS)
{
	text *partial_key = PG_GETARG_TEXT_P(0);
	text *key = PG_GETARG_TEXT_P(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);

	char *str_partial_key;
	char *str_key;
	int result;

	if (strategy != PGJSON_STRATEGY_CONTAINS &&
			strategy != PGJSON_STRATEGY_CONTAINS_PARTIAL)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}

	str_partial_key = text_to_cstring(partial_key); 
	str_key = text_to_cstring(key); 

	if (strategy == PGJSON_STRATEGY_CONTAINS)
	{
		if (strcmp(str_partial_key, str_key))
			result = 1;
		else
			result = 0;
	}
	else if (strategy == PGJSON_STRATEGY_CONTAINS_PARTIAL)
	{
		result = gin_compare_string_partial(str_partial_key, strlen(str_partial_key), str_key, strlen(str_key));
	}

#if TRACE_LIKE_HELL
	elog(PGJSON_TRACE_LEVEL, "GIN compare_partial: partial_key=%s key=%s -> %d", str_partial_key, str_key, result);
#endif

	pfree(str_partial_key);
	pfree(str_key);

	PG_RETURN_INT32(result);
}

/* vim: set noexpandtab tabstop=4 shiftwidth=4: */
