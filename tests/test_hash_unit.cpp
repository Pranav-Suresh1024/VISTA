#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "hash.hpp"
#include "module.hpp"

int main() {
    int failures = 0;
    const auto expect = [&failures](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };

    expect(vista::sha256_hex("") ==
               "e3b0c44298fc1c149afbf4c8996fb924"
               "27ae41e4649b934ca495991b7852b855",
           "SHA-256 empty-string vector");
    expect(vista::sha256_hex("abc") ==
               "ba7816bf8f01cfea414140de5dae2223"
               "b00361a396177a9cb410ff61f20015ad",
           "SHA-256 abc vector");

    vista::ModuleObject object;
    object.name = "RoundTrip";
    object.canonical_source = "module RoundTrip {}\n";
    object.source_sha256 = vista::sha256_hex(object.canonical_source);
    const std::string serialized = vista::serialize_module_object(object);
    const vista::ModuleLoadResult round_trip = vista::load_module_object(serialized);
    expect(round_trip.succeeded(), "valid object must load");
    expect(round_trip.object.name == "RoundTrip" &&
               round_trip.object.canonical_source == object.canonical_source,
           "valid object must preserve identity and source");

    std::string changed = serialized;
    const std::size_t payload =
        changed.find("module RoundTrip {}", changed.find("source-bytes"));
    expect(payload != std::string::npos, "serialized object must contain its payload");
    if (payload != std::string::npos) {
        changed[payload] = 'M';
        const vista::ModuleLoadResult changed_load = vista::load_module_object(changed);
        expect(!changed_load.succeeded() && changed_load.diagnostics.size() == 1 &&
                   changed_load.diagnostics[0].code == "MOD004",
               "changed payload must report MOD004");
    }

    std::ifstream input("examples/vista_link/errors/tampered.vobj", std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
    const vista::ModuleLoadResult loaded = vista::load_module_object(bytes);
    expect(!loaded.succeeded(), "tampered object must fail closed");
    expect(loaded.diagnostics.size() == 1 && loaded.diagnostics[0].code == "MOD004",
           "tampered object must report MOD004");

    if (failures != 0) {
        return 1;
    }
    std::cout << "SHA-256 and module object load tests passed.\n";
    return 0;
}
