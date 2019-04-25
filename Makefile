exec = editor

OBJDIR = obj
CFLAGS =  -g -Wall -Isrc
LDFLAGS = -lbase -lGL -lX11 -lXxf86vm

headers = $(wildcard src/*.h src/*/*.h)
sources = $(wildcard src/*.cpp src/*/*.cpp)
objects = $(addprefix $(OBJDIR)/, $(sources:.cpp=.o))
dirs    = $(dir $(objects))

# Colour coding of g++ output - highlights errors and warnings
SED = sed -e 's/error/\x1b[31;1merror\x1b[0m/g' -e 's/warning/\x1b[33;1mwarning\x1b[0m/g'
SED2 = sed -e 's/undefined reference/\x1b[31;1mundefined reference\x1b[0m/g'

ifeq ($(OS),Windows_NT)
LDFLAGS = -lbase -lgdi32 -lopengl32 -static-libgcc -static-libstdc++ -mwindows -lwinmm
LDFLAGS += -DWIN32
else
CFLAGS += -DLINUX
CFLAGE += -lpthread
endif

.PHONY: clean

all: $(exec)

$(exec): $(objects)
	@echo -e "\033[34;1m[ Linking ]\033[0m"
	@echo $(CXX) -o $(exec) $(objects) $(CFLAGS) $(LDFLAGS)
	@$(CXX) -o $(exec) $(objects) $(CFLAGS) $(LDFLAGS) 2>&1 | $(SED2)

$(OBJDIR)/%.o: %.cpp $(headers) | $(OBJDIR)
	@echo $<
	@$(CXX) $(CFLAGS) -c $< -o $@ 2>&1 | $(SED)
	
$(OBJDIR):
	mkdir -p $(dirs);

clean:
	rm -f *~ */*~ $(exec)
	rm -rf $(OBJDIR)

