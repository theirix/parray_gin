/*-------------------------------------------------------------------------
 *
 * Here goes a licence
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"
#include "catalog/pg_proc.h"
#include "utils/fmgroids.h"
#include "cJSON.h"

PG_MODULE_MAGIC;


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

// TODO should read 64-bit value, cJSON doesn't support it
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

// TODO should deal with large ints
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
				// TODO should free cstring_to_text's?
				// format: yyyy-MM-dd HH:mm:ss
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

Datum json_array_to_text_array(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Not implemented yet")));
	return DatumGetInt16(42);
}

Datum json_array_to_boolean_array(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Not implemented yet")));
	return DatumGetInt16(42);
}

Datum json_array_to_int_array(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Not implemented yet")));
	return DatumGetInt16(42);
}

Datum json_array_to_bigint_array(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Not implemented yet")));
	return DatumGetInt16(42);
}

Datum json_array_to_numeric_array(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Not implemented yet")));
	return DatumGetInt16(42);
}

Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS)
{
	ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("Not implemented yet")));
	return DatumGetInt16(42);
}


