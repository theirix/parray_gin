/* json_accessors_c--1.2.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "create extension json_accessors_c" to load this file. \quit

-- must be run as a normal user

-- Scalar functions

-- JSON builder
create or replace function json_get_object(text, text) returns text
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_get_text(text, text) returns text
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_get_boolean(text, text) returns boolean
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_get_int(text, text) returns int
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_bigint(text, text) returns bigint
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_numeric(text, text) returns numeric
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_get_timestamp(text, text) returns timestamp
 as 'MODULE_PATHNAME' language C immutable strict;

-- Array functions

create or replace function json_array_to_object_array(text) returns text[]
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_array_to_multi_array(text) returns text[]
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

-- Indirect array functions

create or replace function json_get_object_array(text, text) returns text[]
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_get_multi_array(text, text) returns text[]
 as 'MODULE_PATHNAME' language C immutable strict;

create or replace function json_get_text_array(text, text) returns text[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_boolean_array(text, text) returns boolean[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_int_array(text, text) returns int[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_bigint_array(text, text) returns bigint[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_numeric_array(text, text) returns numeric[]
 as 'MODULE_PATHNAME' language C immutable strict; 

create or replace function json_get_timestamp_array(text, text) returns timestamp without time zone[]
 as 'MODULE_PATHNAME' language C immutable strict; 
