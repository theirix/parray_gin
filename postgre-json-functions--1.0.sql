/* postgre-json-functions--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION \"postgre-json-functions\"" to load this file. \quit

-- must be run as a normal user

-- Scalar functions

create or replace function json_object_get_text(text, text) returns text
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_object_get_boolean(text, text) returns boolean
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_object_get_int(text, text) returns int
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_bigint(text, text) returns bigint
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_numeric(text, text) returns numeric
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_object_get_timestamp(text, text) returns timestamp
 as 'MODULE_PATHNAME' language C immutable strict;

-- Array functions

create or replace function json_array_to_text_array(text) returns text[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_array_to_boolean_array(text) returns boolean[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_array_to_int_array(text) returns int[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_array_to_bigint_array(text) returns bigint[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_array_to_numeric_array(text) returns numeric[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_array_to_timestamp_array(text) returns timestamp without time zone[]
 as 'MODULE_PATHNAME' language C immutable strict; 

-- Indirect array functions

create or replace function json_object_get_text_array(text, text) returns text[] 
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_boolean_array(text, text) returns boolean[] 
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_int_array(text, text) returns int[] 
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_bigint_array(text, text) returns bigint[] 
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_numeric_array(text, text) returns numeric[] 
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_object_get_timestamp_array(text, text) returns timestamp without time zone[]
 as 'MODULE_PATHNAME' language C immutable strict; 


-- GIN support

create or replace function json_gin_compare(internal, internal) returns internal
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_gin_extract_value(internal, internal, internal) returns internal
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_gin_extract_query(internal, internal, internal, internal, internal, internal, internal) returns internal
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_gin_consistent(internal, internal, internal, internal, internal, internal, internal, internal) returns internal
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_gin_compare_partial(internal, internal, internal, internal) returns internal
 as 'MODULE_PATHNAME' language C immutable strict;

-- GIN operator class

-- partial operators <@@ and @@>
--   text[] op text[]
--   json_object_get_text_array(val, 'tags') @> array['foo'] - strict contains
--   json_object_get_text_array(val, 'tags') @@> array['ba'] - partial contains
--   json_object_get_text_array(val, 'tags') @< array['all', 'your', 'base'] - NOT USED strict contained by

create or replace function json_op_text_array_contains_partial(_text, _text) returns bool
 as 'MODULE_PATHNAME' language C immutable strict;

comment on function json_op_text_array_contains_partial(_text,_text) is 'text array contains compared by partial';

create or replace function json_op_text_array_contained_partial(_text, _text) returns bool
 as 'MODULE_PATHNAME' language C immutable strict;

comment on function json_op_text_array_contained_partial(_text,_text) is 'text array contained compared by partial';

-- partial contains
create operator @@> (
  leftarg = _text,
  rightarg = _text,
  procedure = json_op_text_array_contains_partial,
  commutator = '<@@',
  restrict = contsel,
  join = contjoinsel
);

-- partial contained by
create operator <@@ (
  leftarg = _text,
  rightarg = _text,
  procedure = json_op_text_array_contained_partial,
  commutator = '@@>',
  restrict = contsel,
  join = contjoinsel
);

-- operator class

create operator class json_gin_ops
for type _text using gin
as
	operator	7	@> (anyarray,anyarray),			-- strict text[], text[] 
	operator	8	@@> (_text,_text),		-- partial text[], text[] 
	--operator	9	<@@ (_text,_text),	-- partial text[], text[] 
	function	1	json_gin_compare(internal, internal),
	function	2	json_gin_extract_value(internal, internal, internal),
	function	3	json_gin_extract_query(internal, internal, internal, internal, internal, internal, internal),
	function	4	json_gin_consistent(internal, internal, internal, internal, internal, internal, internal, internal),
	function	5	json_gin_compare_partial(internal, internal, internal, internal),
	storage		text;

