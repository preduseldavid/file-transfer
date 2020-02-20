#==============================================================================
# Project config
#==============================================================================

# compiler identifier
CC=gcc
# compiler options
CFLAGS=-c -g -std=gnu99 -Wall

# flags which holds the OS and the CPU arhitecture
ifeq ($(OS),Windows_NT)
    CFLAGS           += -D WIN32
    OS_SUFFIX        := win
    OS_FAMILY        := WIN
    OS_FAMILY_SUFFIX := win
    
    ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
        CFLAGS       += -D AMD64
    endif
    ifeq ($(PROCESSOR_ARCHITECTURE),x86)
        CFLAGS       += -D IA32
    endif
else
    # OS kernel name
    UNAME_S          := $(shell uname -s)
    CFLAGS           += -D UNIX
    OS_FAMILY        := UNIX
    OS_FAMILY_SUFFIX := unix
    
    ifeq ($(UNAME_S),Linux)
        CFLAGS      += -D LINUX
        OS_SUFFIX    := linux
    endif
    ifeq ($(UNAME_S),Darwin)
        CFLAGS       += -D OSX
        OS_SUFFIX    := osx
    endif
    # processor arhitecture
    UNAME_P          := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
        CFLAGS       += -D AMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        CFLAGS       += -D IA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
        CFLAGS       += -D ARM
    endif
endif

TARGET               := $(OS_SUFFIX)/file_transfer


INC_DIR         := include
LIB_DIR         := lib
BUILD_DIR       := build
BIN_DIR         := bin
SRC_DIR         := src
STD_DIR         := std

#------------------------------------------------------------------------------
# Modules
#------------------------------------------------------------------------------

MODULES_SRC         := main \
                       send \
                       receive \
                       receive_file \
                       data_types \
                       user_thread
                       
MODULES_STD	    := tcpip_server \
		       error

MODULES             := $(MODULES_SRC) $(MODULES_STD)

#==============================================================================
# SOURCE modules
#==============================================================================

#------------------------------------------------------------------------------
# data_types module 
#------------------------------------------------------------------------------
data_types              := data_types.o
data_types.o            := data_types.c \
                           data_types.h
data_types.dep          := $(addprefix $(SRC_DIR)/data_types/, $(data_types.o))

#------------------------------------------------------------------------------
# send module 
#------------------------------------------------------------------------------
send                    := send.o
send.o                  := send.c \
                           send.h
send.dep                := $(addprefix $(SRC_DIR)/send/, $(send.o))

#------------------------------------------------------------------------------
# receive module 
#------------------------------------------------------------------------------
receive                 := receive.o
receive.o               := receive.c \
                           receive.h
receive.dep             := $(addprefix $(SRC_DIR)/receive/, $(receive.o))

#------------------------------------------------------------------------------
# receive_file module 
#------------------------------------------------------------------------------
receive_file            := receive_file.o
receive_file.o          := $(subst OS_SUFFIX,$(OS_SUFFIX), receive_file_OS_SUFFIX.h) \
                           $(subst OS_SUFFIX,$(OS_SUFFIX), receive_file_OS_SUFFIX.c)
receive_file.dep        := $(addprefix $(SRC_DIR)/receive_file/, $(receive_file.o))

#------------------------------------------------------------------------------
# main module 
#------------------------------------------------------------------------------
main                    := main.o
main.o                  := main.c
main.dep                := $(addprefix $(SRC_DIR)/main/, $(main.o))

#------------------------------------------------------------------------------
# user_thread module 
#------------------------------------------------------------------------------
user_thread             := user_thread.o
user_thread.o           := user_thread.c \
                           user_thread.h
user_thread.dep         := $(addprefix $(SRC_DIR)/user_thread/, $(user_thread.o))

#==============================================================================
# STANDARD modules
#==============================================================================

#------------------------------------------------------------------------------
# error module 
#------------------------------------------------------------------------------
error                   := error.o
error.o                 := $(subst OS_FAMILY_SUFFIX,$(OS_FAMILY_SUFFIX), error_OS_FAMILY_SUFFIX.c) \
                           error.h
error.dep               := $(addprefix $(STD_DIR)/error/, $(error.o))

#------------------------------------------------------------------------------
# tcpip_server module 
#------------------------------------------------------------------------------
tcpip_server            := tcpip_server.o
tcpip_server.o          := $(subst OS_FAMILY_SUFFIX,$(OS_FAMILY_SUFFIX), tcpip_server_OS_FAMILY_SUFFIX.c) \
                           tcpip_server.h
tcpip_server.dep        := $(addprefix $(STD_DIR)/tcpip_server/, $(tcpip_server.o))

#==============================================================================
# Include directories
#==============================================================================
INCLUDES            :=  

#------------------------------------------------------------------------------
# Libraries
#------------------------------------------------------------------------------

ifeq ($(OS_FAMILY),WIN)
LIBRARIES_DIRS      :=  winsock2/x64

LIBRARIES_FILES     :=  pthread 

else ifeq ($(OS_FAMILY),UNIX)
LIBRARIES_DIRS      :=  

LIBRARIES_FILES     :=  pthread

endif

#------------------------------------------------------------------------------
# !!!DO NOT MODIFY THE SECTION BELOW !!!
#------------------------------------------------------------------------------
INC             := $(addprefix -I$(INC_DIR)/,$(INCLUDES))
CONFIG_DIR	:= config

LIB_DIRS        := $(addprefix -L$(LIB_DIR)/, $(LIBRARIES_DIRS))
LIB_FILES       := $(addprefix -l, $(LIBRARIES_FILES))

EXPAND          := $(foreach mod, $(MODULES), $($(mod)))
OBJECTS         := $(addprefix $(BUILD_DIR)/, $(EXPAND))

DEPENDENCIES    := $(subst .o,.dep,$(notdir $(OBJECTS)))
EXPAND_DEP      := $(foreach dep, $(DEPENDENCIES), $($(dep)))
INC_DEP         := $(addprefix -I,$(sort $(dir $(EXPAND_DEP))))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $^ -o $(BIN_DIR)/$@ $(LIB_DIRS) $(LIB_FILES)

$(BUILD_DIR)/%.o: $($(subst .o,.dep,$(notdir $@)))
	$(CC) -I$(CONFIG_DIR) $(CFLAGS) $(INC) $(INC_DEP) $(filter %.c, $($(subst .o,.dep,$(notdir $@)))) -o $(BUILD_DIR)/$*.o 

ifeq ($(OS_FAMILY),WIN)
clean: 
	cls
	del /f /q $(BUILD_DIR)\*.obj
	
else ifeq ($(OS_FAMILY),UNIX)
clean: 
	clear
	rm -rf $(BUILD_DIR)/*.o
	
endif

print-% : ; @echo $* = $($*)
