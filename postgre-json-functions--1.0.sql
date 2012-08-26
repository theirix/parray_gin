/* postgre-json-functions--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION postgre-json-functions" to load this file. \quit

-- as normal user

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

-- opclass

create operator class json_gin_ops
for type _text using gin
as
	operator	7	@> (anyarray,anyarray),		-- text, text[] 
	function	1	json_gin_compare(internal, internal),
	function	2	json_gin_extract_value(internal, internal, internal),
	function	3	json_gin_extract_query(internal, internal, internal, internal, internal, internal, internal),
	function	4	json_gin_consistent(internal, internal, internal, internal, internal, internal, internal, internal),
	storage		text;

--	function	5	json_gin_compare_partial(internal, internal, internal, internal);
