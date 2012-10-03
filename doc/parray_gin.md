parray_gin
==============

Installing
----------

    CREATE EXTENSION parray_gin;

Extension is compatible witgh PostgreSQL 9.1 and 9.2.

Description
-----------

[PostgreSQL](http://www.postgresql.org/) native extension providing GIN index support for arrays with partial match.

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

GIN index can be used with three following operators

1. Strict match _contains_ `@>` operator:

        -- must contain all items from right side, partial matched
        select * from test_table where val @@> array['contain'];


2. Partial match _contains_ `@@>` operator:

        -- must contain all items from right side, partial matched
        select * from test_table where val @@> array['cont'];


3. Partial match _contained by_ `<@@` operator:

        -- must be contained by all items from the right side, partial matched
        select * from test_table where val @@> array['must','contains','or','not'];



Interface
---------

### Operator `@> (anyarray, anyarray) -> bool`

Strict array match. Returns true if LHS array contains all items from the RHS array.

### Operator `@@> (text[], text[]) -> bool`

Partial array match. Returns true if LHS array contains all items from the RHS array,
matched partially (i.e. _foobar_ contains _oobar_).

### Operator `<@@ (text[], text[]) -> bool`

Partial array match. Returns true if LHS array is contained by all items from the RHS array, matched partially (i.e. _foobar_ contains _oobar_). Inversion of the `@@>`

### Operator class `parray_gin_ops`

GIN-capable operator class. Support indexing based on theses operators.

Author
------

Copyright (c) 2012, Con Certeza LLC. All Right Reserved.

Developed by [Eugene Seliverstov](theirix@concerteza.ru)

Copyright and License
---------------------

You can use any code from this project under the terms of [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0).
