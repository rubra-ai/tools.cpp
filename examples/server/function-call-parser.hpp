#include <iostream>
#include <fstream>
#include "json.hpp"
#include <regex>
#include <memory>

using json = nlohmann::ordered_json;


std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 generator(rd());
    static std::uniform_int_distribution<int> distribution(0, 15);

    const char *v = "0123456789abcdef";
    std::stringstream uuid;

    for (int i = 0; i < 8; ++i) {
        uuid << v[distribution(generator)];
    }
    return uuid.str();
}


std::string jsonrepair(const std::string value) {
    std::array<char, 128> buffer;
    std::string result;
    // Ensure the command passed to popen() is null-terminated
    std::string tmpfile_name = "." + generate_uuid() + ".json";
    std::ofstream outfile(tmpfile_name);
    outfile << value; // Assuming jsonStr contains your JSON string
    outfile.close();
    std::string command = "node jsonrepair.ts " + tmpfile_name;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


json parse_if_json(const std::string& value) {
    try {
        // json repair here
        return json::parse(jsonrepair(value));
    } catch (const json::parse_error&) {
        return value;  // Return the original string if parsing fails
    }
}


std::string clean_command_string(const std::string& command_str) {
    std::string cleaned_command = std::regex_replace(command_str, std::regex(R"(\\(?!["\\/bfnrt]|u[a-fA-F0-9]{4}))"), "");
    cleaned_command = std::regex_replace(cleaned_command, std::regex(R"(\\")"), "\"");

    if (cleaned_command.front() == '"' && cleaned_command.back() == '"') {
        cleaned_command = cleaned_command.substr(1, cleaned_command.size() - 2);
    }
    return cleaned_command;
}


json clean_json_strings(const std::string& input_str) {
    try {
        // json repair here
        std::string fixed_str = jsonrepair(input_str);
        json data = json::parse(fixed_str);

        for (auto& [key, value] : data.items()) {
            if (value.is_string()) {
                std::string val = value.get<std::string>();
                if (val.front() == '{' || val.front() == '[') {
                    data[key] = parse_if_json(val);
                } else {
                    data[key] = clean_command_string(val);
                }
            } else if (value.is_object()) {
                for (auto& [k, v] : value.items()) {
                    if (v.is_string()) {
                        v = clean_command_string(v.get<std::string>());
                    }
                }
            }
        }
        return data;
    } catch (const json::parse_error& e) {
        std::cout << "Error decoding JSON: " << e.what() << std::endl;
        return nullptr;
    }
}




std::vector<json> rubra_fc_json_tool_extractor(const std::string& output_str) {
    std::vector<json> result;
    printf("OUTPUT STR TO BE PARSED : %s", output_str.c_str());
    if (output_str.find("endtoolcall") == std::string::npos) {
        return result;
    }

    std::vector<std::string> listOfStrToParse;
    size_t start = 0, end = 0;

    // Iterate until all instances of "endtoolcall" are processed
    while ((end = output_str.find("endtoolcall", start)) != std::string::npos) {
        std::string segment = output_str.substr(start, end - start);
        size_t pos = segment.find("starttoolcall");
        if (pos != std::string::npos) {
            // Extract substring after "toolcall"
            listOfStrToParse.push_back(segment.substr(pos + std::string("starttoolcall").length()));
        }
        start = end + std::string("endtoolcall").length();  // Move past the "endtoolcall"
    }

    std::vector<json> function_call_json;
    
    try {
        for (const auto & line : listOfStrToParse) {
            // json fc = json::parse(line);
            json fc = clean_json_strings(line);
            if (fc["arguments"].is_string()) {
                fc["arguments"] = json::parse(fc["arguments"].get<std::string>());
            }
            if (!fc.is_null()) {
                function_call_json.push_back(fc);
            }
            
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    for (const auto& fc : function_call_json) {
        json func_call;
        func_call["id"] = generate_uuid();
        func_call["name"] = fc["name"];
        func_call["kwargs"] = fc["arguments"];
        func_call["type"] = "function";
        result.push_back(func_call);
    }

    return result;
}