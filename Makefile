ifeq ($(origin CXX), environment)
  override CXX := em++
endif
ifeq ($(origin CC), environment)
  override CC := emcc
endif

web:   CXX := em++
web:   CC  := emcc
web:   all

linux: CXX := clang++
linux: CC  := clang
linux: all

CXX ?= em++
CC  ?= emcc

ROOT_BUILD_DIR ?= _Intermediate
ROOT_OUTPUT_DIR ?= _Builds

# Platform selection. make evaluates `ifeq` once, while reading this file, so it
# cannot see the `web:` target's CXX (a target-specific variable). Decide from
# the goal the user asked for instead; when neither `web` nor `linux` is a goal,
# infer from CXX -- the Flatpak build does `make all CXX=g++`. Native is default.
.DEFAULT_GOAL := linux
ifneq ($(filter web,$(MAKECMDGOALS)),)
  BUILD_PLATFORM := web
else ifneq ($(filter linux,$(MAKECMDGOALS)),)
  BUILD_PLATFORM := linux
else ifneq ($(findstring em++,$(notdir $(CXX))),)
  BUILD_PLATFORM := web
else
  BUILD_PLATFORM := linux
endif

ifeq ($(BUILD_PLATFORM),web)
	BUILD_DIR ?= $(ROOT_BUILD_DIR)/web
	OUTPUT_DIR ?= $(ROOT_OUTPUT_DIR)/web
	OUT ?= index
	EXT ?= .html
else
	BUILD_DIR ?= $(ROOT_BUILD_DIR)/linux
	OUTPUT_DIR ?= $(ROOT_OUTPUT_DIR)/linux
	OUT ?= secondreality
	EXT ?= 
endif

SRC_DIR   ?= .
TARGET    := $(OUTPUT_DIR)/$(OUT)$(EXT)

CPPFLAGS += -I$(SRC_DIR)

CXXFLAGS  ?= -O3 -Wall -Wextra -Wno-missing-braces -Wno-unused-function
CFLAGS    ?= -O3 -Wall -Wextra -Wno-missing-braces -Wno-unused-function

# Required flags, appended unconditionally: build systems such as
# flatpak-builder export their own CXXFLAGS, which would make the ?= defaults
# above be skipped and lose the C++20 standard (and -pthread) the code needs.
CXXFLAGS  += -std=c++20 -pthread

# Emit a .d next to every .o so that editing a header recompiles the objects
# that include it. Without this a signature change in a header only relinks
# stale objects, which fails with undefined references.
DEPFLAGS  := -MMD -MP
CXXFLAGS  += $(DEPFLAGS)
CFLAGS    += $(DEPFLAGS)

# Link-Time Optimization: the linker sees every translation unit at once, so it
# can inline across files and drop unused code. Must be on both compile and link.
# Native only -- Emscripten's -flto drops the EM_JS `is_firefox` import (undefined
# symbol at link, `index.wasm.lto.o: undefined symbol: is_firefox`), so the web
# build stays without it.
ifneq ($(BUILD_PLATFORM),web)
  LTO_FLAGS := -flto
  CXXFLAGS  += $(LTO_FLAGS)
  CFLAGS    += $(LTO_FLAGS)
  LDFLAGS   += $(LTO_FLAGS)
endif

ifeq ($(BUILD_PLATFORM),web)
  EM_COMPILE_FLAGS := -sUSE_SDL=2
  EM_LINK_FLAGS    := -sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 -sWASM=1 -sASYNCIFY -sEXPORTED_RUNTIME_METHODS=callMain

  CXXFLAGS += $(EM_COMPILE_FLAGS)
  CFLAGS   += $(EM_COMPILE_FLAGS)
  LDFLAGS  += $(EM_LINK_FLAGS)
else
  CXXFLAGS += $(shell sdl2-config --cflags)
  LDLIBS   += $(shell sdl2-config --libs) -lGLESv2
endif

MKDIR_LINE = mkdir -p "$(@D)"
RM_RF      = rm -rf
WHICH     := which
NULLDEV   := /dev/null

rwildcard = $(wildcard $1$2) $(foreach d,$(wildcard $1*/),$(call rwildcard,$d,$2))

CPP_EXTS := cpp cc cxx CPP C++
C_EXTS   := c

