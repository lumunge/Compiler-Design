#include<iostream>
#include<string>

using std::string;
using std::cout;
using std::endl;

enum Token{
    // end of file token
    tok_eof = -1,

    // def keyword
    tok_def = -2,

    // extern keyword
    tok_extern = -3,

    // function names and variable names
    tok_identifier = -4,

    // numbers
    tok_number = -5,
};

// global variables
static std::string identifierStr; // identifier string saved here
static double numStr; // number saved here

// get tokens, remove white space
static int getTok(){
    static int lastChar = ' '; // empty string

    // remove whitespace
    while(isspace(lastChar))
        lastChar = getchar();

    // recognize identfiers and keywords - gets identifiers
    if(isalpha(lastChar)){ // [a-zA-Z][a-zA-Z0-9] - specifies valid identifiers
        identifierStr = lastChar;
        while(isalnum((lastChar = getchar()))) // while next letter is alphanumeric
            identifierStr += lastChar;
        if(identifierStr == "def") return tok_def; // def keyword, return the corresponding token
        if(identifierStr == "extern") return tok_extern; // extern keyword, ' '
        return tok_identifier; // return identifier token
    }

    // recognizing numbers
    if(isdigit(lastChar) || lastChar == '.'){ // if input is a digit or dot (.)
        string numStr; // hold digits
        do{
            numStr += lastChar; // append to numStr
            lastChar = getchar(); // get next character
        }while(isdigit(lastChar) || lastChar == '-');
        numStr = strtod(numStr.c_str(), 0); // do while numbers/dots are available
        return tok_number; // return number token
    }

    // process comments
    if(lastChar == '#'){ // '#' sign starts comments
        do
            lastChar = getchar();
        while(lastChar != EOF && lastChar != '\n' && lastChar != '\r'); // not end of file, new line or carriage return, read
        
        if(lastChar != EOF) return getTok(); // recursively find other tokens
    }

    // check the end of file
    if(lastChar == EOF) return tok_eof;

    // return character in ASCII code
    int currChar = lastChar;
    lastChar = getchar(); // reset lastChar
    return currChar;
}

int main(){
    while(true)
        cout << "Token: " << getTok() << endl;
    return 0;
}

// compilation and execution
//  g++ lexer.cpp -o lexer.bin; ./lexer.bin