# You can set UFO_DEBUG=1 or UFO_DEBUG=0 in the environment to compile with or
# without debug symbols (this affects both the C and the Rust code).

MAIN = example
SOURCES_C = src/example.c

# -----------------------------------------------------------------------------

UFO_C_PATH=src/ufo_c

# cargo will generate the output binaries for ufo-c either in a debug
# subdirectory or a release subdirectory. We remember which.
ifeq (${UFO_DEBUG}, 1)
	CARGOFLAGS=+nightly build
	UFO_C_LIB_PATH="$(UFO_C_PATH)/target/debug"
else
	CARGOFLAGS=+nightly build --release
	UFO_C_LIB_PATH="$(UFO_C_PATH)/target/release"
endif

LIBS = -Wl,--no-as-needed -lpthread -lrt -ldl -lm -lstdc++ $(UFO_C_LIB_PATH)/libufos_c.a
ifeq (${UFO_DEBUG}, 1)
	CFLAGS = -DMAKE_SURE -Og -ggdb -fPIC -Wall -Werror -DNDEBUG -I$(UFO_C_PATH)/target/
else
	CFLAGS = -DMAKE_SURE -O2       -fpic -Wall -Werror -DNDEBUG -I$(UFO_C_PATH)/target/
endif

# -----------------------------------------------------------------------------

all: $(SHLIB) $(MAIN)

OBJECTS = $(SOURCES_C:.c=.o)

OBJECTS_WITH_DEPS: ufo-c $(OBJECTS)

$(MAIN): $(OBJECTS_WITH_DEPS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJECTS) $(LFLAGS) $(LIBS) 

$(SHLIB): $(OBJECTS_WITH_DEPS)

clean: ufo-c-clean
	$(RM) src/*.o *~ $(MAIN)

.PHONY: all ufo-c ufo-c-clean clean

ufo-c:
	cargo $(CARGOFLAGS) --manifest-path=$(UFO_C_PATH)/Cargo.toml

ufo-c-clean:
	cargo clean --manifest-path=$(UFO_C_PATH)/Cargo.toml

# update-dependencies:
# 	cd src/ufo_c && git pull	