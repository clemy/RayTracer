SRCDIR := src
LIBS := -lz
OUT := raytracer
# this optimizes for the compiler machine architecture:
OPTFLAGS := -Ofast -flto -march=native
# this optimizes generally:
#OPTFLAGS := -O3
# debug builds:
#OPTFLAGS := -Og -g
# even more debug builds:
#OPTFLAGS := -Og -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all
CPPFLAGS := -Wall -Wextra -std=c++17 -pedantic-errors -pthread $(OPTFLAGS)
CC := g++
LD := $(CC)
OBJDIR := obj

SRCS := $(wildcard $(SRCDIR)/*.cpp)
DEPS := $(wildcard $(SRCDIR)/*.h)
OBJS := $(patsubst src/%.cpp, $(OBJDIR)/%.o, $(SRCS))

vpath %.cpp $(SRCDIR)

.PHONY: all clean
all: $(OBJDIR) $(OUT)

$(OBJDIR):
	mkdir -p $@

$(OUT): $(OBJS)
	$(CC) $(CPPFLAGS) -o $@ $^ $(LIBS)

$(OBJDIR)/%.o: %.cpp $(DEPS)
	$(LD) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -rf $(OUT) $(OBJDIR)