CPP_SOURCES := $(foreach e,$(CPP_EXTS),$(call rwildcard,$(SRC_DIR)/,*.$(e)))
C_SOURCES   := $(foreach e,$(C_EXTS),  $(call rwildcard,$(SRC_DIR)/,*.$(e)))

EXCLUDE_DIRS ?= References validated
EXCLUDE_GLOBS := $(foreach d,$(EXCLUDE_DIRS),$(SRC_DIR)/$(d)/%)

CPP_SOURCES   := $(filter-out $(EXCLUDE_GLOBS),$(CPP_SOURCES))
C_SOURCES     := $(filter-out $(EXCLUDE_GLOBS),$(C_SOURCES))

# Translation units regenerated from the bitmaps by the embed rules below. Listed
# explicitly because a source tree that does not commit them (the public mirror)
# has none of them when the source list is first computed.
GENERATED_SOURCES := \
    $(SRC_DIR)/Parts/ALKU/AlkuHzpicData.cpp \
    $(SRC_DIR)/Parts/TECHNO/TrollData.cpp \
    $(SRC_DIR)/Parts/LENS/LensFaceBgData.cpp \
    $(SRC_DIR)/Parts/LENS/LensFaceTileData.cpp \
    $(SRC_DIR)/Parts/CREDITS/CreditsPicsData.cpp
CPP_SOURCES   := $(sort $(CPP_SOURCES) $(GENERATED_SOURCES))

CPP_OBJECTS := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(CPP_SOURCES))
C_OBJECTS   := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(C_SOURCES))

# Normalize extensions to .o
OBJECTS := $(CPP_OBJECTS:.cpp=.o)
OBJECTS := $(OBJECTS:.cc=.o)
OBJECTS := $(OBJECTS:.cxx=.o)
OBJECTS := $(OBJECTS:.CPP=.o)
OBJECTS := $(OBJECTS:.C++=.o)
OBJECTS := $(OBJECTS:.c=.o)
OBJECTS := $(sort $(OBJECTS))

.PHONY: all clean tree verify-toolchain print-% help embed flatpak

all: verify-toolchain $(TARGET)

help:
	@echo Targets:
	@echo   make tree : show sources/objects/target
	@echo   make clean : clean-up
	@echo   make linux : linux platform
	@echo   emmake make web : web platform
	@echo   make embed : re-embed bitmaps into C++ - normally automatic
	@echo   "make flatpak : build a single-file .flatpak bundle (flatpak/)"
	@echo .
	@echo if you get that [fatal error: SDL2/SDL.h: No such file or directory], run command below:
	@echo   Ubuntu/Debian: sudo apt install libsdl2-dev
	@echo   Fedora: sudo dnf install SDL2-devel
	@echo   Arch: sudo pacman -S sdl2
	@echo   openSUSE: sudo zypper install libSDL2-devel
	@echo   Alpine: sudo apk add sdl2-dev

tree:
	@echo "Sources (C++):"
	@$(foreach s,$(CPP_SOURCES),echo "  $(s)";)
	@echo "Sources (C):"
	@$(foreach s,$(C_SOURCES),echo "  $(s)";)
	@echo "Objects:"
	@$(foreach o,$(OBJECTS),echo "  $(o)";)
	@echo "Target: $(TARGET)"

print-%:
	@echo '$* = $($*)'

# ---------------------------------------------------------------------------
# Embedded assets
#
# The demo ships no runtime data files: bitmaps are compiled into the binary as
# C++ byte arrays. Each generated translation unit below is a real build output
# of its source bitmap, so editing a PNG re-embeds it and recompiles the
# dependent object automatically -- there is no manual regeneration step.
#
# Override EMBED_PY to select a different Python 3 interpreter.
# ---------------------------------------------------------------------------
EMBED_PY   ?= python3
EMBED_TOOL := $(SRC_DIR)/tools/embed_png.py
EMBED_NS   := Lens::Data

EMBED_BG_SRC   := $(SRC_DIR)/tools/extracted_bitmaps/lens_hq.png
EMBED_BG_CPP   := $(SRC_DIR)/Parts/LENS/LensFaceBgData.cpp
EMBED_BG_SYM   := face_bg_png
EMBED_BG_HDR   := LensFaceBgData.h

EMBED_TILE_SRC := $(SRC_DIR)/tools/extracted_bitmaps/lens_tile_hq.png
EMBED_TILE_CPP := $(SRC_DIR)/Parts/LENS/LensFaceTileData.cpp
EMBED_TILE_SYM := face_tile_png
EMBED_TILE_HDR := LensFaceTileData.h

