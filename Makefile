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
SOURCES := src/main.cpp src/analysis.cpp src/ast.cpp src/certificate.cpp src/certify.cpp src/codegen.cpp src/diagnostic.cpp src/evaluator.cpp src/hash.cpp src/linker.cpp src/module.cpp src/parser.cpp src/partition.cpp src/semantic.cpp src/token.cpp src/validation.cpp
HEADERS := include/analysis.hpp include/ast.hpp include/certificate.hpp include/certify.hpp include/codegen.hpp include/diagnostic.hpp include/evaluator.hpp include/hash.hpp include/linker.hpp include/module.hpp include/parser.hpp include/partition.hpp include/scanner.hpp include/semantic.hpp include/token.hpp include/validation.hpp include/version.hpp include/workflow.hpp

.PHONY: all test test-vista-link-targets test-hash-unit test-evaluator-scalar demo clean

all: $(TARGET) build/vista-cert-check

build/vista-cert-check: src/cert_check_main.cpp src/certificate.cpp src/hash.cpp src/diagnostic.cpp include/certificate.hpp include/hash.hpp include/diagnostic.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) src/cert_check_main.cpp src/certificate.cpp src/hash.cpp src/diagnostic.cpp -o $@

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
	@$(MAKE) --no-print-directory test-evaluator-scalar
	@bash tests/test_scanner.sh
	@bash tests/test_parser.sh
	@bash tests/test_vista_link_parser.sh
	@bash tests/test_vista_link_module.sh
	@bash tests/test_vista_link_linker.sh
	@bash tests/test_vista_link_evaluator.sh
	@bash tests/test_vista_link_partition.sh
	@bash tests/test_semantic.sh
	@bash tests/test_analysis.sh
	@bash tests/test_codegen.sh
	@bash tests/test_phase2.sh
	@bash tests/test_templates.sh
	@bash tests/test_validation.sh
	@bash tests/test_pipeline.sh

test-vista-link-targets: $(TARGET) build/vista-cert-check
	@bash tests/test_vista_link_codegen.sh
	@bash tests/test_vista_link_certificate.sh

test-hash-unit: build/test_hash_unit
	@./build/test_hash_unit

test-evaluator-scalar: build/test_evaluator_scalar
	@./build/test_evaluator_scalar

build/test_evaluator_scalar: tests/test_evaluator_scalar.cpp src/evaluator.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/test_evaluator_scalar.cpp src/evaluator.cpp -o $@

build/test-partition-unit: tests/test_partition_unit.cpp src/partition.cpp src/evaluator.cpp $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/test_partition_unit.cpp src/partition.cpp src/evaluator.cpp -o $@

build/test_hash_unit: tests/test_hash_unit.cpp src/hash.cpp src/module.cpp src/semantic.cpp src/ast.cpp src/token.cpp include/hash.hpp include/module.hpp include/semantic.hpp include/ast.hpp include/diagnostic.hpp include/token.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/test_hash_unit.cpp src/hash.cpp src/module.cpp src/semantic.cpp src/ast.cpp src/token.cpp -o $@

demo: $(TARGET)
	@echo "VISTA backend-only compiler and data-validation demonstration"
	@echo
	@./$(TARGET) --version
	@echo
	@./$(TARGET) --list-templates
	@echo
	@./$(TARGET) templates/scholarship_application.vista --emit diagnostics
	@./$(TARGET) templates/scholarship_application.vista --validate-data samples/scholarship-valid.data
	@./$(TARGET) templates/scholarship_application.vista --validate-data samples/scholarship-invalid.data || true
	@echo
	@./$(TARGET) examples/hidden_required.vista --emit diagnostics || true

clean:
	@rm -rf -- build out
