/*-------------------------------------------------------------------------
 *
 * parray_gin.c
 *	 GIN support for arrays with partial match
 *
 * Copyright (c) 2012, Con Certeza
 * Copyright (c) 2013-2021, theirix
 * Author: theirix <theirix@gmail.com>
 *
 * GIN index heavily uses trigram implementation from pg_trgm contrib
 * module. Files trgm.c and trgm.h are copied without changes 
 * (excluded only PG_MODULE_MAGIC singleton). We thought postgresql
 * license allows this kind of code reuse.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "access/gin.h"
#include "access/skey.h"
#if PG_VERSION_NUM < 130000
#include "access/tuptoaster.h"
#endif
#include "utils/fmgroids.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/timestamp.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/formatting.h"
#include "utils/fmgroids.h"

#include "trgm.h"

PG_MODULE_MAGIC;

/* Log level, usually DEBUG5 (silent) or NOTICE (messages are sent to the
 * client side) */
#define PARRAY_GIN_TRACE NOTICE

/* Controls logging from GIN functions. A lot of output */
#define TRACE_LIKE_HELL 0

/*
 * Strategy */
/* @> operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINS 7
/* <@ operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINED_BY 8
/* @@> operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL 9
/* <@@ operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL 10

/* oids changed in postgres 14 */
#if PG_VERSION_NUM < 140000
#define OID_ARRAY_TO_TEXT_NULL F_ARRAY_TO_TEXT_NULL
#else
#define OID_ARRAY_TO_TEXT_NULL F_ARRAY_TO_STRING_ANYARRAY_TEXT_TEXT
#endif

/*
 * Internal functions declarations
 */

bool		is_valid_strategy(int strategy);
int32	   *palloc_int32(int32 value);
ArrayType  *construct_bool_array(bool *raw_array, int count);

Datum		dump_op_args(PG_FUNCTION_ARGS);
Datum		dump_array(PG_FUNCTION_ARGS);
Datum		trigrams_from_textarray(PG_FUNCTION_ARGS);

/*
 * Exported functions
 */

