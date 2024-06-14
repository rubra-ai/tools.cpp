#include <iostream>
#include "json.hpp"
#include <string>
#include <vector>

using json = nlohmann::ordered_json;


static std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < vec.size(); i++) {
        result += vec[i];
        if (i < vec.size() - 1) {
            result += delimiter;
        }
    }
    return result;
}

static std::string capitalize(const std::string& str) {
    std::string capitalized = str;
    if (!capitalized.empty()) {
        capitalized[0] = toupper(capitalized[0]);
        for (size_t i = 1; i < capitalized.length(); i++) {
            capitalized[i] = tolower(capitalized[i]);
        }
    }
    return capitalized;
}

static std::string json_schema_to_typescript_type(const json& schema, const std::string& param_name, std::string& enum_comment, std::string& integer_comment, std::string& description_comment);

static std::pair<std::string, std::string> generate_typescript_interface(const json& schema, const std::string& interface_name);

static std::string generate_typescript_function(const json& function_schema);

// Main functions
static std::string json_schema_to_typescript_type(const json& schema, const std::string& param_name, std::string& enum_comment, std::string& integer_comment, std::string& description_comment) {
    std::string ts_type = "any";  // Default type
    enum_comment = "";
    integer_comment = "";
    description_comment = "";

    if (schema.contains("type")) {
        std::string json_type = schema["type"];
        if (json_type == "array") {
            std::string item_type = "any";
            if (schema.contains("items")) {
                item_type = json_schema_to_typescript_type(schema["items"], param_name, enum_comment, integer_comment, description_comment);
            }
            ts_type = item_type + "[]";
        } else if (json_type == "number") {
            ts_type = "number";
        } else if (json_type == "integer") {
            ts_type = "number";
            integer_comment = " * @param " + param_name + " - Integer";
        } else if (json_type == "object") {
            auto [interface_type, _] = generate_typescript_interface(schema, param_name);
            ts_type = interface_type;
        } else if (json_type == "boolean") {
            ts_type = "boolean";
        } else if (json_type == "null") {
            ts_type = "null";
        } else if (json_type == "string") {
            ts_type = "string";
        }
    }

    if (schema.contains("enum")) {
        std::vector<std::string> enum_values;
        for (const auto& val : schema["enum"]) {
            enum_values.push_back("\"" + val.get<std::string>() + "\"");
        }
        enum_comment = " * @enum " + param_name + " - Possible values: " + join(enum_values, ", ");
        ts_type = "string";
    }
    if (schema.contains("description")) {
        description_comment = " * @param " + param_name + " - " + schema["description"].get<std::string>();
    }

    return ts_type;
}

static std::pair<std::string, std::string> generate_typescript_interface(const json& schema, const std::string& interface_name) { 
    json properties = schema.contains("properties") && !schema["properties"].is_null()
                         ? schema["properties"]
                         : json::object();
    std::vector<std::string> required = schema.value("required", std::vector<std::string>());

    std::vector<std::string> interface_body;
    std::vector<std::string> descriptions;
    for (auto& [prop_name, prop_schema] : properties.items()) {
        std::string enum_comment, integer_comment, description_comment;
        std::string prop_type = json_schema_to_typescript_type(prop_schema, prop_name, enum_comment, integer_comment, description_comment);
        bool is_optional = find(required.begin(), required.end(), prop_name) == required.end();
        interface_body.push_back("    " + prop_name + (is_optional ? "?" : "") + ": " + prop_type + ";");
        if (!description_comment.empty()) {
            descriptions.push_back(description_comment);
        }
        if (!enum_comment.empty()) {
            descriptions.push_back(enum_comment);
        }
        if (!integer_comment.empty()) {
            descriptions.push_back(integer_comment);
        }
    }

    std::string comments = join(descriptions, "\n");
    std::string interface_definition = "interface " + interface_name + " {\n" + join(interface_body, "\n") + "\n}";
    return {interface_definition, comments};
}


bool starts_with(const std::string& fullString, const std::string& prefix) {
    return fullString.find(prefix) == 0;
}

