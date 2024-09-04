# Variables
CC = g++
CFLAGS = --std=c++17 --verbose -I ./dependencies/SDL2-2.30.5/include/ -I ./dependencies/SDL2_ttf-2.22.0/ -I ./dependencies/boost_1_85_0/ -I ./dependencies/nativefiledialog-extended-1.2.1/src/include/ -I /usr/X11/include
LDFLAGS = -L ./dependencies/SDL2-2.30.5/build/ -lSDL2 -ObjC -framework Cocoa -arch arm64 --verbose -framework CoreAudio -framework AudioToolbox -framework CoreHaptics -framework CoreVideo -framework ForceFeedback -framework GameController -framework Metal -framework IOKit -framework Carbon -lSDL2_ttf -L ./dependencies/SDL2_ttf-2.22.0/build -lNFD -L ./dependencies/nativefiledialog-extended-1.2.1/src/ -framework CoreServices -v
SRCS = ${wildcard CircuitSandbox/*.cpp}
OBJS = $(SRCS:.cpp=.o) 
ASMS = $(SRCS:.cpp=.s)
TARGET = CircuitSandboxApp

woof:
	@echo $(SRCS)
	@echo $(OBJS)
	@echo $(ASMS)

meow: $(OBJS)
	@echo hi 
	@echo ${SRCS}
	@echo $(OBJS)

# Rules
all: $(TARGET)

$(TARGET): $(OBJS) # linking .o together
	@echo meow
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJS)

%.o: %.s
# %.o: # assembling cannot be done with g++, must be done with as
	echo Assembling to $@
	as -arch arm64 $< -o $@

# $(ASMS): $(OBJS)
%.s: %.cpp # compiling 
# %.s: 
	@echo Compiling to $@
	$(CC) -S $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean $(TARGET)