set client_min_messages to 'error';
drop extension if exists "parray_gin" cascade;
create extension "parray_gin";
set client_min_messages to 'warning';

\t on
\pset format unaligned

set enable_seqscan to off;

set client_min_messages to 'error';
drop table if exists test_table;
create table test_table(id bigserial, val text[]);
set client_min_messages to 'warning';

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
insert into test_table(val) values(array['foo4','bar4four','baz4']);

-- 31
select count(*) from test_table;

drop index if exists test_val_idx;
create index test_val_idx on test_table using gin (val parray_gin_ops);

-- 8
select count(*) from test_table where val @> array['foo1','bar1','baz1'];
-- 8
select count(*) from test_table where val @@> array['foo1','bar1','baz1'];

-- 7
select count(*) from test_table where val @@> array['bar4%'];
-- 3
select count(*) from test_table where val @> array['bar4'];
-- 8
select count(*) from test_table where val @@> array['bar3%'];
-- 8
select count(*) from test_table where val @> array['bar3'];
-- 0
select count(*) from test_table where val @@> array['qux%'];
-- 0
select count(*) from test_table where val @@> array['qux%'];
-- 0
select count(*) from test_table where val @> array['%bar4%o%'];
-- 3
select count(*) from test_table where val @@> array['%bar4%o%'];
---- todo fix
---- select count(*) from test_table where val @@> array[]::text[];
---- select count(*) from test_table where val @> array[]::text[];
---- todo fix
-- 0
select count(*) from test_table where val @> array['%'];
-- 0
select count(*) from test_table where val @@> array['%'];

set enable_seqscan to on;

\t off
\pset format aligned
