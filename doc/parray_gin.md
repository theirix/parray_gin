parray_gin extension
====================

Installing
----------

		CREATE EXTENSION parray_gin;

Extension is compatible with PostgreSQL 9.1 through 14.
Description
-----------

Extension `parray_gin` provides GIN index and operator support for arrays with 
partial match.

Usage
-----

Extension contains operator class named `parray_gin_ops` for using GIN index 
with the text arrays. Matching can be strict (array items must be equal)
or partial (array items of query may contain like expressions).
Surely operators can be used separately from the index.

Index can be created for the table with the following commands:

		-- test table, column `val` needs to be indexed
		create table test_table(id bigserial, val text[]);
		-- create the index
		create index test_tags_idx on test_table using gin (val parray_gin_ops);
		-- select using index
		select * from test_table where val @> array['must','contain'];
		-- select using index
		select * from test_table where val @@> array['what%like%'];

GIN index can be used with three operators: `@>`, `<@`, `@@>`, `<@@`.

Developers of an extension succesfully used GIN index on JSON arrays extracted 
from JSON text fields using `json_accessors` extension.

GIN index is based on trigram decomposition. Trigrams implementation from
pg_trgm contrib module is used.
Indexed keys are splitted to trigrams which are stored as GIN keys.
Query is splitted to trigrams too and carefully checked against GIN keys.
Query can contain like expressions which could slow down an index a little.
Trigram index can fetch rows with false positive so provided array matching
operators recheck fetched rows for sure.

Interface
---------

### Operators

#### `@> (text[], text[]) -> bool`

Strict array _contains_. Returns true if LHS array contains all items from 
the RHS array.

Sample index search:

		$ select * from test_table;
		{star,wars}
		{long,time,ago,in}
		{a,galaxy,far}
		{far,away}
		
		-- must contain any item from right side, strict matched
		$ select * from test_table where val @> array['far'];
		{a,galaxy,far}
		{far,away}

#### `<@ (text[], text[]) -> bool`

Strict array _contained_. Returns true if LHS array is contained by the 
RHS array.

Sample index search:

		-- must contain all items from right side, partial matched
		$ select * from test_table where val <@ array['galaxy','ago','vader'];
		{long,time,ago,in}
		{a,galaxy,far}


#### `@@> (text[], text[]) -> bool`

Partial array _contains_. Returns true if LHS array contains all items from 
the RHS array,
matched partially (i.e. `'foobar' ~~ 'foo%'` or `'foobar' ~~ '%oo%`)

Sample index search:

		-- must contain any item from right side, partially matched
		$ select * from test_table where val @@> array['%ar%'];
		{star,wars}

#### `<@@ (text[], text[]) -> bool`

Partial array _contained by_. Returns true if LHS array is contained by all 
items from the RHS array, matched partially (i.e. _foobar_ contains _oobar_). 
Inversion of the `@@>`.

Sample index search:

		-- must contain all items from right side, partially matched
		$ select * from test_table where val <@@ array['%ar%','vader'];
		{star,wars}

### Operator class 

#### `operator class parray_gin_ops`

GIN-capable operator class. Support indexing strategies based on 
these operators.

Author
------

Developed and maintaned by [Eugene Seliverstov](theirix@gmail.com)

Copyright and License
---------------------

You can use any code from this project under the terms of 
[PostgreSQL License](http://www.postgresql.org/about/licence/).

Please consult with the COPYING for license information.

<!-- vim: set noexpandtab tabstop=4 shiftwidth=4 colorcolumn=80: -->
