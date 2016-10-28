REQUIRED_GROONGA_VERSION = 6.0.7
GROONGA_PKG = "groonga >= $(REQUIRED_GROONGA_VERSION)"
PACKAGES = $(GROONGA_PKG)
ifdef HAVE_MSGPACK
PACKAGES += msgpack
endif

MODULE_big = pgroonga
include sources.am
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

PG_CPPFLAGS = $(shell pkg-config --cflags $(PACKAGES))
SHLIB_LINK = $(shell pkg-config --libs $(PACKAGES)) -lm
REGRESS = $(shell find sql -name '*.sql' | sed -e 's,\(^sql/\|\.sql$$\),,g')
REGRESS_OPTS = --load-extension=pgroonga

COPT += -Ivendor/xxHash
ifdef HAVE_MSGPACK
COPT += -DPGRN_HAVE_MSGPACK
endif
ifdef DEBUG
COPT += -O0 -g3 -DPGROONGA_DEBUG=1
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L $(PACKAGES) | sed -e 's/^-L//')
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

installcheck: prepare-regress
installcheck: $(RESULT_DIRS)
installcheck: $(EXPECTED_DIRS)
installcheck: $(EXPECTED_FILES)

TMP_DIR = $(shell pwd)/tmp
SETUP_TMP_DIR = yes
prepare-regress:
	@if [ $(SETUP_TMP_DIR) = "yes" ]; then	\
	  rm -rf $(TMP_DIR)/space &&		\
	  mkdir -p $(TMP_DIR)/space;		\
	fi
	@sed -e "s,@TMP_DIR@,$(TMP_DIR),g"	\
	  sql/vacuum/tablespace.sql.in >	\
	  sql/vacuum/tablespace.sql
	@sed -e "s,@TMP_DIR@,$(TMP_DIR),g"	\
	  expected/vacuum/tablespace.out.in >	\
	  expected/vacuum/tablespace.out

$(RESULT_DIRS) $(EXPECTED_DIRS):
	@mkdir -p $@

$(EXPECTED_FILES):
	@touch $@
