-- JSON index usage example, see
-- http://people.planetpostgresql.org/andrew/index.php?/archives/249-Using-PLV8-to-index-JSON.html
-- http://sjsnyder.com/using-postgresql-arrays-with-gin-indexes-for

-- recreate extension
-- TODO remove if a test

drop extension "postgre-json-functions" cascade;
create extension "postgre-json-functions";

-- create table
drop table if exists test_table;
create table test_table(id bigserial, val text);

-- insert data (1572867 rows)
insert into test_table(val) values('{"create_date":"2009-12-01 01:23:45","tags":["foo1","bar1","baz1"]}');
insert into test_table(val) values('{"create_date":"2009-12-02 01:23:45","tags":["foo2","bar2","baz2"]}');
insert into test_table(val) values('{"create_date":"2009-12-03 01:23:45","tags":["foo3","bar3","baz3"]}');
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
--insert into test_table(val) select val from test_table;
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4","baz4"]}');

insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4one","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4two","baz4"]}');
insert into test_table(val) values('{"create_date":"2009-12-04 01:23:45","tags":["foo4","bar4three","baz4"]}');


select count(*) from test_table;

-- JSON object fields query

-- slow query
--select * from test_table where json_object_get_timestamp(val, 'create_date') between '2009-12-04 01:00:00' and '2009-12-04 02:00:00';
-- Seq Scan on test_table  (cost=0.00..830998.51 rows=7864 width=76) (actual time=21904.068..21904.124 rows=3 loops=1)
--   Filter: ((json_object_get_timestamp(val, 'create_date'::text) >= '2009-12-04 01:00:00'::timestamp without time zone) AND (json_object_get_timestamp(val, 'create_date'::text) <= '2009-12-04 02:00:00'::timestamp without time zone))
-- Total runtime: 21904.157 ms

-- create btree index
--drop index if exists test_date_idx;
--create index test_date_idx on test_table using btree (json_object_get_timestamp(val, 'create_date'));

-- fast query
--select * from test_table where json_object_get_timestamp(val, 'create_date') between '2009-12-04 01:00:00' and '2009-12-04 02:00:00';
-- Bitmap Heap Scan on test_table  (cost=169.53..19545.85 rows=7864 width=76) (actual time=0.023..0.026 rows=3 loops=1)
--   Recheck Cond: ((json_object_get_timestamp(val, 'create_date'::text) >= '2009-12-04 01:00:00'::timestamp without time zone) AND (json_object_get_timestamp(val, 'create_date'::text) <= '2009-12-04 02:00:00'::timestamp without time zone))
--   ->  Bitmap Index Scan on test_date_idx  (cost=0.00..167.56 rows=7864 width=0) (actual time=0.015..0.015 rows=3 loops=1)
--         Index Cond: ((json_object_get_timestamp(val, 'create_date'::text) >= '2009-12-04 01:00:00'::timestamp without time zone) AND (json_object_get_timestamp(val, 'create_date'::text) <= '2009-12-04 02:00:00'::timestamp without time zone))
-- Total runtime: 0.057 ms

-- JSON array inclusion query

-- slow query
--explain analyze select * from test_table where json_object_get_text_array(val, 'tags') @> array['bar4'];
-- Seq Scan on test_table  (cost=0.00..827066.34 rows=1573 width=76) (actual time=41336.691..41336.749 rows=3 loops=1)
--   Filter: (json_array_to_text_array(json_object_get_text(val, 'tags'::text)) @> '{bar4}'::text[])
-- Total runtime: 41336.799 ms

-- create gin index, Mk I
-- http://www.postgresql.org/docs/9.0/static/gin.html
--drop index if exists test_tags_idx;
--create index test_tags_idx on test_table using gin (json_object_get_text_array(val, 'tags'));

-- fast query
--explain analyze select * from test_table where json_object_get_text_array(val, 'tags') @> array['bar4'];
-- Bitmap Heap Scan on test_table  (cost=29.08..5679.25 rows=1573 width=76) (actual time=0.050..0.055 rows=3 loops=1)
--   Recheck Cond: (json_array_to_text_array(json_object_get_text(val, 'tags'::text)) @> '{bar4}'::text[])
--   ->  Bitmap Index Scan on test_tags_idx  (cost=0.00..28.69 rows=1573 width=0) (actual time=0.037..0.037 rows=3 loops=1)
--         Index Cond: (json_array_to_text_array(json_object_get_text(val, 'tags'::text)) @> '{bar4}'::text[])
-- Total runtime: 0.129 ms

-- create gin index, Mk II
drop index if exists test_tags_idx;
create index test_tags_idx on test_table using gin (json_object_get_text_array(val, 'tags') json_gin_ops);

-- fast query
-- explain analyze select * from test_table where json_object_get_text_array(val, 'tags') @> array['bar4'];
--   expects 3 rows
explain analyze select * from test_table where json_object_get_text_array(val, 'tags') @@> array['bar4'];
select count(*) from test_table where json_object_get_text_array(val, 'tags') @@> array['bar4'];
--   expects 6 rows
