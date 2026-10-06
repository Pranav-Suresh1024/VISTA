#include "module.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <tuple>
#include <utility>

#include "hash.hpp"
#include "semantic.hpp"

namespace vista {
namespace {

std::string normalize_newlines(std::string_view source) {
    std::string normalized;
    normalized.reserve(source.size());
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (source[index] == '\r') {
            normalized.push_back('\n');
            if (index + 1U < source.size() && source[index + 1U] == '\n') {
                ++index;
            }
        } else {
            normalized.push_back(source[index]);
        }
    }
    return normalized;
}

std::string object_prefix(const ModuleObject& source_object) {
    ModuleObject object = source_object;
    std::sort(object.imports.begin(), object.imports.end(),
              [](const ImportContract& left, const ImportContract& right) {
                  return std::tie(left.provider, left.field, left.alias) <
                         std::tie(right.provider, right.field, right.alias);
              });
    std::sort(object.exports.begin(), object.exports.end(),
              [](const ExportContract& left, const ExportContract& right) {
                  return std::tie(left.field, left.type, left.options) <
                         std::tie(right.field, right.type, right.options);
              });

    std::ostringstream output;
    output << "VISTA-OBJECT " << object.format_version << '\n'
           << "module " << object.name << '\n'
           << "source-sha256 " << object.source_sha256 << '\n'
           << "imports " << object.imports.size() << '\n';
    for (const ImportContract& import : object.imports) {
        output << "import " << import.provider << ' ' << import.field << ' '
               << import.alias << '\n';
    }
    output << "exports " << object.exports.size() << '\n';
    for (const ExportContract& export_contract : object.exports) {
        output << "export " << export_contract.field << ' '
               << field_type_name(export_contract.type) << ' '
               << export_contract.options.size();
        for (const std::string& option : export_contract.options) {
            output << ' ' << option;
        }
        output << '\n';
    }
    output << "source-bytes " << object.canonical_source.size() << '\n'
           << object.canonical_source << '\n';
    return output.str();
}

bool is_sha256(const std::string& value) {
    return value.size() == 64U &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

bool parse_size(const std::string& text, std::size_t& value) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool read_line(std::string_view bytes, std::size_t& offset, std::string& line) {
    const std::size_t end = bytes.find('\n', offset);
    if (end == std::string_view::npos) {
        return false;
    }
    line.assign(bytes.substr(offset, end - offset));
    offset = end + 1U;
    return line.find('\r') == std::string::npos;
}

bool read_key_value(std::string_view bytes,
                    std::size_t& offset,
                    const std::string& key,
                    std::string& value) {
    std::string line;
    if (!read_line(bytes, offset, line) || line.rfind(key + ' ', 0) != 0U) {
        return false;
    }
    value = line.substr(key.size() + 1U);
    return !value.empty();
}

bool parse_field_type(const std::string& text, FieldType& type) {
    static const std::pair<const char*, FieldType> types[] = {
        {"text", FieldType::Text}, {"integer", FieldType::Integer},
        {"decimal", FieldType::Decimal}, {"boolean", FieldType::Boolean},
        {"date", FieldType::Date}, {"choice", FieldType::Choice},
        {"file", FieldType::File}, {"email", FieldType::Email},
        {"phone", FieldType::Phone}, {"textarea", FieldType::Textarea}
    };
    for (const auto& entry : types) {
        if (text == entry.first) {
            type = entry.second;
            return true;
        }
    }
    return false;
}

ModuleLoadResult invalid_object() {
    ModuleLoadResult result;
    result.diagnostics.push_back(
        {"MOD004", "invalid or tampered module object", 1, 1});
    return result;
}

}  // namespace

bool ModuleCompileResult::succeeded() const {
    return diagnostics.empty();
}

bool ModuleLoadResult::succeeded() const {
    return diagnostics.empty();
}

ModuleCompileResult compile_module(const ModuleAst& module,
                                   std::string_view canonical_source) {
    ModuleCompileResult result;
    result.object.name = module.name;
    result.object.canonical_source = normalize_newlines(canonical_source);
    result.object.source_sha256 = sha256_hex(result.object.canonical_source);

    const SemanticResult semantics = analyze_semantics(module);
    result.diagnostics = semantics.diagnostics;
    for (const ImportDecl* import : module.imports) {
        result.object.imports.push_back(
            {import->provider, import->field, import->alias, import->span});
    }
    for (const ExportDecl* export_decl : module.exports) {
        const Symbol* symbol = semantics.symbols.find(export_decl->field);
        if (symbol == nullptr) {
            result.diagnostics.push_back(
                {"MOD002", "exported field '" + export_decl->field +
                               "' is not declared locally",
                 export_decl->span.first_line, export_decl->span.first_column});
            continue;
        }
        result.object.exports.push_back(
            {symbol->name, symbol->type, symbol->options});
    }

    std::sort(result.object.imports.begin(), result.object.imports.end(),
              [](const ImportContract& left, const ImportContract& right) {
                  return std::tie(left.provider, left.field, left.alias) <
                         std::tie(right.provider, right.field, right.alias);
              });
    std::sort(result.object.exports.begin(), result.object.exports.end(),
              [](const ExportContract& left, const ExportContract& right) {
                  return std::tie(left.field, left.type, left.options) <
                         std::tie(right.field, right.type, right.options);
              });
    if (result.succeeded()) {
        result.object.object_sha256 = sha256_hex(object_prefix(result.object));
    }
    return result;
}

std::string serialize_module_object(const ModuleObject& object) {
    const std::string prefix = object_prefix(object);
    return prefix + "object-sha256 " + sha256_hex(prefix) + '\n';
}

ModuleLoadResult load_module_object(std::string_view bytes) {
    ModuleLoadResult result;
    std::size_t offset = 0;
    std::string value;
    if (!read_key_value(bytes, offset, "VISTA-OBJECT", value) || value != "1") {
        return invalid_object();
    }
    result.object.format_version = 1;
    if (!read_key_value(bytes, offset, "module", result.object.name) ||
        !read_key_value(bytes, offset, "source-sha256", result.object.source_sha256) ||
        !is_sha256(result.object.source_sha256)) {
        return invalid_object();
    }

    std::size_t import_count = 0;
    if (!read_key_value(bytes, offset, "imports", value) ||
        !parse_size(value, import_count)) {
        return invalid_object();
    }
    for (std::size_t index = 0; index < import_count; ++index) {
        std::string line;
        ImportContract import;
        std::string extra;
        if (!read_line(bytes, offset, line)) {
            return invalid_object();
        }
        std::istringstream input(line);
        std::string key;
        if (!(input >> key >> import.provider >> import.field >> import.alias) ||
            key != "import" || (input >> extra)) {
            return invalid_object();
        }
        result.object.imports.push_back(std::move(import));
    }

    std::size_t export_count = 0;
    if (!read_key_value(bytes, offset, "exports", value) ||
        !parse_size(value, export_count)) {
        return invalid_object();
    }
    for (std::size_t index = 0; index < export_count; ++index) {
        std::string line;
        if (!read_line(bytes, offset, line)) {
            return invalid_object();
        }
        std::istringstream input(line);
        std::string key;
        std::string type;
        std::size_t option_count = 0;
        ExportContract export_contract;
        if (!(input >> key >> export_contract.field >> type >> option_count) ||
            key != "export" || !parse_field_type(type, export_contract.type)) {
            return invalid_object();
        }
        for (std::size_t option = 0; option < option_count; ++option) {
            std::string name;
            if (!(input >> name)) {
                return invalid_object();
            }
            export_contract.options.push_back(std::move(name));
        }
        std::string extra;
        if (input >> extra) {
            return invalid_object();
        }
        result.object.exports.push_back(std::move(export_contract));
    }

    std::size_t source_size = 0;
    if (!read_key_value(bytes, offset, "source-bytes", value) ||
        !parse_size(value, source_size) || source_size > bytes.size() - offset) {
        return invalid_object();
    }
    result.object.canonical_source.assign(bytes.substr(offset, source_size));
    if (result.object.canonical_source.find('\r') != std::string::npos) {
        return invalid_object();
    }
    offset += source_size;
    if (offset >= bytes.size() || bytes[offset] != '\n') {
        return invalid_object();
    }
    ++offset;
    const std::size_t prefix_size = offset;
    if (!read_key_value(bytes, offset, "object-sha256", result.object.object_sha256) ||
        !is_sha256(result.object.object_sha256) || offset != bytes.size() ||
        result.object.source_sha256 != sha256_hex(result.object.canonical_source) ||
        result.object.object_sha256 != sha256_hex(bytes.substr(0, prefix_size)) ||
        serialize_module_object(result.object) != bytes) {
        return invalid_object();
    }
    return result;
}

}  // namespace vista
