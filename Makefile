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

HEADERS += include/file_io.hpp

.PHONY: all test test-vista-link-targets test-vista-link-final-fixes test-hash-unit test-evaluator-scalar demo vista-link-demo clean

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
	@$(MAKE) --no-print-directory test-vista-link-final-fixes
	@bash tests/test_vista_link_codegen.sh
	@bash tests/test_vista_link_certificate.sh
	@bash tests/test_vista_link_mutations.sh
	@bash tests/test_pipeline.sh --vista-link

test-vista-link-final-fixes: $(TARGET) build/vista-cert-check
	@bash tests/test_vista_link_final_fixes.sh

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

vista-link-demo: $(TARGET) build/vista-cert-check
	@./$(TARGET) examples/vista_link/scholarship-demo.workflow.vista --emit certificate --out-dir out/vista-link-demo/valid
	@./build/vista-cert-check out/vista-link-demo/valid/workflow.vcert --root .
	@printf 'Applicant.category = general\nEligibility.age = 18\n' > out/vista-link-demo/general.data
	@printf 'Applicant.category = reserved\nEligibility.age = 18\nCertificate.certificate = certificate.pdf\n' > out/vista-link-demo/reserved.data
	@./$(TARGET) examples/vista_link/scholarship-demo.workflow.vista --validate-data out/vista-link-demo/general.data
	@./$(TARGET) examples/vista_link/scholarship-demo.workflow.vista --validate-data out/vista-link-demo/reserved.data
	@node -e 'const fs=require("node:fs"),assert=require("node:assert/strict"),q=s=>"\""+[...Buffer.from(s)].map(b=>String.fromCharCode(92)+"u00"+b.toString(16).padStart(2,"0")).join("")+"\"";const js=fs.readFileSync("out/vista-link-demo/valid/workflow.js","utf8"),from=q("18"),to=q("19");assert.equal(js.split(from).length-1,1);fs.writeFileSync("out/vista-link-demo/mutant.js",js.replace(from,to));'
	@code=0; ./$(TARGET) examples/vista_link/scholarship-demo.workflow.vista --emit certificate --out-dir out/vista-link-demo/mutated --javascript out/vista-link-demo/mutant.js || code=$$?; test "$$code" = 1
	@node -e 'const fs=require("node:fs"),assert=require("node:assert/strict"),s=fs.readFileSync("out/vista-link-demo/mutated/workflow.vcert","utf8");assert(s.includes("\nstatus FAIL\n"));for(const key of ["counterexample","counterexample-cli","counterexample-js"]){const line=s.split("\n").find(l=>l.startsWith(key+" "));assert(line&&!line.endsWith(" -"));console.log(key+":\n"+decodeURIComponent(line.slice(key.length+1)));}console.log("Detected age-boundary mutation (expected FAIL); demonstration passed.");'

clean:
	@rm -rf -- build out
