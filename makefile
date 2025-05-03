CXX = g++
CXXFLAGS = -g -O0 -Wall -std=c++20 -D_CHAR_UNSIGNED -funsigned-char
SRC = $(shell find . -name "*.cpp")
OBJ = $(SRC:.cpp=.o)
TARGETDIR = bin
TARGET = lj_decompiler_v2
INSTALL_NAME = ljdec2

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGETDIR)/$(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

install: $(TARGET)
	cp $(TARGETDIR)/$(TARGET) /usr/local/bin/$(INSTALL_NAME)

clean:
	rm -f **/*.o $(TARGETDIR)/$(TARGET)
