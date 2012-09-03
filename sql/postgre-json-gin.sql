set client_min_messages to 'error';
drop extension if exists "postgre-json-functions" cascade;
create extension "postgre-json-functions";
set client_min_messages to 'notice';

\t on
\pset format unaligned

-- t
select array['foo', 'bar', 'baz'] @> array['foo'];
-- t
select array['foo', 'bar', 'baz'] @> array['foo', 'bar'];
-- t
select array['foo', 'bar', 'baz'] @> array['baz', 'foo'];
-- f
select array['foo', 'bar', 'baz'] @> array['qux'];
-- t
select array['foo', 'bar', 'baz'] @> array[]::text[];
-- t
select array[]::text[] @> array[]::text[];
-- f
select array[]::text[] @> array['qux'];

-- t
select array['foo', 'bar', 'baz'] @@> array['foo'];
-- t
select array['foo', 'bar', 'baz'] @@> array['foo', 'bar'];
-- t
select array['foo', 'bar', 'baz'] @@> array['baz', 'foo'];
-- f
select array['foo', 'bar', 'baz'] @@> array['qux'];
-- t
select array['foo', 'bar', 'baz'] @@> array[]::text[];
-- t
select array[]::text[] @> array[]::text[];
-- f
select array[]::text[] @> array['qux'];

-- t
select array['foo', 'bar', 'baz'] @@> array['fo'];
-- t
select array['foo', 'bar', 'baz'] @@> array['ba'];
-- t
select array['foo', 'bar', 'baz'] @@> array['b'];
-- t
select array['foo', 'bar', 'baz'] @@> array[''];
-- f
select array['foo', 'bar', 'baz'] @@> array['baq'];
-- t
select array['foo', 'foobar', 'baz'] @@> array['foo'];

set client_min_messages to 'error';
drop table if exists test_table;
create table test_table(id bigserial, val text);
set client_min_messages to 'notice';

insert into test_table(val) values('{"create_date":"2009-12-01 01:23:45","tags":["foo1","bar1","baz1"]}');
insert into test_table(val) values('{"create_date":"2009-12-02 01:23:45","tags":["foo2","bar2","baz2"]}');
insert into test_table(val) values('{"create_date":"2009-12-03 01:23:45","tags":["foo3","bar3","baz3"]}');
insert into test_table(val) select val from test_table;
insert into test_table(val) select val from test_table;
insert into test_table(val) select val from test_table;
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4one","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4two","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4three","baz4"]}');

-- 30
select count(*) from test_table;

-- NOTICE:  index "test_tags_idx" does not exist, skipping
drop index if exists test_tags_idx;
create index test_tags_idx on test_table using gin (json_object_get_text_array(val, 'tags') json_gin_ops);

-- 6
select count(*) from test_table where json_object_get_text_array(val, 'tags') @@> array['bar4'];
-- 3
select count(*) from test_table where json_object_get_text_array(val, 'tags') @> array['bar4'];
-- 8
select count(*) from test_table where json_object_get_text_array(val, 'tags') @@> array['bar3'];
-- 8
select count(*) from test_table where json_object_get_text_array(val, 'tags') @> array['bar3'];
-- 0
select count(*) from test_table where json_object_get_text_array(val, 'tags') @@> array['qux'];
-- 0
select count(*) from test_table where json_object_get_text_array(val, 'tags') @> array['qux'];
----- select count(*) from test_table where json_object_get_text_array(val, 'tags') @@> array[]::text[];
----- select count(*) from test_table where json_object_get_text_array(val, 'tags') @> array[]::text[];
-- 30
select count(*) from test_table where json_object_get_text_array(val, 'tags') @@> array[''];

\t off
\pset format aligned