EMBED_ASSETS   := $(EMBED_BG_CPP) $(EMBED_TILE_CPP)
EMBED_ALKU_SRC := $(SRC_DIR)/tools/extracted_bitmaps/alku_hq.png
EMBED_ALKU_CPP := $(SRC_DIR)/Parts/ALKU/AlkuHzpicData.cpp
EMBED_ALKU_SYM := hzpic_bg_png
EMBED_ALKU_HDR := AlkuHzpicData.h
EMBED_ALKU_NS  := Alku::Data

EMBED_ASSETS   := $(EMBED_ASSETS) $(EMBED_ALKU_CPP)

# The troll was a 320x400 256-colour bitmap, rasterised on the CPU, so the 4x
# upscale could never show. This is the same artwork at 2560x3200, which is 8x
# the logical size and still above what the screen actually shows.
EMBED_TROLL_SRC := $(SRC_DIR)/tools/extracted_bitmaps/troll_hq.png
EMBED_TROLL_CPP := $(SRC_DIR)/Parts/TECHNO/TrollData.cpp
EMBED_TROLL_SYM := troll_png
EMBED_TROLL_HDR := TrollData.h
EMBED_TROLL_NS  := KOE::Data

EMBED_ASSETS   := $(EMBED_ASSETS) $(EMBED_TROLL_CPP)

# The 21 credits pictures are regenerated together from the re-shot source PNGs,
# resized to the 512x640 picture region and stored as indexed PNG + 6-bit VGA
# palette (see tools/embed_credits.py).
EMBED_CREDITS_TOOL := $(SRC_DIR)/tools/embed_credits.py
EMBED_CREDITS_DIR  := $(SRC_DIR)/tools/credits_pics
EMBED_CREDITS_CPP  := $(SRC_DIR)/Parts/CREDITS/CreditsPicsData.cpp
EMBED_CREDITS_HDR  := CreditsPicsData.h

EMBED_ASSETS   := $(EMBED_ASSETS) $(EMBED_CREDITS_CPP)

# The typeface is embedded like every other asset so the demo needs no font
# file at run time.
EMBED_FONT_SRC := $(SRC_DIR)/Resources/fonts/LiberationSerif-Regular.ttf
EMBED_FONT_CPP := $(SRC_DIR)/Resources/FontData.cpp
EMBED_FONT_SYM := liberation_serif_ttf
EMBED_FONT_HDR := FontData.h
EMBED_FONT_NS  := Font::Data

EMBED_ASSETS   := $(EMBED_ASSETS) $(EMBED_FONT_CPP)


# The bitmap sources and embedding tools are optional: the generated
# translation units below are committed, so a source-only tree (a release
# tarball, the public mirror) builds as-is. Regeneration applies only where
# the bitmap sources are present.
ifeq ($(wildcard $(EMBED_TOOL)),)
else
# Regenerate only when the bitmap (or the tool) is newer than the output.
# Passing FORCE_EMBED=1 adds FORCE to the prerequisites, which is how the
# `embed` target below re-runs these very same rules unconditionally.
$(EMBED_BG_CPP): $(EMBED_BG_SRC) $(EMBED_TOOL) $(if $(FORCE_EMBED),FORCE)
	$(EMBED_PY) "$(EMBED_TOOL)" "$(EMBED_BG_SRC)" "$(EMBED_BG_SYM)" "$@" "$(EMBED_BG_HDR)" "$(EMBED_NS)"

$(EMBED_TILE_CPP): $(EMBED_TILE_SRC) $(EMBED_TOOL) $(if $(FORCE_EMBED),FORCE)
	$(EMBED_PY) "$(EMBED_TOOL)" "$(EMBED_TILE_SRC)" "$(EMBED_TILE_SYM)" "$@" "$(EMBED_TILE_HDR)" "$(EMBED_NS)"

$(EMBED_ALKU_CPP): $(EMBED_ALKU_SRC) $(EMBED_TOOL) $(if $(FORCE_EMBED),FORCE)
	$(EMBED_PY) "$(EMBED_TOOL)" "$(EMBED_ALKU_SRC)" "$(EMBED_ALKU_SYM)" "$@" "$(EMBED_ALKU_HDR)" "$(EMBED_ALKU_NS)"

