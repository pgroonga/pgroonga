MODULE_big = pgroonga
SRCS =						\
	pgroonga.c				\
	vendor/xxHash/xxhash.c
OBJS = $(SRCS:.c=.o)
EXTENSION = pgroonga
EXTENSION_VERSION =						\
	$(shell grep default_version $(EXTENSION).control |	\
		sed -e "s/^.*'\([0-9.]*\)'$$/\1/")
DATA = pgroonga--$(EXTENSION_VERSION).sql
PG_CPPFLAGS = $(shell pkg-config --cflags groonga)
SHLIB_LINK = $(shell pkg-config --libs groonga) -lm
REGRESS = $(shell find sql -name '*.sql' | sed -e 's,\(^sql/\|\.sql$$\),,g')
REGRESS_OPTS = --load-extension=pgroonga

COPT += -Ivendor/xxHash
ifdef DEBUG
COPT += -O0 -g3 -DPGROONGA_DEBUG=1
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L groonga | sed -e 's/^-L//')
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: pgroonga--$(EXTENSION_VERSION).sql
pgroonga--$(EXTENSION_VERSION).sql: pgroonga.sql
	@cp $< $@

RESULT_DIRS = $(shell find sql/* -type d | sed -e 's,^sql/,results/,')
EXPECTED_DIRS = $(shell find sql/* -type d | sed -e 's,^sql/,expected/,')
EXPECTED_FILES =				\
	$(shell find sql -name '*.sql' |	\
		sed -e 's,^sql/,expected/,'	\
		    -e 's,sql$$,out,')

installcheck: $(RESULT_DIRS) $(EXPECTED_DIRS) $(EXPECTED_FILES)

$(RESULT_DIRS) $(EXPECTED_DIRS):
	@mkdir -p $@

$(EXPECTED_FILES):
	@touch $@
