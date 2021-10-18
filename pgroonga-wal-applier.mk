PACKAGES =

MODULE_big = pgroonga_wal_applier
include makefiles/pgroonga-wal-applier-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)

include makefiles/pgrn-pgxs.mk
