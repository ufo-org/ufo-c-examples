# You can set UFO_DEBUG=1 or UFO_DEBUG=0 in the environment to compile with or
# without debug symbols (this affects both the C and the Rust code).

SOURCES_C = src/postgres.c src/bzip.c src/fib.c src/timing.c src/bench.c src/seq.c

# -----------------------------------------------------------------------------

UFO_C_PATH=src/ufo_c

# cargo will generate the output binaries for ufo-c either in a debug
# subdirectory or a release subdirectory. We remember which.
ifeq (${UFO_DEBUG}, 1)
	CARGOFLAGS=+nightly build
	UFO_C_LIB_PATH=$(UFO_C_PATH)/target/debug
else
	CARGOFLAGS=+nightly build --release
	UFO_C_LIB_PATH=$(UFO_C_PATH)/target/release
endif

LIBS = -Wl,--no-as-needed -lpthread -lpq -lrt -ldl -lm -lbz2 -lstdc++ $(UFO_C_LIB_PATH)/libufo_c.a 
ifeq (${UFO_DEBUG}, 1)
	CFLAGS = -DMAKE_SURE -Og -ggdb -fPIC -Wall -Werror -DNDEBUG -I$(UFO_C_PATH)/target/ -I/usr/include/postgresql
else
	CFLAGS =             -O2       -fPIC -Wall -Werror          -I$(UFO_C_PATH)/target/ -I/usr/include/postgresql
endif

# TODO split CFLAGS for different wotsits

# -----------------------------------------------------------------------------

.PHONY: all ufo-c ufo-c-clean clean

all: libs postgres bzip fib seq bench

OBJECTS = $(SOURCES_C:.c=.o)

libs: ufo-c $(OBJECTS)

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

bench: libs
	$(CC) $(CFLAGS) $(INCLUDES) -o bench $(OBJECTS) $(LFLAGS) $(LIBS) 

clean: ufo-c-clean
	$(RM) src/*.o *~ $(MAIN)

ufo-c:
	cargo $(CARGOFLAGS) --manifest-path=$(UFO_C_PATH)/Cargo.toml

ufo-c-clean:
	cargo clean --manifest-path=$(UFO_C_PATH)/Cargo.toml

# update-dependencies:
# 	cd src/ufo_c && git pull	