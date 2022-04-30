#include<iostream>
#include<string>
#include<memory>
#include<vector>
#include<map>


using std::vector;
using std::unique_ptr;
using std::map;
using std::make_unique;
using std::move;
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

namespace {
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
}

// THE PARSER
static int currTok; // current token
static int getNextToken() { 
    return currTok = getTok();
}

// PARSING BINARY EXPRESSIONS
static map<char, int> BinopPrecedence;

// get the precedence of the pending binary operator token.
static int GetTokPrecedence(){
    switch(currTok){
        case '<':
        case '>':
            return 10;
        case '+':
        case '-':
            return 20;
        case '*':
        case '/':
            return 40; // highest precedence
        default:
            return -1;
    }
}


// error reporting for expressions
void LogError(const char *Str) {
    fprintf(stderr, "LogError: %s\n", Str); // print error
    // return nullptr; // null pointer
}

// error reporting for function prototypes
// unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
//     LogError(Str); // print error
//     // return nullptr;
// }

static unique_ptr<ExprAST> ParseExpression();

// PARSING NUMBER EXPRESSIONS
static unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = make_unique<NumberExprAST>(numStr); // create and allocate
    getNextToken(); // consume the number
    return move(Result);
}

// PARSING PARENTHESIS EXPRESSIONS
static unique_ptr<ExprAST> ParseParenExpr(){
    getNextToken(); // eat (. --> we expect '(' to come first
    auto V = ParseExpression();
    if (!V) return nullptr; // the above statement failed
    
    if(currTok == ')'){ // if we previously ate '(' we expect ')'
        getNextToken(); // eat ).
        return V; // return expression
    }else{
        LogError("expected ')'"); // not got what was expected
        return nullptr;
    }
    return V;
}

// PARSING IDENTIFIERS AND FUNCTION CALL EXPRESSIONS
static unique_ptr<ExprAST> ParseIdentifierOrCallExpr(){
    string IdName = identifierStr;

    getNextToken();  // eat identifier.

    if(currTok == '('){ // parse args list
        getNextToken();  // eat (. -> eat open paren
        vector<unique_ptr<ExprAST>> Args; // strore arguments
        while(1){
            auto Arg = ParseExpression();
            if (Arg)
                Args.push_back(move(Arg)); // push to vector
            else
                return nullptr; // return null pointer

            if(currTok == ')'){
                getNextToken(); // eat
                break;
            }else if(currTok == ','){ // more arguments
                getNextToken(); //eat
                continue;
            }else{ // report errors
                LogError("Expected ')' or ',' in argument list \n");
            }
        }
    }
    return make_unique<VariableExprAST>(IdName);
}

// PARSING PRIMARIES
static unique_ptr<ExprAST> ParsePrimary() {
    switch (currTok) {
        case tok_identifier: // identifiers
            return ParseIdentifierOrCallExpr(); // parse identifier
        case tok_number: // number literal
            return ParseNumberExpr(); // parse number literal
        case '(': // parenthesis
            return ParseParenExpr(); // parse parenthesis
        default: // report error
            LogError("Unknown token. expected an expression \n");
            return nullptr;
    }
}

// PARSE RIGHT-HAND SIDE
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS){
    // If this is a binop, find its precedence.
    while (1) { // keep parsing right hand side
        int TokPrec = GetTokPrecedence(); // get precedence

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (TokPrec < ExprPrec) // precedence is < than curr precedence
            return LHS; // return left-hand side
        else{
            int BinOp = currTok;
            getNextToken();  // eat binop

            // Parse the primary expression after the binary operator.
            auto RHS = ParsePrimary(); // parse right-hand side
            if(RHS){
                int NextPrec = GetTokPrecedence();
                if(TokPrec < NextPrec){ // get next
                    RHS = ParseBinOpRHS(TokPrec+1, move(RHS));
                    if(!RHS) return nullptr;
                }
                // merge curr LHS, curr RHS to make a new binary expression AST as new LHS
                LHS = make_unique<BinaryExprAST>(BinOp, move(LHS), move(RHS));
            }else return nullptr;
            
        }
    }
}

// PARSE BINARY EXPRESSION
static unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();

    if(LHS){
        return ParseBinOpRHS(0, move(LHS)); // parse left side
    }else return nullptr;
}

// PARSING FUNCTION PROTOTYPES - function signature
static unique_ptr<PrototypeAST> ParsePrototype(){
    if (currTok != tok_identifier){ // current token, not token identfier
        LogError("Expected function name in prototype \n"); // report error
        return nullptr;
    }

    string fnName = identifierStr;
    getNextToken(); // eat identifier

    if(currTok != '('){ // report error
        LogError("Expected '(' in prototype \n");
         return nullptr;
    }

    // Read the list of argument names.
    vector<string> argNames; // srore argument names
    while (getNextToken() == tok_identifier)
        argNames.push_back(identifierStr); // add to vector
    if(currTok != ')'){ // report error
        LogError("Expected ')' in prototype \n");
         return nullptr;
    }

    // success.
    getNextToken();  // eat ')'.

    return make_unique<PrototypeAST>(fnName, move(argNames)); // unique pointer to a prototype AST
}

// PARSING FUNCTION DEFINITIONS
static unique_ptr<FunctionAST> ParseDefinition(){
    getNextToken();  // eat 'def' token
    auto Proto = ParsePrototype();
    if(!Proto) return nullptr;

    auto E = ParseExpression();
    if(E) return make_unique<FunctionAST>(move(Proto), move(E)); // unique pointer to a new function AST

    return nullptr; // otherwise return null pointer
}

// PARSING THE EXTERN KEYWORD
static unique_ptr<PrototypeAST> ParseExtern(){
    getNextToken();  // eat extern token
    return ParsePrototype();
}


// PARSING TOP-LEVEL EXPRESSIONS
static unique_ptr<FunctionAST> ParseTopLevelExpr(){
    auto E = ParseExpression();
    if(E){
        // Make an anonymous proto.
        auto proto = make_unique<PrototypeAST>("", vector<string>());
        return make_unique<FunctionAST>(move(proto), move(E));
    }
    return nullptr;
}

// TOP_LEVEL PARSING
static void handleDefinition(){
    if(ParseDefinition()) fprintf(stderr, "Parsed a function definition. \n");
    else getNextToken();
}

static void handleExtern(){
    if(ParseExtern()) fprintf(stderr, "Parsed an extern. \n");
    else getNextToken();
}

static void handleTopLevelExpression(){
    if(ParseTopLevelExpr()) fprintf(stderr, "Parsed a top-level expression. \n");
    else getNextToken();
}



// DRIVER CODE - repl
static void run(){
  while (1) {
    fprintf(stderr, "ready> ");
    switch(currTok){
        case tok_eof:
            return;
        case ';': // ignore top-level semicolons.
            getNextToken();
        break;
        case tok_def:
            handleDefinition();
        break;
        case tok_extern:
            handleExtern();
        break;
        default:
            handleTopLevelExpression();
        break;
    }
  }
}
int main(){
    // test lexer
    // while(true)
    //     cout << "Token: " << getTok() << endl;
    // test parser
      // Prime the first token.
    fprintf(stderr, "ready> ");
    getNextToken();
    run();
    return 0;
}

// compilation and execution
// clang++  main.cpp -o main.bin
// ./main.bin