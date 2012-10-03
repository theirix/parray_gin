parray_gin extension
====================

Installing
----------

    CREATE EXTENSION parray_gin;

Extension is compatible witgh PostgreSQL 9.1.

Description
-----------

Extension `parray_gin` provides GIN index and operator support for arrays with partial match.

Usage
-----

Extension contains operator class named `parray_gin_ops` for using GIN index with the text arrays. Matchin can be strict and partial (by substring). Surely operators can be used separately.

Index can be created for the table with the following commands:

        -- test table, column `val` needs to be indexed
        create table test_table(id bigserial, val text[]);
        -- create the index
        create index test_tags_idx on test_table using gin (val parray_gin_ops);
        -- select using index
        select * from test_table where val @> array['must','contain'];

GIN index can be used with three operators: `@>`, `<@@`, `@@>`.

Developers of an extension succesfully used GIN index on JSON arrays extracted from JSON text fields using `json_accessors` extension.

Interface
---------

### Operators

#### `@> (anyarray, anyarray) -> bool`

Strict array _contains_. Returns true if LHS array contains all items from the RHS array.

Sample index search:
        
        -- must contain all items from right side, partial matched
        select * from test_table where val @@> array['contain'];

#### `@@> (text[], text[]) -> bool`

Partial array _contains_. Returns true if LHS array contains all items from the RHS array,
matched partially (i.e. _foobar_ contains _oobar_).

Sample index search:
        
        -- must contain all items from right side, partial matched
        select * from test_table where val @@> array['cont'];

#### `<@@ (text[], text[]) -> bool`

Partial array _contained by_. Returns true if LHS array is contained by all items from the RHS array, matched partially (i.e. _foobar_ contains _oobar_). Inversion of the `@@>`.

Sample index search:
        
        -- must be contained by all items from the right side, partial matched
        select * from test_table where val @@> array['must','contains','or','not'];

### Operator class 

#### `operator class parray_gin_ops`

GIN-capable operator class. Support indexing strategies based on these operators.

Author
------

Copyright (c) 2012, Con Certeza LLC. All Right Reserved.

Developed by [Eugene Seliverstov](theirix@concerteza.ru)

Copyright and License
---------------------

You can use any code from this project under the terms of [PostgreSQL License](http://www.postgresql.org/about/licence/).

Please consult with the COPYING for license information.
