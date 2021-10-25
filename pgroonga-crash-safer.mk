PACKAGES =

MODULE_big = pgroonga_crash_safer
include makefiles/pgroonga-crash-safer-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)

include makefiles/pgrn-pgxs.mk
