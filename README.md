GIN partial array match for PostgreSQL
======================================

Extension `parray_gin` provides GIN index and operator support for arrays with partial match.

Extension supports PostgreSQL 9.1 through 15.

Usage
-----

Please consult with [docs](doc/parray_gin.md) for a function and operator reference.

On PGXN please click on extension from _Extensions_ section to view reference.

Installing extension
--------------------

To use an extension one must be built, installed into PostgreSQL directory
and registered in a database.

### Building extension

#### Using PGXN network

The easisest method to get and install an extension from PGXN network.
PGXN client downloads and builds the extension.

		pgxn --pg_config <postgresql_install_dir>/bin/pg_config install parray_gin

PGXN client itself is available at [github](https://github.com/dvarrazzo/pgxnclient) and
can be installed with your favourite method, i.e. `easy_install pgxnclient`.

#### Using PGXS makefiles

C extension are best built and installed using [PGXS](https://www.postgresql.org/docs/current/extend-pgxs.html).
PGXS ensures that make is performed with needed compiler and flags. You only need GNU make and a compiler to build
an extension on an almost any UNIX platform (Linux, Solaris, OS X).

Compilation:

		gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config

Installation (as superuser):

		gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config install

PostgreSQL server must be restarted. 

To uninstall extension completely you may use this command (as superuser):

		gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config uninstall

Project contains SQL tests that can be launched on PostgreSQL with installed extension.
Tests are performed on a dynamically created database with a specified user (with the 
appropriated permissions - create database, for example):

		gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config install
		gmake PG_CONFIG=<postgresql_install_dir>/bin/pg_config PGUSER=postgres installcheck

Sometimes it's needed to add platform-specific flags for building. For example,
to build with ICU provided by homebrew you should add to `Makefile`:

		PG_CPPFLAGS = "-I/usr/local/opt/icu4c/include"
		SHLIB_LINK =  "-L/usr/local/opt/icu4c/lib"

#### Manually

Use this method if you have a precompiled extension and do not want to install this with help of PGXS.
Or maybe you just do not have GNU make on a production server.
Or if you use Windows (use MSVC 2008 for Postgres 9.1 and MSVC 2010 for Postgres 9.2).

Copy library to the PostgreSQL library directory:

		cp parray_gin.so `<postgresql_install_dir>/bin/pg_config --pkglibdir` 

Copy control file to the extension directory:
		
		cp parray_gin.control `<postgresql_install_dir>/bin/pg_config --sharedir`/extension

Copy SQL prototypes file to the extension directory:
		
		cp parray_gin--<version>.sql `<postgresql_install_dir>/bin/pg_config --sharedir`/extension

To uninstall extension just remove files you copied before.

### Creating extension in a database

Extension must be previously installed to a PostgreSQL directory.

Extension is created in a particular database (as superuser):

		create extension parray_gin;

It creates all the functions, operators and other stuff from extension.
Note that you must restart a server if a previous library was already installed
at the same place. In other words, always restart to be sure. 

To drop an extension use:

		drop extension parray_gin cascade;


License information
-------------------

You can use any code from this project under the terms of [PostgreSQL License](http://www.postgresql.org/about/licence/).

Please consult with the COPYING for license information.
<!-- vim: set noexpandtab tabstop=4 shiftwidth=4 colorcolumn=80: -->
