create extension "postgre-json-functions";
\t on
\pset format unaligned

-- qq
select json_object_get_text('{"foo":"qq", "bar": true}', 'foo');
-- true
select json_object_get_boolean('{"foo":"qq", "bar": true}', 'bar');
-- 42
select json_object_get_int('{"baz": 42, "boo": 42.424242}', 'baz');
-- 42
select json_object_get_bigint('{"baz": 42, "boo": 42.424242}', 'baz');
-- 9223372036854
select json_object_get_bigint('{"baz": 9223372036854, "boo": 42.424242}', 'baz');
-- 42.424242
select json_object_get_numeric('{"baz": 42, "boo": 42.424242}', 'boo');

-- Tue Dec 01 01:23:45 2009
select json_object_get_timestamp('{"foo":"qq", "bar": "2009-12-01 01:23:45"}', 'bar');
set time zone EST;
-- 2009-12-01 01:23:45
select to_char(json_object_get_timestamp('{"foo":"qq", "bar": "2009-12-01 01:23:45"}', 'bar'),
          'YYYY-MM-DD HH:MI:SSTZ');
set time zone "Europe/Moscow";
-- 2009-12-01 01:23:45
select to_char(json_object_get_timestamp('{"foo":"qq", "bar": "2009-12-01 01:23:45"}', 'bar'),
          'YYYY-MM-DD HH:MI:SSTZ');

-- {foo,bar}
select json_array_to_text_array('["foo", "bar"]');
-- {t,f}
select json_array_to_boolean_array('[true, false]');
-- {42,43}
select json_array_to_int_array('[42, 43]');
-- {42,9223372036854}
select json_array_to_bigint_array('[42, 9223372036854]');
-- {42.4242,43.4343}
select json_array_to_numeric_array('[42.4242,43.4343]');
-- {"Tue Dec 01 01:23:45 2009"}
select json_array_to_timestamp_array('["2009-12-01 01:23:45"]');


\t off
\pset format aligned
