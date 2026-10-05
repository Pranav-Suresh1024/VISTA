#ifndef VISTA_MODULE_HPP
#define VISTA_MODULE_HPP

#include <string>
#include <string_view>
#include <vector>

#include "ast.hpp"
#include "diagnostic.hpp"

namespace vista {

struct ImportContract {
    std::string provider;
    std::string field;
    std::string alias;
    SourceSpan span;
};

struct ExportContract {
    std::string field;
    FieldType type;
    std::vector<std::string> options;
};

struct ModuleObject {
    unsigned format_version = 1;
    std::string name;
    std::string source_sha256;
    std::vector<ImportContract> imports;
    std::vector<ExportContract> exports;
    std::string canonical_source;
    std::string object_sha256;
};

struct ModuleCompileResult {
    ModuleObject object;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const;
};

struct ModuleLoadResult {
    ModuleObject object;
    std::vector<Diagnostic> diagnostics;
    bool succeeded() const;
};

ModuleCompileResult compile_module(const ModuleAst& module,
                                   std::string_view canonical_source);
std::string serialize_module_object(const ModuleObject& object);
ModuleLoadResult load_module_object(std::string_view bytes);

}  // namespace vista

#endif
