# datamosh-sync makefile
# note: requires libavformat, libavcodec, libavutil, libswscale
# and gtk+-3.0 dev headers

CC = g++
CFLAGS = -Wall -Wextra -g -std=c++11
LDFLAGS = -lavformat -lavcodec -lavutil -lswscale -lm

# GTK optional - disable if not available
# GTK_CFLAGS = `pkg-config --cflags gtk+-3.0 2>/dev/null`
# GTK_LDFLAGS = `pkg-config --libs gtk+-3.0 2>/dev/null`
# CFLAGS += $(GTK_CFLAGS)
# LDFLAGS += $(GTK_LDFLAGS)

SRCDIR = src
OBJDIR = obj
TARGET = datamosh-sync

# core files only (gui disabled until gtk available)
SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/video_processor.cpp \
          $(SRCDIR)/audio_analyzer.cpp $(SRCDIR)/effects.cpp \
          $(SRCDIR)/processor.cpp

OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@echo "Cleaned"

.PHONY: all clean
