# You can set UFO_DEBUG=1 or UFO_DEBUG=0 in the environment to compile with or
# without debug symbols (this affects both the C and the Rust code).

SOURCES_C = src/postgres.c src/bzip.c src/fib.c src/timing.c src/bench.c src/seq.c src/random.c src/mmap.c src/ufo.c src/nyc.c src/normil.c src/toronto.c
SOURCES_CPP = src/nycpp.cpp

# -----------------------------------------------------------------------------

UFO_C_PATH=src/ufo_c
NEW_YORK_PATH=src/new_york
TORONTO_PATH=src/toronto

# cargo will generate the output binaries for ufo-c either in a debug
# subdirectory or a release subdirectory. We remember which.
ifeq (${UFO_DEBUG}, 1)
	CARGOFLAGS=+nightly build
	LIB_SUBDIR=/target/debug
else
	CARGOFLAGS=+nightly build --release
	LIB_SUBDIR=/target/release
endif

OUR_LIBS=$(UFO_C_PATH)/$(LIB_SUBDIR)/libufo_c.a $(NEW_YORK_PATH)/$(LIB_SUBDIR)/libnew_york_city.a  $(TORONTO_PATH)/$(LIB_SUBDIR)/libtoronto.a
OUR_INCLUDES=-I$(UFO_C_PATH)/target/ -I$(NEW_YORK_PATH)/target/ -I$(TORONTO_PATH)/target/

LIBS = -Wl,--no-as-needed -lpthread -lpq -lrt -ldl -lm -lbz2 -lstdc++ $(OUR_LIBS)
ifeq (${UFO_DEBUG}, 1)
	CFLAGS = -DMAKE_SURE -Og -ggdb -fPIC -Wall -Werror -DNDEBUG $(OUR_INCLUDES) -I/usr/include/postgresql
	CXXFLAGS =           -Og -ggdb -fPIC -Wall -Werror -DNDEBUG $(OUR_INCLUDES) -I/usr/include/postgresql
else
	CFLAGS =             -O2       -fPIC -Wall -Werror          $(OUR_INCLUDES) -I/usr/include/postgresql
	CXXFLAGS =           -O2       -fPIC -Wall -Werror          $(OUR_INCLUDES) -I/usr/include/postgresql
endif

# TODO split CFLAGS for different wotsits

# -----------------------------------------------------------------------------

.PHONY: all ufo-c ufo-c-clean clean new-york new-york-clean toronto toronto-clean

all: libs postgres bzip fib seq bench

OBJECTS = $(SOURCES_C:.c=.o)
OBJECTS_CPP = $(SOURCES_CPP:.cpp=.o)

libs: ufo-c new-york toronto $(OBJECTS) $(OBJECTS_CPP)

example: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o example $(LFLAGS) $(LIBS) src/example.c

postgres: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o postgres src/postgres.o $(LFLAGS) $(LIBS) src/postgres_example.c

bzip: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o bzip src/bzip.o $(LFLAGS) $(LIBS) src/bzip_example.c

fib: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o fib src/fib.o $(LFLAGS) $(LIBS) src/fib_example.c

seq: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o seq src/seq.o $(LFLAGS) $(LIBS) src/seq_example.c

# mmap: libs
# 	$(CC) $(CFLAGS) $(INCLUDES) -o mmap src/mmap.o $(LFLAGS) $(LIBS) src/mmap_example.c

bench: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o bench $(OBJECTS) $(OBJECTS_CPP) $(LFLAGS) $(LIBS) 

clean: ufo-c-clean new-york-clean
	$(RM) src/*.o *~ $(MAIN) bench seq fib bzip postgres 

ufo-c:
	cargo $(CARGOFLAGS) --manifest-path=$(UFO_C_PATH)/Cargo.toml

ufo-c-clean:
	cargo clean --manifest-path=$(UFO_C_PATH)/Cargo.toml

new-york:
	cargo $(CARGOFLAGS)	--manifest-path=$(NEW_YORK_PATH)/Cargo.toml

new-york-clean:
	cargo clean --manifest-path=$(NEW_YORK_PATH)/Cargo.toml	

toronto:
	cargo $(CARGOFLAGS)	--manifest-path=$(TORONTO_PATH)/Cargo.toml

toronto-clean:
	cargo clean	--manifest-path=$(TORONTO_PATH)/Cargo.toml

# update-dependencies:
# 	cd src/ufo_c && git pull	