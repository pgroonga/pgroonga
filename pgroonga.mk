PACKAGES =
ifdef HAVE_MSGPACK
ifdef MSGPACK_PACKAGE_NAME
PACKAGES += $(MSGPACK_PACKAGE_NAME)
else
PACKAGES += msgpack
endif
endif
ifdef HAVE_XXHASH
PACKAGES += "libxxhash >= 0.8.0"
endif

MODULE_big = pgroonga
include makefiles/pgroonga-sources.mk
include makefiles/pgroonga-headers.mk
ifndef HAVE_XXHASH
SRCS += vendor/xxHash/xxhash.c
HEADERS += vendor/xxHash/xxhash.h
endif
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)
EXTENSION = pgroonga

SCRIPTS = tools/pgroonga-primary-maintainer.sh

DATA =						\
	data/pgroonga--$(PGRN_VERSION).sql	\
	$(shell echo data/pgroonga--*--*.sql)

REGRESS = $(shell find sql -name '*.sql' | sed -E -e 's,(^sql/|\.sql$$),,g')
REGRESS_OPTS = --load-extension=pgroonga

ifdef HAVE_MSGPACK
PG_CPPFLAGS += -DPGRN_HAVE_MSGPACK
endif
ifndef HAVE_XXHASH
PG_CPPFLAGS += -Ivendor/xxHash
endif

include makefiles/pgrn-pgxs.mk

all: data/pgroonga--$(PGRN_VERSION).sql
data/pgroonga--$(PGRN_VERSION).sql: data/pgroonga.sql
	@cp $< $@

RESULT_DIRS = $(shell find sql/* -type d | sed -E -e 's,^sql/,results/,')
EXPECTED_DIRS = $(shell find sql/* -type d | sed -E -e 's,^sql/,expected/,')
EXPECTED_FILES =				\
	$(shell find sql -name '*.sql' |	\
		sed -E -e 's,^sql/,expected/,'	\
		    -E -e 's,sql$$,out,')

# installcheck: prepare-regress
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
	@sed -e "s,@TMP_DIR@,$(TMP_DIR),g"			\
	  sql/compatibility/schema/vacuum/tablespace.sql.in >	\
	  sql/compatibility/schema/vacuum/tablespace.sql
	@sed -e "s,@TMP_DIR@,$(TMP_DIR),g"				\
	  expected/compatibility/schema/vacuum/tablespace.out.in >	\
	  expected/compatibility/schema/vacuum/tablespace.out

$(RESULT_DIRS) $(EXPECTED_DIRS):
	@mkdir -p $@

$(EXPECTED_FILES):
	@touch $@
