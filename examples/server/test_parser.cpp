#include "python-parser.hpp"
#include "json.hpp"

using json = nlohmann::json;
int main() {
    std::vector<json> parsed_content = parsePythonFunctionCalls("<<functions>>[find(directory='.', pattern='*.go'), count(file='file1.go'), count(file='file2.go'), count(file='file3.go'), count(file='file4.go'), count(file='file5.go'), count(file='file6.go'), count(file='file7.go'), count(file='file8.go'), count(file='file9.go'), count(file='file10.go')]");
    printf("parsed_content: %s\n", json(parsed_content).dump().c_str());
    return 0;
}