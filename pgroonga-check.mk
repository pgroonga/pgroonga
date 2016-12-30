PACKAGES =

MODULE_big = pgroonga-check
include pgroonga-check-sources.mk
OBJS = $(SRCS:.c=.o)

include pgrn-pgxs.mk

