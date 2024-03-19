EXTENSION    = parray_gin
EXTVERSION   = 1.3.9
MODULE_big   = $(EXTENSION)
OBJS         = $(patsubst %.c,%.o,$(wildcard src/*.c))
DOCS         = $(wildcard doc/*.md)
TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS += --inputdir=test
PG_CONFIG    := pg_config
#PG_CPPFLAGS  = -g -O0
DATA = $(wildcard sql/*--*.sql)
EXTRA_CLEAN  = sql/$(EXTENSION)--$(EXTVERSION).sql

all: sql/$(EXTENSION)--$(EXTVERSION).sql

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@

dist:
	git archive --format zip --prefix=$(EXTENSION)-$(EXTVERSION)/ -o $(EXTENSION)-$(EXTVERSION).zip HEAD

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
