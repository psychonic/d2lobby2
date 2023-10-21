###########################################
### EDIT THESE PATHS FOR YOUR OWN SETUP ###
###########################################

HL2SDK_DOTA = ../hl2sdk-dota
MMSOURCE = ../metamod-source

#####################################
### EDIT BELOW FOR OTHER PROJECTS ###
#####################################

PROJECT = d2lobby

OBJECTS_MAIN = \
	constants.cpp    \
	d2lobby.cpp      \
	eventlog.cpp     \
	forcedheroes.cpp \
	gcmgr.cpp        \
	httpmgr.cpp      \
	lobbymgr.cpp     \
	logger.cpp       \
	norunes.cpp      \
	pb2json.cpp      \
	pluginsystem.cpp \
	scripttools.cpp  \
	steamnet.cpp     \
	util.cpp

OBJECTS_PROTO = \
	generated_proto/base_gcmessages.pb.cc                         \
	generated_proto/dota_client_enums.pb.cc                       \
	generated_proto/dota_commonmessages.pb.cc                     \
	generated_proto/dota_gcmessages_client_match_management.pb.cc \
	generated_proto/dota_gcmessages_common.pb.cc                  \
	generated_proto/dota_gcmessages_common_match_management.pb.cc \
	generated_proto/dota_gcmessages_server.pb.cc                  \
	generated_proto/dota_shared_enums.pb.cc                       \
	generated_proto/dota_usermessages.pb.cc                       \
	generated_proto/econ_gcmessages.pb.cc                         \
	generated_proto/econ_shared_enums.pb.cc                       \
	generated_proto/gcsdk_gcmessages.pb.cc                        \
	generated_proto/network_connection.pb.cc                      \
	generated_proto/networkbasetypes.pb.cc                        \
	generated_proto/steammessages.pb.cc

##############################################
### CONFIGURE ANY OTHER FLAGS/OPTIONS HERE ###
##############################################

C_OPT_FLAGS = -DNDEBUG -O3 -funroll-loops -pipe -fno-strict-aliasing
C_DEBUG_FLAGS = -D_DEBUG -DDEBUG -g -ggdb3
C_GCC4_FLAGS = -fvisibility=hidden -fPIC
CPP_GCC4_FLAGS = -fvisibility-inlines-hidden
CPP = clang

HL2PUB = $(HL2SDK_DOTA)/public

INCLUDE += -I$(HL2SDK_DOTA)/common/protobuf-2.6.1/src -I$(HL2SDK_DOTA)/public/game/server \
	-I../jansson-2.5/src -I../steamworks/public -I../subhook
METAMOD = $(MMSOURCE)/core

LIB_EXT = so
HL2LIB = $(HL2SDK_DOTA)/lib/linux

LIB_PREFIX = lib
LIB_SUFFIX = .$(LIB_EXT)

INCLUDE += -I. -I.. 

LINK += -Wl,--exclude-libs,ALL -lm -lgcc_eh -lstdc++ $(HL2LIB)/tier1_i486.a libsteam_api.so $(LIB_PREFIX)vstdlib$(LIB_SUFFIX) $(LIB_PREFIX)tier0$(LIB_SUFFIX) $(HL2LIB)/interfaces_i486.a ../jansson-2.5/src/.libs/libjansson.a ../protobuf-2.6.1/src/.libs/libprotobuf.a ../subhook/libsubhook.a

INCLUDE += -I$(HL2PUB) -I$(HL2PUB)/engine -I$(HL2PUB)/tier0 -I$(HL2PUB)/tier1 -I$(METAMOD) \
	-I$(METAMOD)/sourcehook 

LINK += -m64 -lm -ldl -shared

CFLAGS += -D_LINUX -DLINUX -DPOSIX -Dstricmp=strcasecmp -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp \
	-D_snprintf=snprintf -D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp -DCOMPILER_GCC -Wall \
	-Wno-overloaded-virtual -Wno-switch -Wno-unused -msse -DHAVE_STDINT_H -m64 -DPLATFORM_64BITS \
	-DVERSION_SAFE_STEAM_API_INTERFACES -DSUBHOOK_IMPLEMENTATION
CPPFLAGS += -Wno-non-virtual-dtor -fno-exceptions -std=c++11

################################################
### DO NOT EDIT BELOW HERE FOR MOST PROJECTS ###
################################################

BINARY = $(PROJECT).$(LIB_EXT)

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug
	CFLAGS += $(C_DEBUG_FLAGS)
else
	BIN_DIR = Release
	CFLAGS += $(C_OPT_FLAGS)
endif

LIB_EXT = so

IS_CLANG := $(shell $(CPP) --version | head -1 | grep clang > /dev/null && echo "1" || echo "0")

ifeq "$(IS_CLANG)" "1"
	CPP_MAJOR := $(shell $(CPP) --version | grep clang | sed "s/.*version \([0-9]\)*\.[0-9]*.*/\1/")
	CPP_MINOR := $(shell $(CPP) --version | grep clang | sed "s/.*version [0-9]*\.\([0-9]\)*.*/\1/")
else
	CPP_MAJOR := $(shell $(CPP) -dumpversion >&1 | cut -b1)
	CPP_MINOR := $(shell $(CPP) -dumpversion >&1 | cut -b3)
endif

# If not clang
ifeq "$(IS_CLANG)" "0"
	CFLAGS += -mfpmath=sse
endif

# Clang || GCC >= 4
ifeq "$(shell expr $(IS_CLANG) \| $(CPP_MAJOR) \>= 4)" "1"
	CFLAGS += $(C_GCC4_FLAGS)
	CPPFLAGS += $(CPP_GCC4_FLAGS)
endif

# Clang >= 3 || GCC >= 4.7
ifeq "$(shell expr $(IS_CLANG) \& $(CPP_MAJOR) \>= 3 \| $(CPP_MAJOR) \>= 4 \& $(CPP_MINOR) \>= 7)" "1"
	CFLAGS += -Wno-delete-non-virtual-dtor
endif

# OS is Linux and not using clang
#ifeq "$(shell expr $(IS_CLANG) \= 0)" "1"
#	LINK += -static-libgcc
#endif

OBJ_MAIN_BIN := $(OBJECTS_MAIN:%.cpp=$(BIN_DIR)/%.o)
OBJ_PROTO_BIN := $(OBJECTS_PROTO:generated_proto/%.cc=$(BIN_DIR)/generated_proto/%.o)

MAKEFILE_NAME := $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))

$(BIN_DIR)/%.o: %.cpp
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

$(BIN_DIR)/generated_proto/%.o: generated_proto/%.cc
	$(CPP) $(INCLUDE) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

all:
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/generated_proto
	ln -sf $(HL2LIB)/$(LIB_PREFIX)vstdlib$(LIB_SUFFIX); \
	ln -sf $(HL2LIB)/$(LIB_PREFIX)tier0$(LIB_SUFFIX); \
	ln -sf ../steamworks/redistributable_bin/linux64/libsteam_api.so; \
	$(MAKE) -f $(MAKEFILE_NAME) extension

extension: $(OBJ_MAIN_BIN) $(OBJ_PROTO_BIN)
	$(CPP) $(INCLUDE) $(OBJ_MAIN_BIN) $(OBJ_PROTO_BIN) $(LINK) -o $(BIN_DIR)/$(BINARY)

debug:
	$(MAKE) -f $(MAKEFILE_NAME) all DEBUG=true

default: all

clean:
	rm -rf $(BIN_DIR)/*.o
	rm -rf $(BIN_DIR)/$(BINARY)
	rm -rf $(BIN_DIR)/generated_proto/*.o

