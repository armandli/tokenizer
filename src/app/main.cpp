#include <iostream>
#include <sstream>
#include <string>

#include "tokenizer.h"

int main(int argc, char** argv) {
    std::string text;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (i > 1) {
                text += ' ';
            }
            text += argv[i];
        }
    } else {
        std::ostringstream buffer;
        buffer << std::cin.rdbuf();
        text = buffer.str();
    }

    tokenizer::Tokenizer tok;
    for (const auto& token : tok.tokenize(text)) {
        std::cout << token << '\n';
    }

    return 0;
}
