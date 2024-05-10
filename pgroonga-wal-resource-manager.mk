PACKAGES =

MODULE_big = pgroonga_wal_resource_manager
include makefiles/pgroonga-wal-resource-manager-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)

include makefiles/pgrn-pgxs.mk
