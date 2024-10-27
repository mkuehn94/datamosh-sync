# datamosh-sync makefile

CC = g++
CFLAGS = -Wall -Wextra -g -O2 -fopenmp -std=c++11
LDFLAGS = -lavformat -lavcodec -lavutil -lswscale -lm -fopenmp

GTK_CFLAGS  = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LDFLAGS = $(shell pkg-config --libs   gtk+-3.0 2>/dev/null)

SRCDIR = src
OBJDIR = obj
TARGET = datamosh-sync
GUI_TARGET = datamosh-gui

CORE_SOURCES = $(SRCDIR)/main.cpp \
               $(SRCDIR)/audio_analyzer.cpp $(SRCDIR)/effects.cpp \
               $(SRCDIR)/processor.cpp $(SRCDIR)/warp.cpp \
               $(SRCDIR)/arrange.cpp

CORE_OBJECTS = $(CORE_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(CORE_OBJECTS)
	$(CC) $(CORE_OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET)"

$(GUI_TARGET): $(OBJDIR)/gui_main.o $(OBJDIR)/arrange.o
	$(CC) $(OBJDIR)/gui_main.o $(OBJDIR)/arrange.o -o $(GUI_TARGET) $(GTK_LDFLAGS)
	@echo "Built $(GUI_TARGET)"

$(OBJDIR)/gui_main.o: $(SRCDIR)/gui_main.cpp $(SRCDIR)/arrange.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

# NOTE: depend on ALL headers because make was happily linking stale
# object files against the new class layout and the whole thing
# segfaulted in avformat_open_input of all places
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/*.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/test_effect: test_effect.cpp $(OBJDIR)/effects.o
	$(CC) $(CFLAGS) $^ -o $@

$(OBJDIR)/test_basic: test_basic.cpp $(filter-out $(OBJDIR)/main.o,$(CORE_OBJECTS))
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

check: $(OBJDIR)/test_effect
	./$(OBJDIR)/test_effect
	PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=. python3 tests/test_shift.py
	PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=. python3 -m unittest discover -s tests -p 'test_tools.py'

clean:
	rm -rf $(OBJDIR) $(TARGET) $(GUI_TARGET)
	@echo "Cleaned"

.PHONY: all clean check