PGDLLEXPORT Datum parray_gin_compare(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_extract_value(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_extract_query(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_consistent(PG_FUNCTION_ARGS);

PGDLLEXPORT Datum parray_contains_strict(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_contained_strict(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_contains_partial(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_contained_partial(PG_FUNCTION_ARGS);

/*
 * Declare V1 exports
 */

PG_FUNCTION_INFO_V1(parray_gin_compare);
PG_FUNCTION_INFO_V1(parray_gin_extract_value);
PG_FUNCTION_INFO_V1(parray_gin_extract_query);
PG_FUNCTION_INFO_V1(parray_gin_consistent);

PG_FUNCTION_INFO_V1(parray_contains_strict);
PG_FUNCTION_INFO_V1(parray_contained_strict);
PG_FUNCTION_INFO_V1(parray_contains_partial);
PG_FUNCTION_INFO_V1(parray_contained_partial);

PG_FUNCTION_INFO_V1(dump_op_args);
PG_FUNCTION_INFO_V1(dump_array);
PG_FUNCTION_INFO_V1(trigrams_from_textarray);

/**
 *
 * Operator support
 *
 */

static bool
text_array_contains_partial(ArrayType *array1, ArrayType *array2,
							Oid collation, bool partial, bool switch_args)
{
	bool		matchall = true;
	bool		result = matchall;
	Oid			element_type = ARR_ELEMTYPE(array1);
	int			nelems1;
	Datum	   *values2;
	bool	   *nulls2;
	int			nelems2;
	int16		typlen16;
	int			typlen;
	bool		typbyval;
	char		typalign;
	char	   *ptr1;
	bits8	   *bitmap1;
	int			bitmask;
	int			i;
	int			j;

	if (element_type != ARR_ELEMTYPE(array2))
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("cannot compare arrays of different element types")));

	nelems1 = ArrayGetNItems(ARR_NDIM(array1), ARR_DIMS(array1));
	ptr1 = ARR_DATA_PTR(array1);
	bitmap1 = ARR_NULLBITMAP(array1);
	bitmask = 1;

	if (nelems1 == 0)
		return true;

	get_typlenbyvalalign(element_type, &typlen16, &typbyval, &typalign);
	typlen = typlen16;

	/*
	 * Since we probably will need to scan array2 multiple times, it's
	 * worthwhile to use deconstruct_array on it.	We scan array1 the hard
	 * way however, since we very likely won't need to look at all of it.
	 */
	deconstruct_array(array2, element_type, typlen, typbyval, typalign,
					  &values2, &nulls2, &nelems2);

	/* Loop over source data */

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
#if TRACE_LIKE_HELL && 0
			if (switch_args)
				elog(PARRAY_GIN_TRACE, " cmp %s vs %s",
					 TextDatumGetCString(elt2), TextDatumGetCString(elt1));
			else
				elog(PARRAY_GIN_TRACE, " cmp %s vs %s",
					 TextDatumGetCString(elt1), TextDatumGetCString(elt2));
#endif

			/*
			 * Apply the operator to the element pair
			 */
			if (partial)
			{
				if (switch_args)
					oprresult = DatumGetBool(DirectFunctionCall2Coll(textlike,
													 collation, elt2, elt1));
				else
					oprresult = DatumGetBool(DirectFunctionCall2Coll(textlike,
													 collation, elt1, elt2));
			}
			else
			{
				if (switch_args)
					oprresult = DatumGetBool(DirectFunctionCall2Coll(texteq,
													 collation, elt2, elt1));
				else
					oprresult = DatumGetBool(DirectFunctionCall2Coll(texteq,
													 collation, elt1, elt2));
			}

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

ArrayType *
construct_bool_array(bool *raw_array, int count)
{
	Oid			elmtype = BOOLOID;
	int			i;
	int16		elmlen;
	bool		elmbyval;
	char		elmalign;
	ArrayType  *array;
	Datum	   *pdatum = (Datum *) palloc(sizeof(Datum) * count);

	for (i = 0; i < count; ++i)
		pdatum[i] = BoolGetDatum(raw_array[i]);
	get_typlenbyvalalign(elmtype, &elmlen, &elmbyval, &elmalign);
	array = construct_array(pdatum, count,
							elmtype, elmlen, elmbyval, elmalign);
	pfree(pdatum);
	return array;
}

Datum
dump_array(PG_FUNCTION_ARGS)
{
	ArrayType  *array = PG_GETARG_ARRAYTYPE_P(0);
	const char *prefix = PG_GETARG_CSTRING(1);
	const char *delim = PG_GETARG_CSTRING(2);
	text	   *tstr;
	int			nelems;

	tstr = DatumGetTextP(OidFunctionCall3Coll(OID_ARRAY_TO_TEXT_NULL,
											  PG_GET_COLLATION(),
											  PointerGetDatum(array),
											  CStringGetTextDatum(delim),
											  CStringGetTextDatum("NULL")));
	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));
	elog(PARRAY_GIN_TRACE, "%s, count %d, items: %s",
		 prefix, nelems, TextDatumGetCString(tstr));

	PG_RETURN_VOID();
}

Datum
dump_op_args(PG_FUNCTION_ARGS)
{
	bool		result = PG_GETARG_BOOL(2);
	const char *prefix = PG_GETARG_CSTRING(3);
	char		buf1[1024], buf2[1024];

	snprintf(buf1, sizeof(buf1)-1, "GIN %s lhs", prefix);
	snprintf(buf2, sizeof(buf2)-1, "GIN %s rhs", prefix);
	DirectFunctionCall3Coll(dump_array, PG_GET_COLLATION(),
							PG_GETARG_DATUM(0),
							CStringGetDatum(buf1),
							CStringGetDatum("#"));
	DirectFunctionCall3Coll(dump_array, PG_GET_COLLATION(),
							PG_GETARG_DATUM(1),
							CStringGetDatum(buf2),
							CStringGetDatum("#"));
	elog(PARRAY_GIN_TRACE, "GIN %s result=%d", prefix, result);
	PG_RETURN_VOID();
}

/*
 * Underlying functions for @> operator
 */
Datum
parray_contains_strict(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	bool		result;

	result = text_array_contains_partial(array2, array1,
										 PG_GET_COLLATION(),
										 false, false);
#if TRACE_LIKE_HELL
	DirectFunctionCall4Coll(dump_op_args, PG_GET_COLLATION(),
							PointerGetDatum(array1), PointerGetDatum(array2),
							BoolGetDatum(result),
							CStringGetDatum("parray_contains_strict"));
#endif
	PG_RETURN_BOOL(result);
}

/*
 * Underlying functions for @@> operator
 */
Datum
parray_contains_partial(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	bool		result;

	result = text_array_contains_partial(array2, array1,
										 PG_GET_COLLATION(),
										 true, true);
#if TRACE_LIKE_HELL
	DirectFunctionCall4Coll(dump_op_args, PG_GET_COLLATION(),
							PointerGetDatum(array1), PointerGetDatum(array2),
							BoolGetDatum(result),
							CStringGetDatum("parray_contains_partial"));
#endif
	PG_RETURN_BOOL(result);
}

/*
 * Underlying functions for <@ operator
 */
Datum
parray_contained_strict(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	bool		result;

	result = text_array_contains_partial(array1, array2,
										 PG_GET_COLLATION(),
										 false, true);
#if TRACE_LIKE_HELL
	DirectFunctionCall4Coll(dump_op_args, PG_GET_COLLATION(),
							PointerGetDatum(array1), PointerGetDatum(array2),
							BoolGetDatum(result),
							CStringGetDatum("parray_contained_strict"));
#endif
	PG_RETURN_BOOL(result);
}

/*
 * Underlying functions for <@@ operator
 */
Datum
parray_contained_partial(PG_FUNCTION_ARGS)
{
	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	bool		result;

	result = text_array_contains_partial(array1, array2,
										 PG_GET_COLLATION(),
										 true, false);
#if TRACE_LIKE_HELL
	DirectFunctionCall4Coll(dump_op_args, PG_GET_COLLATION(),
							PointerGetDatum(array1), PointerGetDatum(array2),
							BoolGetDatum(result),
							CStringGetDatum("parray_contained_partial"));
#endif
	PG_RETURN_BOOL(result);
}


/**
 *
 * Is a strategy valid
 */
bool
is_valid_strategy(int strategy)
{
	return
		strategy == PARRAY_GIN_STRATEGY_CONTAINS ||
		strategy == PARRAY_GIN_STRATEGY_CONTAINED_BY ||
		strategy == PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL ||
		strategy == PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL;
}

int32 *
palloc_int32(int32 value)
{
	int32	   *p = palloc(sizeof(int32));

	*p = value;
	return p;
}

/*
 * TODO document
 */
Datum
trigrams_from_textarray(PG_FUNCTION_ARGS)
{
	ArrayType  *items = PG_GETARG_ARRAYTYPE_P(0);
	int32	   *countTrigrams = (int32 *) PG_GETARG_POINTER(1);
	bool		useWildcards = (bool) PG_GETARG_BOOL(2);
	Pointer   **lengthsTrigrams = (Pointer **) PG_GETARG_POINTER(3);

	/*
	 * Result type, contains int32 datums with all trigrams for all indexed
	 * strings from a value
	 */
	Datum	   *keys = NULL;

	int			indexKey;
	size_t		countArrTrigram = 0;
	size_t		i;

	Datum	   *itemKeys;
	bool	   *itemNullFlags = NULL;
	int32		countItemKeys;
	int16		elmlen;
	bool		elmbyval;
	char		elmalign;

	*countTrigrams = 0;

	get_typlenbyvalalign(ARR_ELEMTYPE(items),
						 &elmlen, &elmbyval, &elmalign);

	deconstruct_array(items, ARR_ELEMTYPE(items),
					  elmlen, elmbyval, elmalign,
					  &itemKeys, &itemNullFlags, &countItemKeys);

	/*
	 * preallocate array, it's a upper bound estimate but check for overflow
	 * later anyway
	 */
	for (indexKey = 0; indexKey < countItemKeys; ++indexKey)
		if (!itemNullFlags[indexKey])
			countArrTrigram += 2 +
				DatumGetInt32(OidFunctionCall1Coll(F_TEXTLEN,
									PG_GET_COLLATION(), itemKeys[indexKey]));
	keys = (Datum *) palloc(countArrTrigram * sizeof(TRGM));

	if (lengthsTrigrams)
	{
		*lengthsTrigrams = (Pointer *) palloc0(
									  (1 + countItemKeys) * sizeof(Pointer));
		(*lengthsTrigrams)[0] = (Pointer) palloc_int32(countItemKeys);
	}
	for (indexKey = 0; indexKey < countItemKeys; ++indexKey)
	{
		char	   *pstr;
		TRGM	   *trg;
		trgm	   *ptr;
		int32		item;

		if (!itemNullFlags[indexKey])
		{
			pstr = TextDatumGetCString(itemKeys[indexKey]);
			if (useWildcards)
				trg = generate_wildcard_trgm(pstr, strlen(pstr));
			else
				trg = generate_trgm(pstr, strlen(pstr));

			if (lengthsTrigrams)
				(*lengthsTrigrams)[indexKey + 1] =
					(Pointer) palloc_int32(ARRNELEM(trg));

			ptr = GETARR(trg);
			for (i = 0; i < ARRNELEM(trg); i++)
			{
				/* grow on need */
				if (*countTrigrams >= (int32)countArrTrigram)
				{
					countArrTrigram = (size_t) (1.4 * countArrTrigram);
					keys = repalloc(keys, countArrTrigram);
				}
				item = trgm2int(ptr++);
				keys[*countTrigrams] = Int32GetDatum(item);
				(*countTrigrams)++;
			}
		}
	}
#if TRACE_LIKE_HELL
	{
		text	   *tstr;

		tstr = DatumGetTextP(OidFunctionCall3Coll(OID_ARRAY_TO_TEXT_NULL,
												  PG_GET_COLLATION(),
												  PointerGetDatum(items),
												  CStringGetTextDatum("#"),
												  CStringGetTextDatum("NULL")
												  ));

		elog(PARRAY_GIN_TRACE,
			 "GIN trigrams_from_textarray: %d items, %s, %d trigrams",
			 countItemKeys, TextDatumGetCString(tstr), *countTrigrams);
		for (indexKey = 0; indexKey < countItemKeys; ++indexKey)
		{
			elog(PARRAY_GIN_TRACE,
				 "  trigrams_from_textarray item %d = %s (%d)", indexKey,
				 itemNullFlags[indexKey]
				 ? "NULL"
				 : (const char *) TextDatumGetCString(itemKeys[indexKey]),
				 (lengthsTrigrams
				  ? *((int32 *) (*lengthsTrigrams)[indexKey + 1])
				  : -1));
		}
		for (i = 0; i < *countTrigrams; ++i)
		{
			uint32		key = (uint32) DatumGetInt32(keys[i]);

			elog(PARRAY_GIN_TRACE,
				 "  trigrams_from_textarray item trgm = %x (%c%c%c)", key,
				 ((unsigned char *) &key)[2],
				 ((unsigned char *) &key)[1],
				 ((unsigned char *) &key)[0]
				);
		}
	}
#endif

	PG_RETURN_POINTER(keys);
}

/**
 *
 * GIN support
 *
 */

/*
 * Compare two keys
 * Strict compare
 */
Datum
parray_gin_compare(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);

	int32		result = DatumGetInt32(DirectFunctionCall2Coll(btint4cmp,
														  PG_GET_COLLATION(),
													   PointerGetDatum(key1),
													 PointerGetDatum(key2)));

#if TRACE_LIKE_HELL
/*	elog(PARRAY_GIN_TRACE, "GIN compare: %d vs %d -> %d",
		 key1, key2, result);*/
#endif

	PG_RETURN_INT32(result);
}

/*
 * Extract keys from indexed item
 * Keys are text, item is an array text[]
 * (Datum itemValue, int32 *nkeys, bool **nullFlags) */
Datum
parray_gin_extract_value(PG_FUNCTION_ARGS)
{
	ArrayType  *itemValue = PG_GETARG_ARRAYTYPE_P_COPY(0);
	int32	   *nkeys = (int32 *) PG_GETARG_POINTER(1);
	bool	  **nullFlags = (bool **) PG_GETARG_POINTER(2);

	/*
	 * Result type, contains int32 datums with all trigrams for all indexed
	 * strings from a value
	 */
	Datum	   *keys;

#if TRACE_LIKE_HELL
	elog(PARRAY_GIN_TRACE, "GIN extract_value invoked");
#endif

	keys = (Datum *) DirectFunctionCall4Coll(trigrams_from_textarray,
											 PG_GET_COLLATION(),
											 PointerGetDatum(itemValue),
											 PointerGetDatum(nkeys),
											 BoolGetDatum(false),
											 PointerGetDatum(NULL));

	*nullFlags = NULL;

	PG_RETURN_POINTER(keys);
}

/*
 * Parse query (rhs) to the keys
 * They are similar to keys extracted from an indexed item
 *	 Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch,
 *	 Pointer **extra_data, bool **nullFlags, int32 *searchMode)
 */
Datum
parray_gin_extract_query(PG_FUNCTION_ARGS)
{
	Datum		query = PG_GETARG_DATUM(0);
	int32	   *nkeys = (int32 *) PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool	  **pmatch = (bool **) PG_GETARG_POINTER(3);
	Pointer   **extra_data = (Pointer **) PG_GETARG_POINTER(4);
	bool	  **nullFlags = (bool **) PG_GETARG_POINTER(5);
	int32     *searchMode = (int32 *) PG_GETARG_POINTER(6);

	Datum	   *keys;
	bool		is_partial;

#if TRACE_LIKE_HELL
	elog(PARRAY_GIN_TRACE, "GIN extract_query invoked");
#endif

	if (!is_valid_strategy(strategy))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME),
						errmsg("wrong strategy %d", strategy)));
	}
	is_partial = strategy == PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL ||
		strategy == PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL;

	/* query is an array of texts, parse it and return trigrams */
	keys = (Datum *) DirectFunctionCall4Coll(trigrams_from_textarray,
											 PG_GET_COLLATION(),
											 query,
											 PointerGetDatum(nkeys),
											 BoolGetDatum(is_partial),
											 PointerGetDatum(extra_data));
	*nullFlags = NULL;
	*pmatch = NULL;

	/*
	 * If no trigram was extracted then we have to scan all the index.
	 */
	if (*nkeys == 0)
		*searchMode = GIN_SEARCH_MODE_ALL;

	PG_RETURN_POINTER(keys);
}