static std::string generate_typescript_function(const json& function_schema) {
    std::string func_name = function_schema["name"];
    std::string description = function_schema.value("description", "");
    json parameters_schema = function_schema.contains("parameters") && !function_schema["parameters"].is_null()
                         ? function_schema["parameters"]
                         : json::object();
    std::vector<std::string> required_params = parameters_schema.value("required", std::vector<std::string>());

    std::vector<std::string> args_list;
    std::vector<std::string> comments_list;
    std::vector<std::string> interfaces;

    if (parameters_schema.contains("properties") && parameters_schema["properties"].is_object()){
        for (auto& [param_name, param_schema] : parameters_schema["properties"].items()) {
            std::string enum_comment, integer_comment, description_comment;
            std::string ts_type = json_schema_to_typescript_type(param_schema, param_name, enum_comment, integer_comment, description_comment);
            if (starts_with(ts_type, "interface")) {
                auto [interface_definition, nested_comments] = generate_typescript_interface(param_schema, func_name + "_" + capitalize(param_name) + "Params");
                interfaces.push_back(interface_definition);
                comments_list.push_back(nested_comments);
                ts_type = func_name + "_" + capitalize(param_name) + "Params";
            } else {
                if (!description_comment.empty()) {
                    comments_list.push_back(description_comment);
                }
                if (!enum_comment.empty()) {
                    comments_list.push_back(enum_comment);
                }
                if (!integer_comment.empty()) {
                    comments_list.push_back(integer_comment);
                }
            }
            bool is_optional = find(required_params.begin(), required_params.end(), param_name) == required_params.end();
            args_list.push_back(param_name + (is_optional ? "?" : "") + ": " + ts_type);
        }
    }
    

    std::string args_str = join(args_list, ", ");
    std::string comments_str = join(comments_list, "\n");
    std::string interfaces_str = join(interfaces, "\n\n");

    std::string description_comment = (!description.empty()) ? " * " + description + "\n" : "";
    std::string typescript_func_declaration =
        "/**\n" +
        description_comment +
        (comments_str.empty() ? "" : (comments_str + "\n")) +
        " */\n" +
        (interfaces_str.empty() ? "" : (interfaces_str + "\n\n")) +
        "function " + func_name + "(" + args_str + "): any {};";

    return typescript_func_declaration;
}



std::string rubra_format_typescript_function_call_str(const std::vector<json> &functions, json &tool_name_map) {
    std::string final_str = "You have access to the following tools:\n";
    std::vector<std::string> function_definitions;
    for (auto& function : functions) {
        // If function is directly the object or nested under "function"
        json spec = function.contains("function") ? function["function"] : function;

        // Making a modifiable copy of spec
        json spec_copy = spec;

        std::string func_name = spec_copy.value("name", "");

        if (func_name.find('-') != std::string::npos) {
            const std::string origin_func_name = func_name;
            std::replace(func_name.begin(), func_name.end(), '-', '_'); // replace "-" with "_" because - is invalid in typescript function name
            tool_name_map[func_name] = origin_func_name;
            spec_copy["name"] = func_name; // Modify the name in the copied json object
        }
        
        std::string res_string = generate_typescript_function(spec_copy);  // generate TypeScript function
        function_definitions.push_back(res_string);
    }


    for (const auto& def : function_definitions) {
        final_str += def + "\n\n";
    }
    final_str += "You can choose to respond with one or more tool calls at once, or with a chat message back to the user. Ensure you have all necessary details before making tool calls. If additional information is needed, ask the user appropriately. Any tool call you make must correspond to the functions listed above. If you decide to call a tool, format it like this: starttoolcall{\"name\": \"<function_name>\", \"arguments\": {\"<arg1_name>\": \"<arg1_value>\", \"<arg2_name>\": \"<arg2_value>\", ...}}endtoolcall where the JSON wrapped between starttoolcall and endtoolcall represents the function call.\n";
    return final_str;

}


std::string construct_json_tool_call_str(const json& tool_calls, nlohmann::ordered_map<std::string, std::string> & func_observation_map) {
    std::string tool_call_str;
    bool first = true;
    for (const auto& tool_call : tool_calls) {
        std::string tool_call_id = tool_call["id"];
        func_observation_map[tool_call_id] = "";  // Initialize with empty value, updated later from the message with tool role
        
        if (!first) {
            tool_call_str += "\n";
        }
        json tc = tool_call["function"];
        if (tc["arguments"].is_string()) {
            tc["arguments"] = json::parse(tc["arguments"].get<std::string>());
        }
        tool_call_str += std::string("starttoolcall") + tc.dump() + std::string("endtoolcall");
        first = false;
    }
    
    return tool_call_str;
}


