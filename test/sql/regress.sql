set client_min_messages to 'error';
drop extension if exists "parray_gin" cascade;
create extension "parray_gin";
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

-- t
select array['foo', 'boo', 'baz'] @@> array['oo'];
-- f
select array['foo', 'boo', 'baz'] @@> array['ooz'];
-- t
select array['food', 'booze', 'baz'] @@> array['ooz'];

set client_min_messages to 'error';
drop table if exists test_table;
create table test_table(id bigserial, val text[]);
set client_min_messages to 'notice';

insert into test_table(val) values(array['foo1','bar1','baz1']);
insert into test_table(val) values(array['foo2','bar2','baz2']);
insert into test_table(val) values(array['foo3','bar3','baz3']);
insert into test_table(val) select val from test_table;
insert into test_table(val) select val from test_table;
insert into test_table(val) select val from test_table;
insert into test_table(val) values(array['foo4','bar4','baz4']);
insert into test_table(val) values(array['foo4','bar4','baz4']);
insert into test_table(val) values(array['foo4','bar4','baz4']);
insert into test_table(val) values(array['foo4','bar4one','baz4']);
insert into test_table(val) values(array['foo4','bar4two','baz4']);
insert into test_table(val) values(array['foo4','bar4three','baz4']);

-- 30
select count(*) from test_table;

-- NOTICE:  index "test_val_idx" does not exist, skipping
drop index if exists test_val_idx;
create index test_val_idx on test_table using gin (val parray_gin_ops);

-- 6
select count(*) from test_table where val @@> array['bar4'];
-- 3
select count(*) from test_table where val @> array['bar4'];
-- 8
select count(*) from test_table where val @@> array['bar3'];
-- 8
select count(*) from test_table where val @> array['bar3'];
-- 0
select count(*) from test_table where val @@> array['qux'];
-- 0
select count(*) from test_table where val @> array['qux'];
----- select count(*) from test_table where val @@> array[]::text[];
----- select count(*) from test_table where val @> array[]::text[];
-- 30
select count(*) from test_table where val @@> array[''];

\t off
\pset format aligned
