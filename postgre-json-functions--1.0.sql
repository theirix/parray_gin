/* postgre-json-functions--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION postgre-json-functions" to load this file. \quit

-- as normal user

create or replace function json_object_get_text(text, text) returns text
 as 'MODULE_PATHNAME', 'json_object_get_text' language C immutable strict;

create or replace function json_object_get_boolean(text, text) returns boolean
 as 'MODULE_PATHNAME', 'json_object_get_boolean' language C immutable strict;

create or replace function json_object_get_int(text, text) returns int
 as 'MODULE_PATHNAME', 'json_object_get_int' language C immutable strict; 

create or replace function json_object_get_bigint(text, text) returns bigint
 as 'MODULE_PATHNAME', 'json_object_get_bigint' language C immutable strict; 

create or replace function json_object_get_numeric(text, text) returns numeric
 as 'MODULE_PATHNAME', 'json_object_get_numeric' language C immutable strict;

create or replace function json_object_get_timestamp(text, text) returns timestamp
 as 'MODULE_PATHNAME', 'json_object_get_timestamp' language C immutable strict;

create or replace function json_array_to_text_array(text) returns text[]
 as 'MODULE_PATHNAME', 'json_array_to_text_array' language C immutable strict; 

create or replace function json_array_to_boolean_array(text) returns boolean[]
 as 'MODULE_PATHNAME', 'json_array_to_boolean_array' language C immutable strict; 

create or replace function json_array_to_int_array(text) returns int[]
 as 'MODULE_PATHNAME', 'json_array_to_int_array' language C immutable strict; 

create or replace function json_array_to_bigint_array(text) returns bigint[]
 as 'MODULE_PATHNAME', 'json_array_to_bigint_array' language C immutable strict; 

create or replace function json_array_to_numeric_array(text) returns numeric[]
 as 'MODULE_PATHNAME', 'json_array_to_numeric_array' language C immutable strict; 

create or replace function json_array_to_timestamp_array(text) returns timestamp without time zone[]
 as 'MODULE_PATHNAME', 'json_array_to_timestamp_array' language C immutable strict; 

-- GIN support

create operator class json_gin_ops
for type _text using gin
as
	operator	7			@>, -- text, text[]
	function	1			json_gin_compare,
	function	2			json_gin_extract_value,
	function	3			json_gin_extract_query,
	function	4			json_gin_consistent,
	--function	5			json_gin_compare_partial,
	storage		text;
