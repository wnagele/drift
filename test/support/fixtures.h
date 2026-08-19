#ifndef DRIFT_TEST_FIXTURES_H
#define DRIFT_TEST_FIXTURES_H

#include <fstream>
#include <sstream>
#include <string>

// Read a fixture file (path relative to test/fixtures/), tolerating the
// working directory the test runner happens to use.
static std::string fixture_read(const std::string &rel) {
    const std::string candidates[] = {
        "test/fixtures/" + rel,
        "../fixtures/" + rel,
        "../../test/fixtures/" + rel,
    };
    for (const auto &path : candidates) {
        std::ifstream f(path);
        if (f.good()) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    return "";
}

#endif
