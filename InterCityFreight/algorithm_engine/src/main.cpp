// ============================================================================
//  main.cpp — CLI entry point. Reads a JSON request from stdin, runs the
//  engine, and writes a JSON result to stdout.
//
//  Usage:  freight_engine [data_dir]   (data_dir defaults to "data")
// ============================================================================
#include "engine.hpp"
#include "json.hpp"
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    std::string data_dir = "data";
    if (argc > 1) data_dir = argv[1];

    freight::Engine engine;
    std::string err;
    if (!engine.init(data_dir, err)) {
        minijson::Json e = minijson::Json::object();
        e["status"]  = minijson::Json("error");
        e["message"] = minijson::Json("engine initialisation failed: " + err);
        std::cout << e.dump() << std::endl;
        return 1;
    }

    // Read the entire JSON request from stdin.
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    const std::string input = ss.str();

    minijson::Json req;
    try {
        req = minijson::Json::parse(input);
    } catch (const std::exception& ex) {
        minijson::Json e = minijson::Json::object();
        e["status"]  = minijson::Json("error");
        e["message"] = minijson::Json(std::string("invalid request JSON: ") + ex.what());
        std::cout << e.dump() << std::endl;
        return 1;
    }

    std::cout << engine.handle_request(req) << std::endl;
    return 0;
}