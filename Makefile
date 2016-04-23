REQUIRED_GROONGA_VERSION = 5.1.2
GROONGA_PKG = "groonga >= $(REQUIRED_GROONGA_VERSION)"

MODULE_big = pgroonga
SRCS =						\
	src/pgroonga.c				\
	src/pgrn_column_name.c			\
	src/pgrn_convert.c			\
	src/pgrn_create.c			\
	src/pgrn_global.c			\
	src/pgrn_groonga.c			\
	src/pgrn_jsonb.c			\
	src/pgrn_options.c			\
	src/pgrn_snippet_html.c			\
	src/pgrn_value.c			\
	src/pgrn_variables.c			\
	vendor/xxHash/xxhash.c
OBJS = $(SRCS:.c=.o)
EXTENSION = pgroonga
EXTENSION_VERSION =						\
	$(shell grep default_version $(EXTENSION).control |	\
		sed -e "s/^.*'\([0-9.]*\)'$$/\1/")
ifdef GP
DATA =						\
	pgroonga-gpdb.sql
else
DATA =						\
	pgroonga--$(EXTENSION_VERSION).sql	\
	$(shell echo pgroonga--*--*.sql)
endif

PG_CPPFLAGS = $(shell pkg-config --cflags $(GROONGA_PKG))
SHLIB_LINK = $(shell pkg-config --libs $(GROONGA_PKG)) -lm
REGRESS = $(shell find sql -name '*.sql' | sed -e 's,\(^sql/\|\.sql$$\),,g')
REGRESS_OPTS = --load-extension=pgroonga

COPT += -Ivendor/xxHash
ifdef DEBUG
COPT += -O0 -g3 -DPGROONGA_DEBUG=1
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L $(GROONGA_PKG) | sed -e 's/^-L//')
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
