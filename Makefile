CC = g++
CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lavformat -lavcodec -lavutil -lswscale

SRCDIR = src
OBJDIR = obj
TARGET = datamosh-sync

SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
