##############################################################################
# CCRetro Makefile – works with normal copy/paste, no tab issues
#
# Usage:
#   make                 # print help
#   make arch=modern     # build modern binaries (OpenMP+1t)
#   make arch=dos        # build DJGPP-targeted DOS binaries
#   make clean           # delete all built files
#   make distclean       # full clean
#
# - Everything works with spaces, thanks to .RECIPEPREFIX
# - To use src/ dir for code, set SRC_DIR=src on make command line/rule
##############################################################################
.RECIPEPREFIX = >

# ------------ config --------------------------------------------------------
SRC_DIR    ?= .
BUILD_DIR  ?= build
STATIC     ?= 0             # STATIC=1 for static on Linux modern builds
STRIP_BIN  ?= 1             # STRIP_BIN=1 to strip after build

MT_SRC     := $(SRC_DIR)/ccretro.c
ST_SRC     := $(SRC_DIR)/ccretro-1t.c

# ------------ Modern (native build) -----------------------------------------
HOST_CC        ?= gcc
HOST_CPPFLAGS  :=
HOST_CFLAGS    := -Ofast -march=native -ffast-math -fomit-frame-pointer -funroll-loops -std=c11 -Wall -Wextra -DNDEBUG
HOST_LDFLAGS   :=
HOST_LIBS      := -lgmp -fopenmp
PKG_CONFIG     ?= pkg-config

HOST_CPPFLAGS += $(shell $(PKG_CONFIG) --cflags gmp 2>/dev/null)
HOST_LIBS     := $(shell $(PKG_CONFIG) --libs gmp 2>/dev/null || echo -lgmp) -fopenmp

ifeq ($(STATIC),1)
  ifeq ($(shell uname -s),Linux)
    HOST_LDFLAGS += -static
  endif
endif

STRIP ?= strip
ifeq ($(STRIP_BIN),1)
  STRIP_CMD = $(STRIP) -s
else
  STRIP_CMD = true
endif

MODERN_DIR    := $(BUILD_DIR)/modern
MODERN_MT_BIN := $(MODERN_DIR)/ccretro
MODERN_ST_BIN := $(MODERN_DIR)/ccretro-1t

# ------------ DOS (DJGPP cross-compile) -------------------------------------
DOS_CC       ?= $(CC)
ifeq ($(strip $(DOS_CC)),)
  DOS_CC := i586-pc-msdosdjgpp-gcc
endif
DOS_CPPFLAGS ?=
DOS_CFLAGS   ?= -Ofast -ffast-math -fomit-frame-pointer -std=c11 -Wall -Wextra -DNDEBUG
DOS_LDFLAGS  ?= -static
ifeq ($(STRIP_BIN),1)
  DOS_LDFLAGS += -s
endif
DOS_LIBS     ?= -lgmp
DOS_DIR      := $(BUILD_DIR)/dos

DOS_CPUS := cc386 cc486 cci686 ccp2 ccp3 ccp4 ccp4p \
            cck6 cck62 cck63 cck7tb ccaxp cca64 cci7nh

MARCH_cc386  := i386         ; EXTRA_cc386  :=
MARCH_cc486  := i486         ; EXTRA_cc486  :=
MARCH_cci686 := i686         ; EXTRA_cci686 :=
MARCH_ccp2   := pentium2     ; EXTRA_ccp2   :=
MARCH_ccp3   := pentium3     ; EXTRA_ccp3   :=
MARCH_ccp4   := pentium4     ; EXTRA_ccp4   := -msse2
MARCH_ccp4p  := prescott     ; EXTRA_ccp4p  := -msse3
MARCH_cck6   := k6           ; EXTRA_cck6   :=
MARCH_cck62  := k6-2         ; EXTRA_cck62  :=
MARCH_cck63  := k6-3         ; EXTRA_cck63  :=
MARCH_cck7tb := athlon-tbird ; EXTRA_cck7tb :=
MARCH_ccaxp  := athlon-xp    ; EXTRA_ccaxp  := -m3dnow
MARCH_cca64  := athlon64     ; EXTRA_cca64  :=
MARCH_cci7nh := nehalem      ; EXTRA_cci7nh := -msse4.2

DOS_BINS := $(addprefix $(DOS_DIR)/,$(addsuffix .exe,$(DOS_CPUS)))

# ------------ Phony targets -------------------------------------------------
.PHONY: all help modern dos clean distclean

# Default target logic:
# - 'make' with no arguments will run the 'else' and show help.
# - 'make arch=modern' will run the 'ifeq' and build 'modern'.
# - 'make clean' will skip this rule entirely and run 'clean' directly.
all:
> @if [ -z "$(arch)" ]; then \
    $(MAKE) help; \
  else \
    $(MAKE) $(arch); \
  fi

# Print help
help:
> @echo ""
> @echo "ccretro make targets"
> @echo "--------------------"
> @echo "make arch=modern     Build modern binaries:"
> @echo "                     - $(MODERN_MT_BIN)   (OpenMP, GMP)"
> @echo "                     - $(MODERN_ST_BIN)   (single-thread, GMP)"
> @echo "make arch=dos        Build DOS binaries with DJGPP cross-compiler in $(DOS_DIR):"
> @echo "                     $(DOS_CPUS)"
> @echo ""
> @echo "Variables you may use:"
> @echo "  STATIC=1           Static linking for modern builds (Linux)"
> @echo "  STRIP_BIN=0        Don't strip binaries"
> @echo "  SRC_DIR=src        Use if you move .c files to src/"
> @echo "  DOS_CC=...         Path to DJGPP cross GCC"
> @echo "  DOS_CPPFLAGS/DOS_CFLAGS/DOS_LDFLAGS...   See Makefile"
> @echo ""
> @echo "make clean | distclean"

# Modern build
modern: $(MODERN_MT_BIN) $(MODERN_ST_BIN)
> @echo "===> Modern build complete."

$(MODERN_DIR):
> mkdir -p $@

$(MODERN_MT_BIN): $(MT_SRC) | $(MODERN_DIR)
> printf "Building ccretro (SMP): "
> if $(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -fopenmp $(HOST_LDFLAGS) -o $@ $< $(HOST_LIBS) >/dev/null 2>&1; then \
      $(STRIP_CMD) $@ >/dev/null 2>&1; \
      echo "Done!"; \
  else \
      echo "FAILED"; exit 1; \
  fi

$(MODERN_ST_BIN): $(ST_SRC) | $(MODERN_DIR)
> printf "Building ccretro (single thread): "
> if $(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(HOST_LDFLAGS) -o $@ $< $(HOST_LIBS) >/dev/null 2>&1; then \
      $(STRIP_CMD) $@ >/dev/null 2>&1; \
      echo "Done!"; \
  else \
      echo "FAILED"; exit 1; \
  fi

# DOS build
dos: $(DOS_BINS)
> @echo "===> DOS build complete."

$(DOS_DIR):
> mkdir -p $@

$(DOS_DIR)/%.exe: $(ST_SRC) | $(DOS_DIR)
> echo "[DOS] $@ march=$(MARCH_$*) $(EXTRA_$*)"
> $(DOS_CC) $(DOS_CPPFLAGS) $(DOS_CFLAGS) -march=$(MARCH_$*) $(EXTRA_$*) \
          $(DOS_LDFLAGS) -o $@ $< $(DOS_LIBS)

# Clean
clean:
> rm -rf $(BUILD_DIR)
distclean:
> rm -rf $(BUILD_DIR)
