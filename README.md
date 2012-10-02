PostgreSQL GIN support for arrays with partial match
====================================================

[PostgreSQL](http://www.postgresql.org/) native extension providing GIN index support for arrays with partial match.

This project contains PostgreSQL [extension](http://www.postgresql.org/docs/9.1/static/extend-extensions.html) `parray_gin` with stored functions. Extension is native and writen in C.

Usage
-----

Please consult with [doc/parray_gin.md](doc/parray_gin.md) for a function reference.


Installing extension
--------------------

### Building and installing extension with PGXS

C extension are best built and installed using [PGXS](http://www.postgresql.org/docs/9.1/static/extend-pgxs.html).
PGXS ensures that make is performed with needed compiler and flags. You only need GNU make and a compiler to build
an extension on an almost any UNIX platform (Linux, Solaris, OS X).

Compilation:

    gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config

Installation (as superuser):

    gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config install

PostgreSQL server must be restarted and extension created in particular database as superuser:

    create extension parray_gin

To drop all functions use:

    drop extension parray_gin cascade

To uninstall extension completely you may use this command (as superuser):

    gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config uninstall

Project contains SQL tests that can be launched on PostgreSQL with installed extension.
Tests are performed on a dynamically created database with a specified user (with the 
appropriated permissions - create database, for example):

    gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config PGUSER=postgres installcheck


### Installing manually

Use this method if you have a precompiled extension and do not want to install this with help of PGXS.
Or maybe you just do not have GNU make on a production server.

Copy library to the PostgreSQL library directory:

    cp parray_gin.so `<postgresql_install_dir>/bin/pg_config --pkglibdir` 

Copy control file to the extension directory:
    
    cp parray_gin.control `<postgresql_install_dir>/bin/pg_config --sharedir`/extension

Copy SQL prototypes file to the extension directory:
    
    cp parray_gin--1.3.sql `<postgresql_install_dir>/bin/pg_config --sharedir`/extension

Create an extension by running:

    create extension parray_gin

It creates all the functions and operators. Note that you must restart a server if a previous library was
already installed at the same place.

To drop all functions use:

    drop extension parray_gin cascade

To uninstall extension just remove files you copied before.


License information
-------------------

You can use any code from this project under the terms of [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0).
