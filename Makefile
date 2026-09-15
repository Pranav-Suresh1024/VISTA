CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude

TARGET := build/vista
SOURCES := src/main.cpp
HEADERS := include/version.hpp

.PHONY: all test demo clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

test: $(TARGET)
	@version_output="$$(./$(TARGET) --version)"; \
		test "$$version_output" = "VISTA 0.1.0" || { \
			echo "FAIL: unexpected version output: $$version_output"; \
			exit 1; \
		}
	@./$(TARGET) --help | grep -q "Usage: vista" || { \
		echo "FAIL: help output does not contain the usage line"; \
		exit 1; \
	}
	@./$(TARGET) --unknown >/dev/null 2>&1; status=$$?; \
		test $$status -eq 2 || { \
			echo "FAIL: unknown option returned $$status instead of 2"; \
			exit 1; \
		}
	@echo "Stage 1 tests passed."

demo: $(TARGET)
	@echo "VISTA Stage 1 demonstration"
	@echo
	@./$(TARGET) --version
	@echo
	@./$(TARGET) --help

clean:
	@rm -rf -- build out