/*
 * Consistent function
 * Assume we have AND operation
 * Needs to be rethinked
 *	bool check[], StrategyNumber n, Datum query, int32 nkeys,
 *	Pointer extra_data[], bool *recheck, Datum queryKeys[],
 *	bool nullFlags[]
 */
Datum
parray_gin_consistent(PG_FUNCTION_ARGS)
{
	bool	   *check = (bool *) PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);

	/* Datum query = PG_GETARG_DATUM(2); */
	int32		nkeys = PG_GETARG_INT32(3);
	Pointer    *extra_data = (Pointer *) PG_GETARG_POINTER(4);
	bool	   *recheck = (bool *) PG_GETARG_POINTER(5);

	bool		result = false;
	int			i;

	if (!is_valid_strategy(strategy))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME),
						errmsg("wrong strategy %d", strategy)));
	}
	*recheck = true;

	if (strategy == PARRAY_GIN_STRATEGY_CONTAINS ||
		strategy == PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL)
	{
		/* all */
		result = true;
		for (i = 0; i < nkeys; ++i)
			if (!check[i])
				result = false;
	}
	else
	{
		int32	  **positions = (int32 **) extra_data;
		int			extent,
					prev = 0;

		/*
		 * Contained-by query is very suspicious because it triggers even if a
		 * single key from a query is found in a indexed item. Recheck should
		 * drop all false positive items
		 *
		 * Check each extent described by extra_data (= each query element)
		 */
		if (!extra_data)
			ereport(ERROR, (errcode(ERRCODE_INVALID_NAME),
					   errmsg("not enough data for strategy %d", strategy)));
		for (extent = 0; extent < *(positions[0]) - 1; ++extent)
		{
			result = true;
			for (i = prev; i < prev + *(positions[extent + 1]); ++i)
				if (!check[i])
					result = false;
			if (result)
				break;
			prev += *(positions[extent]);
		}
	}

#if TRACE_LIKE_HELL
	{
		ArrayType  *arrayCheck = construct_bool_array(check, nkeys);

		elog(PARRAY_GIN_TRACE, "GIN consistent: strategy=%d -> %s",
			 (int) strategy, result ? "true" : "false");
		DirectFunctionCall3Coll(dump_array, PG_GET_COLLATION(),
								PointerGetDatum(arrayCheck),
								CStringGetDatum("  check"),
								CStringGetDatum(""));
	}
#endif

	PG_RETURN_BOOL(result);
}

/* vim: set noexpandtab tabstop=4 shiftwidth=4 colorcolumn=80: */
