#include<iostream>
#include<string>
#include<memory>
#include<vector>


using std::vector;
using std::unique_ptr;
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
static string identifierStr; // identifier string saved here
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


// THE AST(Abstract Syntax Tree)

// the base class for all nodes of the AST
class ExprAST {
    public:
        virtual ~ExprAST() {}
};

// class for numeric literals
class NumberExprAST : public ExprAST {
  double Val;

    public:
        NumberExprAST(double d) : Val(d) {}
};

// expressions
class VariableExprAST : public ExprAST {
    string Name;

    public:
        VariableExprAST(const string &Name) : Name(Name) {}
};

// binary expressions
class BinaryExprAST : public ExprAST {
    char Op;
    unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char op, unique_ptr<ExprAST> LHS,
                        unique_ptr<ExprAST> RHS)
            : Op(op), LHS(move(LHS)), RHS(move(RHS)) {}
};

// function calls
class CallExprAST : public ExprAST {
    string Callee;
    vector<unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const string &Callee,
                    vector<unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(move(Args)) {}
};

// function prototypes
class PrototypeAST {
    string Name;
    vector<string> Args;

    public:
        PrototypeAST(const string &name, vector<string> Args)
            : Name(name), Args(move(Args)) {}

        const string &getName() const { return Name; }
};

// function definition
class FunctionAST {
    unique_ptr<PrototypeAST> Proto;
    unique_ptr<ExprAST> Body;

    public:
        FunctionAST(unique_ptr<PrototypeAST> Proto,
                    unique_ptr<ExprAST> Body)
            : Proto(move(Proto)), Body(move(Body)) {}
};




// int main(){
//     while(true)
//         cout << "Token: " << getTok() << endl;
//     return 0;
// }

// AST


// compilation and execution
//  g++ lexer.cpp -o lexer.bin; ./lexer.bin