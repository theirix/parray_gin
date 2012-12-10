set client_min_messages to 'error';
drop extension if exists "parray_gin" cascade;
create extension "parray_gin";
set client_min_messages to 'warning';

\t on
\pset format unaligned

-- t
select (array['foo', 'bar', 'baz']) @> array['foo'];
-- t
select (array['foo', 'bar', 'baz']) @> array['foo', 'bar'];
-- t
select (array['foo', 'bar', 'baz']) @> array['baz', 'foo'];
-- f
select (array['foo', 'bar', 'baz']) @> array['qux'];
-- t
select (array['foo', 'bar', 'baz']) @> array[]::text[];
-- t
select (array[]::text[]) @> array[]::text[];
-- f
select (array[]::text[]) @> array['qux'];

-- t
select (array['foo', 'bar', 'baz']) @@> array['foo'];
-- t
select (array['foo', 'bar', 'baz']) @@> array['foo', 'bar'];
-- t
select (array['foo', 'bar', 'baz']) @@> array['baz', 'foo'];
-- f
select (array['foo', 'bar', 'baz']) @@> array['qux'];
-- t
select (array['foo', 'bar', 'baz']) @@> array[]::text[];
-- t
select (array[]::text[]) @> array[]::text[];
-- f
select (array[]::text[]) @> array['qux'];

-- t
select (array['foo', 'bar', 'baz']) @@> array['fo%'];
-- t
select (array['foo', 'bar', 'baz']) @@> array['ba%'];
-- t
select (array['foo', 'bar', 'baz']) @@> array['b%'];
-- t
select (array['foo', 'bar', 'baz']) @@> array['%'];
-- f
select (array['foo', 'bar', 'baz']) @@> array['baq'];
-- t
select (array['foo', 'foobar', 'baz']) @@> array['foo%'];

-- t
select (array['foo', 'boo', 'baz']) @@> array['%oo%'];
-- t
select (array['foo', 'boo', 'baz']) @@> array['ba%', 'fo%'];
-- f
select (array['foo', 'boo', 'baz']) @@> array['%ooz%'];
-- t
select (array['food', 'booze', 'baz']) @@> array['%ooz%'];

\t off
\pset format aligned
