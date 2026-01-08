CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pthread
OPTFLAGS ?= -O2 -fomit-frame-pointer -fdata-sections -ffunction-sections -pipe -DNDEBUG -flto
CFLAGS += $(OPTFLAGS)
LDFLAGS = -pthread -lm -lrt -Wl,--gc-sections -flto

TARGET = trimui_inputd

SRCDIR = src
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj
BINDIR = $(BUILDDIR)/$(TARGET)/bin

SRCS = $(shell find $(SRCDIR) -type f -name "*.c")
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

.PHONY: all clean

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	$(MKDIR_P) $(OBJDIR)

$(BINDIR):
	$(MKDIR_P) $(BINDIR)

clean:
	rm -rf $(BUILDDIR)
	rm -f $(TARGET)
	find $(SRCDIR) -name '*.o' -delete

MKDIR_P ?= mkdir -p
