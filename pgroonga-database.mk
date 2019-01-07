PACKAGES =

MODULE_big = pgroonga_database
include makefiles/pgroonga-database-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)
EXTENSION = pgroonga_database

DATA =							\
	data/pgroonga_database--$(PGRN_VERSION).sql
#	$(shell echo data/pgroonga_database--*--*.sql)

include makefiles/pgrn-pgxs.mk

all: data/pgroonga_database--$(PGRN_VERSION).sql
data/pgroonga_database--$(PGRN_VERSION).sql: data/pgroonga_database.sql
	@cp $< $@
