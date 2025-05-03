CXX = g++
CXXFLAGS = -Wall -std=c++20 -D_CHAR_UNSIGNED -funsigned-char
SRC = $(shell find . -name "*.cpp")
OBJ = $(SRC:.cpp=.o)
TARGET = lj_decompiler_v2

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