$(EMBED_TROLL_CPP): $(EMBED_TROLL_SRC) $(EMBED_TOOL) $(if $(FORCE_EMBED),FORCE)
	$(EMBED_PY) "$(EMBED_TOOL)" "$(EMBED_TROLL_SRC)" "$(EMBED_TROLL_SYM)" "$@" "$(EMBED_TROLL_HDR)" "$(EMBED_TROLL_NS)"

$(EMBED_CREDITS_CPP): $(wildcard $(EMBED_CREDITS_DIR)/*.png) $(EMBED_CREDITS_TOOL) $(if $(FORCE_EMBED),FORCE)
	$(EMBED_PY) "$(EMBED_CREDITS_TOOL)" "$(EMBED_CREDITS_DIR)" "$@" "$(SRC_DIR)/Parts/CREDITS/$(EMBED_CREDITS_HDR)"

$(EMBED_FONT_CPP): $(EMBED_FONT_SRC) $(EMBED_TOOL) $(if $(FORCE_EMBED),FORCE)
	$(EMBED_PY) "$(EMBED_TOOL)" "$(EMBED_FONT_SRC)" "$(EMBED_FONT_SYM)" "$@" "$(EMBED_FONT_HDR)" "$(EMBED_FONT_NS)"

.PHONY: FORCE
FORCE:

# Unconditional re-embed of every asset, reusing the rules above.
embed:
	@$(MAKE) --no-print-directory FORCE_EMBED=1 $(EMBED_ASSETS)
endif

verify-toolchain:
	@$(WHICH) "$(CXX)" > $(NULLDEV) 2>&1 || ( \
	  echo "ERROR: CXX '$(CXX)' not found. For Web, source emsdk_env.sh first; otherwise set CXX (e.g. CXX=g++)." ; exit 1 )
	@$(WHICH) "$(CC)" > $(NULLDEV) 2>&1 || ( \
	  echo "ERROR: CC '$(CC)' not found.  For Web, source emsdk_env.sh first; otherwise set CC (e.g. CC=gcc)." ; exit 1 )
ifeq ($(wildcard $(EMBED_TOOL)),)
	@echo "note: bitmap sources not shipped - building the committed embedded data (Python 3 not required)"
else
	@$(WHICH) "$(EMBED_PY)" > $(NULLDEV) 2>&1 || ( \
	  echo ERROR: EMBED_PY '$(EMBED_PY)' not found. Install Python 3 or set EMBED_PY=python3 ; exit 1 )
endif

$(BUILD_DIR):
	@$(MKDIR_LINE)

$(ROOT_OUTPUT_DIR):
	@$(MKDIR_LINE)

$(OUTPUT_DIR):
	@$(MKDIR_LINE)

$(TARGET): $(OBJECTS) | $(OUTPUT_DIR)
	@$(MKDIR_LINE)
ifeq ($(findstring em++,$(notdir $(CXX))),em++)
	$(CXX) --shell-file Web/shell.html -o "$@" $(OBJECTS) $(LDFLAGS) $(LDLIBS)
	@cp -f Resources/atomic_playboy.ico $(OUTPUT_DIR)/favicon.ico
else
	$(CXX) -o "$@" $(OBJECTS) $(LDFLAGS) $(LDLIBS)
endif

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@$(MKDIR_LINE)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	@$(MKDIR_LINE)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cxx
	@$(MKDIR_LINE)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.CPP
	@$(MKDIR_LINE)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.C++
	@$(MKDIR_LINE)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR_LINE)
	$(CC)  $(CPPFLAGS) $(CFLAGS)   -c "$<" -o "$@"

serve: all
	python -m http.server -d $(OUTPUT_DIR)

# ---------------------------------------------------------------------------
# Flatpak packaging
#
# Build a single-file .flatpak bundle. The demo is compiled inside the
# Freedesktop SDK that matches the runtime (see flatpak/build.sh), so the
# executable runs on any Flatpak host. No flatpak-builder required.
# ---------------------------------------------------------------------------
flatpak:
	@bash flatpak/build.sh

clean:
	-@$(RM_RF) "$(ROOT_BUILD_DIR)" 2>$(NULLDEV)
	-@$(RM_RF) "$(ROOT_OUTPUT_DIR)" 2>$(NULLDEV)

DEPFILES := $(OBJECTS:.o=.d)
-include $(DEPFILES)
