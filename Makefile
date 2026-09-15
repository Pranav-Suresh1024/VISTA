CXX := g++
FLEX := flex
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude
FLEX_CXXFLAGS := $(CXXFLAGS) -Wno-sign-compare -Wno-unused-function

TARGET := build/vista
GENERATED_SCANNER := build/vista_lexer.cpp
SOURCES := src/main.cpp src/diagnostic.cpp src/token.cpp
HEADERS := include/diagnostic.hpp include/scanner.hpp include/token.hpp include/version.hpp

.PHONY: all test demo clean

all: $(TARGET)

$(GENERATED_SCANNER): src/vista.l $(HEADERS)
	@mkdir -p build
	$(FLEX) --outfile=$(GENERATED_SCANNER) src/vista.l

$(TARGET): $(SOURCES) $(HEADERS) $(GENERATED_SCANNER)
	$(CXX) $(FLEX_CXXFLAGS) $(SOURCES) $(GENERATED_SCANNER) -o $(TARGET)

test: $(TARGET)
	@bash tests/test_scanner.sh

demo: $(TARGET)
	@echo "VISTA Stage 2 scanner demonstration"
	@echo
	@./$(TARGET) --version
	@echo
	@./$(TARGET) examples/valid_scholarship.vista --emit tokens
	@echo
	@./$(TARGET) examples/lexical_error.vista --emit tokens || true

clean:
	@rm -rf -- build out
