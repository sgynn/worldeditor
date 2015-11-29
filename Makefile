exec = editor

CFLAGS =  -g -Wall -Isrc
LDFLAGS = -lbase -lGL -lX11 -lXxf86vm

headers = $(wildcard src/*.h src/*/*.h)
sources = $(wildcard src/*.cpp src/*/*.cpp)
objects = $(sources:.cpp=.o)

# Colour coding of g++ output - highlights errors and warnings
SED = sed -e 's/error/\x1b[31;1merror\x1b[0m/g' -e 's/warning/\x1b[33;1mwarning\x1b[0m/g'


ifeq ($(OS),Windows_NT)
LDFLAGS = -lbase -lgdi32 -lopengl32 -static-libgcc -static-libstdc++
LDFLAGS += -DWIN32
else
CFLAGS += -DLINUX
endif

.PHONY: clean

all: $(exec)

$(exec): $(objects)
	@echo -e "\033[34;1m[ Linking ]\033[0m"
	$(CXX) -o $(exec) $(objects) $(CFLAGS) $(LDFLAGS)

%.o: %.cpp $(headers) Makefile
	@echo $<
	@$(CXX) $(CFLAGS) -c $< -o $@ 2>&1 | $(SED)
	

clean:
	rm -f *~ */*~ $(objects) $(testobj) $(exec)