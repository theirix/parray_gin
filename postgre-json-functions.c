/*-------------------------------------------------------------------------
 *
 * Here goes a licence
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

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
	return DatumGetInt16(42);
}

Datum json_object_get_boolean(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_object_get_int(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_object_get_bigint(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_object_get_numeric(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_object_get_timestamp(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_array_to_text_array(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_array_to_boolean_array(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_array_to_int_array(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_array_to_bigint_array(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_array_to_numeric_array(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}

Datum json_array_to_timestamp_array(PG_FUNCTION_ARGS)
{
	return DatumGetInt16(42);
}


