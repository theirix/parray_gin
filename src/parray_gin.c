/*-------------------------------------------------------------------------
 *
 * parray_gin.c
 *    GIN support for arrays with partial match
 *
 * Copyright (c) 2012, Con Certeza
 * Author: irix <theirix@concerteza.ru>
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

#include "trgm.h"

PG_MODULE_MAGIC;

/* Log level, usually DEBUG5 (silent) or NOTICE (messages are sent to client side) */
#define PARRAY_GIN_TRACE_LEVEL DEBUG5

/* Controls logging from GIN functions. A lot of output */
#define TRACE_LIKE_HELL 1

/* 
 * Strategy */
/* @> operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINS 7
/* @@> operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL 8
/* <@@ operator strategy */
#define PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL 9

/*
 * Internal functions declarations
 */

int32 gin_compare_string_partial(char *a, size_t lena, char *b, size_t lenb);
void* memmem_ported(const void *l, size_t l_len, const void *s, size_t s_len);


/*
 * Exported functions
 */

PGDLLEXPORT Datum parray_gin_compare(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_extract_value(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_extract_query(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_consistent(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_gin_compare_partial(PG_FUNCTION_ARGS);

PGDLLEXPORT Datum parray_op_text_array_contains_partial(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum parray_op_text_array_contained_partial(PG_FUNCTION_ARGS);

Datum text_equal_partial(PG_FUNCTION_ARGS);
Datum trigrams_from_textarray(PG_FUNCTION_ARGS);

/*
 * Declare V1 exports
 */

PG_FUNCTION_INFO_V1(parray_gin_compare);
PG_FUNCTION_INFO_V1(parray_gin_extract_value);
PG_FUNCTION_INFO_V1(parray_gin_extract_query);
PG_FUNCTION_INFO_V1(parray_gin_consistent);
PG_FUNCTION_INFO_V1(parray_gin_compare_partial);

PG_FUNCTION_INFO_V1(parray_op_text_array_contains_partial);
PG_FUNCTION_INFO_V1(parray_op_text_array_contained_partial);
PG_FUNCTION_INFO_V1(text_equal_partial);
PG_FUNCTION_INFO_V1(trigrams_from_textarray);


/**
 *
 * Operator support
 *
 */

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


/* 
 * Underlying functions for @@> and <@@ operators
 * Compare arrays for contains/contained by
 * Elements are text and compared partially (substring)
 */
Datum
parray_op_text_array_contains_partial(PG_FUNCTION_ARGS)
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

/* 
 * Paired <@@ function, see note above
 */
Datum
parray_op_text_array_contained_partial(PG_FUNCTION_ARGS)
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
	int32 key1 = PG_GETARG_INT32(0);
	int32 key2 = PG_GETARG_INT32(1);

	int32 result = DatumGetInt32(DirectFunctionCall2Coll(btint4cmp, DEFAULT_COLLATION_OID, PointerGetDatum(key1), PointerGetDatum(key2)));

#if TRACE_LIKE_HELL
	{
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN compare: %d vs %d -> %d", key1, key2, result);
	}
#endif

	PG_RETURN_INT32(result);
}

/*
 * TODO document
 */
Datum trigrams_from_textarray(PG_FUNCTION_ARGS)
{
	ArrayType *items = PG_GETARG_ARRAYTYPE_P(0);
	int32 *countTrigrams = (int32*)PG_GETARG_POINTER(1);

	/* Result type, contains int32 datums with all trigrams for all
	 * indexed strings from a value */
	Datum *keys = NULL;

	int indexKey, i;
	size_t countArrTrigram = 0;

	Datum *itemKeys;
	bool *itemNullFlags = NULL;
	int32 countItemKeys;
	int16 elmlen;
	bool elmbyval;
	char elmalign;

	*countTrigrams = 0;

	get_typlenbyvalalign(ARR_ELEMTYPE(items),
			&elmlen, &elmbyval, &elmalign);

	deconstruct_array(items, ARR_ELEMTYPE(items),
			elmlen, elmbyval, elmalign,
			&itemKeys, &itemNullFlags, &countItemKeys);

	/* preallocate array, it's a upper bound estimate but check for overflow
	 * later anyway */
	for (indexKey = 0; indexKey < countItemKeys; ++indexKey)
		countArrTrigram += 
			DatumGetInt32(OidFunctionCall1Coll(F_TEXTLEN, PG_GET_COLLATION(), itemKeys[indexKey])) + 2;
	keys = (Datum*)palloc(countArrTrigram * sizeof(TRGM));
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN extract_value preallocate %ld items", countArrTrigram);

	for (indexKey = 0; indexKey < countItemKeys; ++indexKey)
	{
		char *pstr;
		TRGM *trg;
		trgm *ptr;
		int32 item;
		if (!itemNullFlags[indexKey])
		{
			pstr = text_to_cstring(DatumGetTextP(itemKeys[indexKey]));
			trg = generate_trgm( pstr, strlen( pstr ) );
			ptr = GETARR(trg);
			for (i = 0; i < ARRNELEM(trg); i++)
			{
				/* grow on need */
				if (*countTrigrams >= countArrTrigram)
				{
					countArrTrigram = (size_t)(1.4*countArrTrigram);
					keys = repalloc( keys, countArrTrigram );
				}
				item = trgm2int(ptr++);
				keys[*countTrigrams] = Int32GetDatum(item);
				(*countTrigrams)++;
			}
		}
	}
#if TRACE_LIKE_HELL
	{
	text *tstr;
	char *pstr;
	int i;

	tstr = DatumGetTextP(OidFunctionCall2Coll(F_ARRAY_TO_TEXT, PG_GET_COLLATION(), PointerGetDatum(items), CStringGetTextDatum("#")));
	pstr = text_to_cstring(tstr);
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN trigrams_from_textarray: %d items, %s, %d trigrams", countItemKeys, pstr, *countTrigrams);
	pfree(pstr);
	for (i = 0; i < countItemKeys; ++i)
	{
		pstr = text_to_cstring(DatumGetTextP(itemKeys[i]));
		elog(PARRAY_GIN_TRACE_LEVEL, "  trigrams_from_textarray item %d = %s", i, pstr);
		pfree(pstr);
	}
	for (i = 0; i < *countTrigrams; ++i)
	{
		elog(PARRAY_GIN_TRACE_LEVEL, "  trigrams_from_textarray item tgrm = %x", DatumGetInt32(keys[i]));
	}
	}
#endif

	PG_RETURN_POINTER(keys);
}

/* 
 * Extract keys from indexed item
 * Keys are text, item is an array text[]
 * (Datum itemValue, int32 *nkeys, bool **nullFlags) */
Datum
parray_gin_extract_value(PG_FUNCTION_ARGS)
{
	ArrayType *itemValue = PG_GETARG_ARRAYTYPE_P_COPY(0);
	int32 *nkeys = (int32*)PG_GETARG_POINTER(1);
	bool **nullFlags = (bool**)PG_GETARG_POINTER(2);

	/* Result type, contains int32 datums with all trigrams for all
	 * indexed strings from a value */
	Datum *keys;

#if TRACE_LIKE_HELL
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN extract_value invoked");
#endif

	keys = (Datum*)DirectFunctionCall2Coll(trigrams_from_textarray, PG_GET_COLLATION(),
				PointerGetDatum(itemValue), PointerGetDatum(nkeys));
	
	*nullFlags = NULL;

	PG_RETURN_POINTER(keys);
}

/* 
 * Parse query (rhs) to the keys
 * They are similar to keys extracted from an indexed item
 * (Datum query, int32 *nkeys, StrategyNumber n, bool **pmatch, Pointer **extra_data, bool **nullFlags, int32 *searchMode) */
Datum
parray_gin_extract_query(PG_FUNCTION_ARGS)
{
	Datum query = PG_GETARG_DATUM(0);
	int32 *nkeys = (int32*)PG_GETARG_POINTER(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);
	bool **pmatch = (bool**)PG_GETARG_POINTER(3);
	bool **nullFlags = (bool**)PG_GETARG_POINTER(5);
	/* int32 *searchMode = (int32*)PG_GETARG_POINTER(6);*/

	Datum *keys;
	int i;

#if TRACE_LIKE_HELL
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN extract_query invoked");
#endif


	if (strategy != PARRAY_GIN_STRATEGY_CONTAINS &&
			strategy != PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL &&
			strategy != PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}

	/* query is an array of texts, parse it and return trigrams */
	keys = (Datum*)DirectFunctionCall2Coll(trigrams_from_textarray, PG_GET_COLLATION(),
				query, PointerGetDatum(nkeys));
	*nullFlags = NULL;

	/* don't bother about empty queries
	 * but be careful - it will lead to an undefined result
	 * searchMode = GIN_SEARCH_MODE_ALL;*/

	if (strategy == PARRAY_GIN_STRATEGY_CONTAINS)
	{
		*pmatch = NULL;
	}
	else
	{
		*pmatch = (bool*)palloc(sizeof(bool) * *nkeys);
		for (i = 0; i < *nkeys; ++i)
		{
			(*pmatch)[i] = TRUE;
		}
	}
	
	PG_RETURN_POINTER(keys);
}

/* 
 * Consistent function
 * Assume we have AND operation
 * Needs to be rethinked
 * bool check[], StrategyNumber n, Datum query, int32 nkeys,
		Pointer extra_data[], bool *recheck, Datum queryKeys[], bool nullFlags[]*/
Datum
parray_gin_consistent(PG_FUNCTION_ARGS)
{
	bool *check = (bool*)PG_GETARG_POINTER(0);
	StrategyNumber strategy = PG_GETARG_UINT16(1);
	/*Datum query = PG_GETARG_DATUM(2);*/
	int32 nkeys = PG_GETARG_INT32(3);
	bool *recheck = (bool*)PG_GETARG_POINTER(5);

	bool result = false;
	int i;

	if (strategy != PARRAY_GIN_STRATEGY_CONTAINS && 
			strategy != PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL &&
			strategy != PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL)
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
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN consistent: %d keys, strategy=%d -> %s", 
			nkeys, (int)strategy, result?"true":"false");
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

	result = gin_compare_string_partial(VARDATA_ANY(targ1), len1 - VARHDRSZ, VARDATA_ANY(targ2), len2 - VARHDRSZ) == 0;

#if TRACE_LIKE_HELL
	{
	char *pstr1, *pstr2;
	pstr1 = text_to_cstring(targ1);
	pstr2 = text_to_cstring(targ2);
	elog(PARRAY_GIN_TRACE_LEVEL, "text_equal_partial: %s vs %s -> %d", pstr1, pstr2, result);
	pfree(pstr1);
	pfree(pstr2);
	}
#endif

	PG_FREE_IF_COPY(targ1, 0);
	PG_FREE_IF_COPY(targ2, 1);

	PG_RETURN_BOOL(result);
}


/*
 * memmem ported from FreeBSD 9
 * Find the first occurrence of the byte string s in byte string l.
 */
void *
memmem_ported(const void *l, size_t l_len, const void *s, size_t s_len)
{
	register char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}

/*
 * Derived from tsearch2's tsCompareString
 * Compare two strings by tsvector rules.
 * returns zero value iff b has partial a (b is larger)
 */
int
gin_compare_string_partial(char *a, size_t lena, char *b, size_t lenb)
{
	int cmp;

	if (lena == 0)
		cmp = 0;            /* empty string is substring of anything */
	else if (lenb == 0)
		cmp = 1;
	else
		cmp = memmem_ported(b, lenb, a, lena) ? 0 : 1;

	return cmp;
}

/*
 * Compare keys partially
 * Returns a value that indicates a status of scan (see doc)
 */
Datum
parray_gin_compare_partial(PG_FUNCTION_ARGS)
{
	text *partial_key = PG_GETARG_TEXT_P(0);
	text *key = PG_GETARG_TEXT_P(1);
	StrategyNumber strategy = PG_GETARG_UINT16(2);

	char *str_partial_key;
	char *str_key;
	int result;

	if (strategy != PARRAY_GIN_STRATEGY_CONTAINS &&
			strategy != PARRAY_GIN_STRATEGY_CONTAINS_PARTIAL &&
			strategy != PARRAY_GIN_STRATEGY_CONTAINED_BY_PARTIAL)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_NAME), errmsg("wrong strategy %d", strategy)));
	}

	str_partial_key = text_to_cstring(partial_key); 
	str_key = text_to_cstring(key); 

	if (strategy == PARRAY_GIN_STRATEGY_CONTAINS)
	{
		if (strcmp(str_partial_key, str_key))
			result = 1;
		else
			result = 0;
	}
	else 
	{
		result = gin_compare_string_partial(str_partial_key, strlen(str_partial_key), str_key, strlen(str_key));
	}

#if TRACE_LIKE_HELL
	elog(PARRAY_GIN_TRACE_LEVEL, "GIN compare_partial: partial_key=%s key=%s -> %d", str_partial_key, str_key, result);
#endif

	pfree(str_partial_key);
	pfree(str_key);

	PG_RETURN_INT32(result);
}

/* vim: set noexpandtab tabstop=4 shiftwidth=4: */
