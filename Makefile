CXX := g++
FLEX := flex
BISON := bison
BISONFLAGS := -Wall -Werror
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude -Ibuild
FLEX_CXXFLAGS := $(CXXFLAGS) -Wno-sign-compare -Wno-unused-function

TARGET := build/vista
GENERATED_SCANNER := build/vista_lexer.cpp
GENERATED_PARSER := build/vista_parser.cpp
GENERATED_PARSER_HEADER := build/vista_parser.hpp
SOURCES := src/main.cpp src/ast.cpp src/diagnostic.cpp src/parser.cpp src/token.cpp
HEADERS := include/ast.hpp include/diagnostic.hpp include/parser.hpp include/scanner.hpp include/token.hpp include/version.hpp

.PHONY: all test demo clean

all: $(TARGET)

$(GENERATED_SCANNER): src/vista.l $(HEADERS)
	@mkdir -p build
	$(FLEX) --outfile=$(GENERATED_SCANNER) src/vista.l
	@touch -r src/vista.l $(GENERATED_SCANNER)

$(GENERATED_PARSER) $(GENERATED_PARSER_HEADER) &: src/vista.y $(HEADERS)
	@mkdir -p build
	$(BISON) $(BISONFLAGS) --defines=$(GENERATED_PARSER_HEADER) --output=$(GENERATED_PARSER) src/vista.y
	@touch -r src/vista.y $(GENERATED_PARSER) $(GENERATED_PARSER_HEADER)

$(TARGET): $(SOURCES) $(HEADERS) $(GENERATED_SCANNER) $(GENERATED_PARSER) $(GENERATED_PARSER_HEADER)
	$(CXX) $(FLEX_CXXFLAGS) -Wno-unused-parameter $(SOURCES) $(GENERATED_SCANNER) $(GENERATED_PARSER) -o $(TARGET)

test: $(TARGET)
	@bash tests/test_scanner.sh
	@bash tests/test_parser.sh

demo: $(TARGET)
	@echo "VISTA Stage 3 parser demonstration"
	@echo
	@./$(TARGET) --version
	@echo
	@./$(TARGET) examples/valid_scholarship.vista --emit ast
	@echo
	@./$(TARGET) examples/syntax_error.vista --emit ast || true

clean:
	@rm -rf -- build out
