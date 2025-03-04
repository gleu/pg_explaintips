# pg_explaintips/Makefile

MODULE_big = pg_explaintips
OBJS = $(WIN32RES) pg_explaintips.o

PGFILEDESC = "pg_explaintips - add tips to EXPLAIN"

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