const std::vector<json> expand_messages(const json & body, json &tool_name_map) {
    std::string function_str = "";
    if (body.contains("tools") && !body["tools"].empty()) {
        function_str = rubra_format_typescript_function_call_str(body["tools"], tool_name_map);
    }
    // If 'tool' is not set or empty, check 'functions'
    else if (body.contains("functions") && !body["functions"].empty()) {
       function_str = rubra_format_typescript_function_call_str(body["functions"], tool_name_map);
    }
    
    if (function_str != "") {
        printf("\n=============Formatting function call Input from OPENAI format...============\n");
        const std::vector<json> expanded_messages = [&]() {
            std::vector<json> temp_vec;
            nlohmann::ordered_map<std::string, std::string> func_observation_map;
            for (size_t i = 0; i < body["messages"].size(); ++i) {
                if (body["messages"][i]["role"] != "tool" and func_observation_map.size() > 0) {
                    // insert the observation from the tool call before the next message
                    std::string observation_str = "";
                    std::vector<std::string> func_observation_array;
                    for (const auto& [key, value] : func_observation_map) {
                        func_observation_array.push_back(value);
                    }
                    json func_json_array = func_observation_array;
                    observation_str = std::string("start observation ") + func_json_array.dump() + std::string(" end observation");
                    json observation_call;
                    observation_call["role"] = "user";
                    observation_call["content"] = observation_str;
                    temp_vec.push_back(observation_call);
                    func_observation_map.clear();
                }

                if (i == 0){
                    if (body["messages"][0]["role"] == "system") {
                        std::string old_content = body["messages"][0]["content"];
                        json function_call;
                        function_call["role"] = "system";
                        function_call["content"] = old_content + "\n" + function_str;
                        temp_vec.push_back(function_call);
                    }
                    else { // insert a system message of tool definition before the first message
                        json function_call;
                        function_call["role"] = "system";
                        function_call["content"] = "You are a helpful assistant.\n" + function_str;
                        temp_vec.push_back(function_call);
                        temp_vec.push_back(body["messages"][0]);
                    }
                }
                // else if (body["messages"][i]["role"] == "assistant" and (body["messages"][i]["content"].is_null() or body["messages"][i]["content"]=="") and !body["messages"][i]["tool_calls"].is_null() and !body["messages"][i]["tool_calls"].empty()){
                else if (body["messages"][i]["role"] == "assistant" and body["messages"][i].contains("tool_calls")){
                    // convert OpenAI function call format to Rubra format
                    // string tool_call_str = construct_python_tool_call_str(body["messages"][i]["tool_calls"], func_observation_map);
                    std::string tool_call_str = construct_json_tool_call_str(body["messages"][i]["tool_calls"], func_observation_map);
                    json function_call;
                    function_call["role"] = "assistant";
                    function_call["content"] = tool_call_str;
                    temp_vec.push_back(function_call);
                }
                else if (body["messages"][i]["role"] == "tool") {
                    std::string tool_call_id = body["messages"][i]["tool_call_id"].get<std::string>();
                    if (func_observation_map.find(tool_call_id) != func_observation_map.end()) {
                        func_observation_map[tool_call_id] = body["messages"][i]["content"].get<std::string>();
                    } else {
                        printf("Tool call id not found in the map : %s", tool_call_id.c_str());
                        // TODO: the input is not valid in this case, should return an error
                    }

                }
                else {
                    temp_vec.push_back(body["messages"][i]);
                }
                
            }
            if (func_observation_map.size() > 0) {
                // insert the observation from the tool call before the next message
                std::string observation_str = "";
                std::vector<std::string> func_observation_array;
                for (const auto& [key, value] : func_observation_map) {
                    func_observation_array.push_back(value);
                }
                json func_json_array = func_observation_array;
                observation_str = std::string("start observation ") + func_json_array.dump() + std::string(" end observation");
                json observation_call;
                observation_call["role"] = "user";
                observation_call["content"] = observation_str;
                temp_vec.push_back(observation_call);
                func_observation_map.clear();
            }
            return temp_vec;
        }();
        return expanded_messages;
    }
    else {
        return body["messages"];
    }
    
}