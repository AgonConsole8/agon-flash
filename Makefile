# Program name
NAME=flash

# verbosity - comment to show all output
V = @

# Tool directories
AGONDEV_TOOLCHAIN ?= $(shell agondev-config --prefix)
TOOLBINDIR=$(AGONDEV_TOOLCHAIN)/bin
INCLUDEDIR=$(AGONDEV_TOOLCHAIN)/include
LINKERCONFIG=./config/linker.conf
LIBDIR=$(AGONDEV_TOOLCHAIN)/lib

# Architecture
ARCH=ez80+full
TARGET=ez80-none-elf

# Tools and flags
## Compiler
CC=$(TOOLBINDIR)/ez80-none-elf-clang
CFLAGS=-target $(TARGET) -Oz -Wa,-march=$(ARCH) -I $(INCLUDEDIR)
## Assembler
ASM=$(TOOLBINDIR)/ez80-none-elf-as
ASMFLAGS=-march=$(ARCH)
## Linker
LINKER=$(TOOLBINDIR)/ez80-none-elf-ld
ifeq ($(LDHAS_ARG_PROCESSING),1)
	LINKER_ARG=-defsym=_parse_option=___arg_processing
endif
ifeq ($(LDHAS_EXIT_HANDLER), 1)
    LINKER_EXIT=-defsym=_has_exit_handler=1
else
    LINKER_EXIT=-defsym=_has_exit_handler=0
endif
LINKERFLAGS=$(LINKER_ARG) $(LINKER_EXIT) -Map=$(BINDIR)/$(NAME).map -T $(LINKERCONFIG) --oformat binary -o 
LINKERLIBFLAGS=-L$(LIBDIR) -\( -l c -l agon -l crt -l fp -\) 
SETPROGNAME=$(TOOLBINDIR)/agondev-setname

# project directories
SRCDIR=src
OBJDIR=obj
BINDIR=bin
# Final binary
BINARY=$(BINDIR)/$(NAME).bin

# Automatically get all object names from sourcefiles
OBJS=$(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.asm)) \
	 $(patsubst $(SRCDIR)/%.s,   $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.s)) \
	 $(patsubst $(SRCDIR)/%.src, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.src)) \
	 $(patsubst $(SRCDIR)/%.c,   $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.c)) \
	 $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cpp))

# Default rule
all: $(BINDIR) $(OBJDIR) $(BINARY)
	@echo [Setting name \'$(NAME).bin\' in binary]
	$(V)$(SETPROGNAME) $(BINARY) >/dev/null
	@echo [Done]
# Linking all compiled objects into final binary
$(BINARY):$(OBJS)
	@echo [Linking $(BINARY)]
	$(V)$(LINKER) $(LINKERFLAGS)$@ $(OBJS) $(LINKERLIBFLAGS)

# Compile each .c file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo [compiling $<]
	$(V)$(CC) $(CFLAGS) $< -c -o $@
# Compile each .cpp file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@echo [compiling $<]
	$(V)$(CC) $(CFLAGS) $< -c -o $@
# Assemble each .s file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.s
	@echo [assembling $<]
	$(V)$(ASM) $(ASMFLAGS) $< -o $@
# Assemble each .asm file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.asm
	@echo [assembling $<]
	$(V)$(ASM) $(ASMFLAGS) $< -o $@
# Assemble each .src file into .o file
$(OBJDIR)/%.o: $(SRCDIR)/%.src
	@echo [assembling $<]
	$(V)$(ASM) $(ASMFLAGS) $< -o $@

# Directories to create
$(BINDIR):
	@mkdir $(BINDIR)
$(OBJDIR):
	@mkdir $(OBJDIR)

clean:
	@$(RM) -r $(BINDIR) $(OBJDIR) 
