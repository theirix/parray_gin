/*-------------------------------------------------------------------------
 *
 * Here goes a licence
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "utils/fmgroids.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "cJSON.h"

PG_MODULE_MAGIC;

ArrayType* construct_typed_array(Datum *elems, int nelems, Oid elmtype);

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
 * Base functions *
 */

Datum json_object_get_text(PG_FUNCTION_ARGS)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	text *argKey = PG_GETARG_TEXT_P(1);
	bool status = false;
	text *result;
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
			if (sel->type == cJSON_String)
			{
				result = cstring_to_text(sel->valuestring);
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
				 errmsg("json string is not a string")));

	PG_RETURN_TEXT_P(result);
}

Datum json_object_get_boolean(PG_FUNCTION_ARGS)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	text *argKey = PG_GETARG_TEXT_P(1);
	bool status = false;
	int result;
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
			if (sel->type == cJSON_True)
			{
				result = 1;
				status = true;
			}
			else if (sel->type == cJSON_False)
			{
				result = 0;
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
				 errmsg("json string is not a bool")));

	PG_RETURN_BOOL(result);
}

Datum json_object_get_int(PG_FUNCTION_ARGS)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	text *argKey = PG_GETARG_TEXT_P(1);
	bool status = false;
	int result;
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
			if (sel->type == cJSON_Number)
			{
				result = sel->valueint;
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
				 errmsg("json string is not an integer")));

	PG_RETURN_INT32(result);
}

/* TODO should read 64-bit value, cJSON doesn't support it */
Datum json_object_get_bigint(PG_FUNCTION_ARGS)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	text *argKey = PG_GETARG_TEXT_P(1);
	bool status = false;
	int64 result;
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
			if (sel->type == cJSON_Number)
			{
				result = sel->valueint;
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
				 errmsg("json string is not an integer")));

	PG_RETURN_INT64(result);
}

/* TODO should deal with large ints
 * json large int is specified without quotes and so interpeted by cJSON
 * as a double not as a string
 * Needs a manual parsing and then translating to a numeric from a string 
 */
Datum json_object_get_numeric(PG_FUNCTION_ARGS)
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
			if (sel->type == cJSON_Number)
			{
				result = DirectFunctionCall1(float4_numeric,
						Float4GetDatum(sel->valuedouble));
				/*result = OidFunctionCall2(F_NUMERIC_TO_NUMBER,
						PointerGetDatum(cstring_to_text(sel->valuestring)),
						PointerGetDatum(cstring_to_text("99999999999999999999999999999999999999.99999999999999999999999999999999999999")));*/
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
				 errmsg("json string is not an integer")));

	PG_RETURN_DATUM(result);
}

Datum json_object_get_timestamp(PG_FUNCTION_ARGS)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	text *argKey = PG_GETARG_TEXT_P(1);
	bool status = false;
	Datum result;
	char *strJson, *strKey;
	cJSON *root, *sel;
	Datum timestampWithTz;

	strJson = text_to_cstring(argJson);
	strKey = text_to_cstring(argKey);
	
	root = cJSON_Parse(strJson);
	if (root)
	{
		sel = cJSON_GetObjectItem(root, strKey);
		if (sel)
		{
			if (sel->type == cJSON_String)
			{
				/* TODO should free cstring_to_text's?
						format: yyyy-MM-dd HH:mm:ss
				*/
				timestampWithTz = OidFunctionCall2(F_TO_TIMESTAMP,
						PointerGetDatum(cstring_to_text(sel->valuestring)),
						PointerGetDatum(cstring_to_text("YYYY-MM-DD HH:MI:SS")));
				Assert(timestampWithTz);
				result = DirectFunctionCall1(timestamptz_timestamp,
						timestampWithTz);
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
				 errmsg("json string is not a timestamp")));

	PG_RETURN_DATUM(result);
}

/**
 * Array functions *
 */


ArrayType* construct_typed_array(Datum *elems, int nelems, Oid elmtype)
{
	int16   elmlen;
	bool    elmbyval;
	char    elmalign;	
	get_typlenbyvalalign(elmtype, &elmlen, &elmbyval, &elmalign);
	return construct_array(elems, nelems, elmtype, elmlen, elmbyval, elmalign);
}

Datum json_array_to_text_array(PG_FUNCTION_ARGS)
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
			if (elem->type != cJSON_String)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected boolean value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				items[ind] = CStringGetTextDatum(elem->valuestring);
			}
			array = construct_typed_array(items, count, TEXTOID);

			for (ind = 0; ind < count; ++ind)
				pfree(DatumGetPointer(items[ind]));
			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
			
	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with texts")));

	PG_RETURN_ARRAYTYPE_P(array);
}

Datum json_array_to_boolean_array(PG_FUNCTION_ARGS)
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
			if (elem->type != cJSON_True && elem->type != cJSON_False)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected boolean value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				items[ind] = BoolGetDatum(elem->type == cJSON_True);
			}
			array = construct_typed_array(items, count, BOOLOID);

			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
			
	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with texts")));

	PG_RETURN_ARRAYTYPE_P(array);
}

Datum json_array_to_int_array(PG_FUNCTION_ARGS)
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
			if (elem->type != cJSON_Number)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected int value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				items[ind] = Int32GetDatum(elem->valueint);
			}
			array = construct_typed_array(items, count, INT4OID);

			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
			
	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with texts")));

	PG_RETURN_ARRAYTYPE_P(array);
}

Datum json_array_to_bigint_array(PG_FUNCTION_ARGS)
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
			if (elem->type != cJSON_Number)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected int value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				items[ind] = Int64GetDatum(elem->valueint);
			}
			array = construct_typed_array(items, count, INT8OID);

			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
			
	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with texts")));

	PG_RETURN_ARRAYTYPE_P(array);
}

Datum json_array_to_numeric_array(PG_FUNCTION_ARGS)
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
			if (elem->type != cJSON_Number)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected int value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				items[ind] = DirectFunctionCall1(float4_numeric,
						Float4GetDatum(elem->valuedouble));
			}
			array = construct_typed_array(items, count, NUMERICOID);

			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
			
	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with texts")));

	PG_RETURN_ARRAYTYPE_P(array);
}

Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS)
{
	text *argJson = PG_GETARG_TEXT_P(0);
	Datum *items = NULL;
	ArrayType *array = NULL;
	char *strJson;
	cJSON *root, *elem;
	int count = 0, ind;
	Datum timestampWithTz;

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
			if (elem->type != cJSON_String)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("expected int value at %d position", ind)));
			++count;
		}

		if (count)
		{
			items = (Datum*)palloc(count * sizeof(Datum));

			for (elem = root->child, ind = 0; elem; elem = elem->next, ++ind)
			{
				timestampWithTz = OidFunctionCall2(F_TO_TIMESTAMP,
						PointerGetDatum(cstring_to_text(elem->valuestring)),
						PointerGetDatum(cstring_to_text("YYYY-MM-DD HH:MI:SS")));
				Assert(timestampWithTz);
				items[ind] = DirectFunctionCall1(timestamptz_timestamp,
						timestampWithTz);
			}
			array = construct_typed_array(items, count, TIMESTAMPOID);

			pfree(items);
		}
		cJSON_Delete(root);
	}

	pfree(strJson);
			
	if (!array)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("json string is not an array with texts")));

	PG_RETURN_ARRAYTYPE_P(array);
}


