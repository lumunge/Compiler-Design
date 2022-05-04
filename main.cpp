// #include "../include/KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <map>

// ADDED
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
// ADDED

using namespace llvm;
using std::cout;
using std::endl;
using std::make_unique;
using std::map;
using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

enum Token
{
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
static double numVal;        // number saved here

// get tokens, remove white space
static int getTok()
{
    static int lastChar = ' '; // empty string

    // remove whitespace
    while (isspace(lastChar))
        lastChar = getchar();

    // recognize identfiers and keywords - gets identifiers
    if (isalpha(lastChar))
    { // [a-zA-Z][a-zA-Z0-9] - specifies valid identifiers
        identifierStr = lastChar;
        while (isalnum((lastChar = getchar()))) // while next letter is alphanumeric
            identifierStr += lastChar;
        if (identifierStr == "def")
            return tok_def; // def keyword, return the corresponding token
        if (identifierStr == "extern")
            return tok_extern; // extern keyword, ' '
        return tok_identifier; // return identifier token
    }

    // recognizing numbers
    if (isdigit(lastChar) || lastChar == '.')
    {                  // if input is a digit or dot (.)
        string numStr; // hold digits
        do
        {
            numStr += lastChar;   // append to numStr
            lastChar = getchar(); // get next character
        } while (isdigit(lastChar) || lastChar == '.');
        numVal = strtod(numStr.c_str(), nullptr); // do while numbers/dots are available
        return tok_number;                        // return number token
    }

    // process comments
    if (lastChar == '#')
    { // '#' sign starts comments
        do
            lastChar = getchar();
        while (lastChar != EOF && lastChar != '\n' && lastChar != '\r'); // not end of file, new line or carriage return, read

        if (lastChar != EOF)
            return getTok(); // recursively find other tokens
    }

    // check the end of file
    if (lastChar == EOF)
        return tok_eof;

    // return character in ASCII code
    int currChar = lastChar;
    lastChar = getchar(); // reset lastChar
    return currChar;
}

// THE AST(Abstract Syntax Tree)

namespace // anonymous namespace
{
    // the base class for all nodes of the AST
    class ExprAST
    {
    public:
        virtual ~ExprAST() {}
        // virtual implementation not implemented = 0
        virtual Value *codegen() = 0;
    };

    // class for numeric literals
    class NumberExprAST : public ExprAST
    {
        double Val;

    public:
        NumberExprAST(double d) : Val(d) {}
        virtual Value *codegen();
    };

    // expressions
    class VariableExprAST : public ExprAST
    {
        string Name;

    public:
        VariableExprAST(const string &Name) : Name(Name) {}
        virtual Value *codegen();
    };

    // binary expressions
    class BinaryExprAST : public ExprAST
    {
        char Op;
        unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char Op, unique_ptr<ExprAST> LHS,
                      unique_ptr<ExprAST> RHS)
            : Op(Op), LHS(move(LHS)), RHS(move(RHS)) {}
        virtual Value *codegen();
    };

    // function calls
    class CallExprAST : public ExprAST
    {
        string Callee;
        vector<unique_ptr<ExprAST>> Args;

    public:
        CallExprAST(const string &Callee,
                    vector<unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(move(Args)) {}
        virtual Value *codegen();
    };

    // function prototypes
    class PrototypeAST
    {
        string Name;
        vector<string> Args;

    public:
        PrototypeAST(const string &name, vector<string> Args)
            : Name(name), Args(move(Args)) {}
        Function *codegen();
        const string &getName() const { return Name; }
    };

    // function definition
    class FunctionAST
    {
        unique_ptr<PrototypeAST> Proto;
        unique_ptr<ExprAST> Body;

    public:
        FunctionAST(unique_ptr<PrototypeAST> Proto,
                    unique_ptr<ExprAST> Body)
            : Proto(move(Proto)), Body(move(Body)) {}
        Function *codegen();
    };
} // end anonymous namespace

// THE PARSER
static int currTok; // current token
static int getNextToken()
{
    return currTok = getTok();
}

// PARSING BINARY EXPRESSIONS
static map<char, int> BinopPrecedence;

// get the precedence of the pending binary operator token.
static int getTokPrecedence()
{
    switch (currTok)
    {
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
void LogError(const char *Str)
{
    fprintf(stderr, "LogError: %s\n", Str); // print error
}

static unique_ptr<ExprAST> ParseExpression();

// PARSING NUMBER EXPRESSIONS
static unique_ptr<ExprAST> ParseNumberExpr()
{
    auto Result = make_unique<NumberExprAST>(numVal); // create and allocate
    getNextToken();                                   // consume the number
    return move(Result);
}

// PARSING PARENTHESIS EXPRESSIONS
static unique_ptr<ExprAST> ParseParenExpr()
{
    getNextToken(); // eat (. --> we expect '(' to come first
    auto V = ParseExpression();
    if (!V)
        return nullptr; // the above statement failed

    if (currTok != ')') // if we previously ate '(' we expect ')'
    {
        LogError("expected ')'"); // not got what was expected
        return nullptr;
    }
    getNextToken(); // eat ).
    return V;       // return expression
}

// PARSING IDENTIFIERS AND FUNCTION CALL EXPRESSIONS
static unique_ptr<ExprAST> ParseIdentifierOrCallExpr()
{
    string idName = identifierStr;

    getNextToken(); // eat identifier.

    if (currTok != '(') // Simple variable ref.
        return make_unique<VariableExprAST>(idName);

    // Call.
    getNextToken(); // eat (
    vector<unique_ptr<ExprAST>> Args;
    if (currTok != ')')
    {
        while (true)
        {
            if (auto Arg = ParseExpression())
                Args.push_back(move(Arg));
            else
                return nullptr;

            if (currTok == ')')
                break;

            if (currTok != ',')
            {
                LogError("Expected ')' or ',' in argument list");
                return nullptr;
            }
            getNextToken();
        }
    }

    // Eat the ')'.
    getNextToken();

    return make_unique<CallExprAST>(idName, move(Args));
}

// PARSING PRIMARIES
static unique_ptr<ExprAST> ParsePrimary()
{
    switch (currTok)
    {
    case tok_identifier:                    // identifiers
        return ParseIdentifierOrCallExpr(); // parse identifier
    case tok_number:                        // number literal
        return ParseNumberExpr();           // parse number literal
    case '(':                               // parenthesis
        return ParseParenExpr();            // parse parenthesis
    default:                                // report error
        LogError("Unknown token. expected an expression \n");
        return nullptr;
    }
}

// PARSE RIGHT-HAND SIDE
static unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS)
{
    // If this is a binop, find its precedence.
    while (1)
    {                                     // keep parsing right hand side
        int TokPrec = getTokPrecedence(); // get precedence

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (TokPrec < ExprPrec) // precedence is < than curr precedence
            return LHS;         // return left-hand side
        else
        {
            int BinOp = currTok;
            getNextToken(); // eat binop

            // Parse the primary expression after the binary operator.
            auto RHS = ParsePrimary(); // parse right-hand side
            if (RHS)
            {
                int NextPrec = getTokPrecedence();
                if (TokPrec < NextPrec)
                { // get next
                    RHS = ParseBinOpRHS(TokPrec + 1, move(RHS));
                    if (!RHS)
                        return nullptr;
                }
                // merge curr LHS, curr RHS to make a new binary expression AST as new LHS
                LHS = make_unique<BinaryExprAST>(BinOp, move(LHS), move(RHS));
            }
            else
                return nullptr;
        }
    }
}

// PARSE EXPRESSION
static unique_ptr<ExprAST> ParseExpression()
{
    auto LHS = ParsePrimary();

    if (LHS)
    {
        return ParseBinOpRHS(0, move(LHS)); // parse left side
    }
    else
        return nullptr;
}

// PARSING FUNCTION PROTOTYPES - function signature
static unique_ptr<PrototypeAST> ParsePrototype()
{
    if (currTok != tok_identifier)
    {                                                       // current token, not token identfier
        LogError("Expected function name in prototype \n"); // report error
        return nullptr;
    }

    string fnName = identifierStr;
    getNextToken(); // eat identifier

    if (currTok != '(')
    { // report error
        LogError("Expected '(' in prototype \n");
        return nullptr;
    }

    // Read the list of argument names.
    vector<string> argNames; // srore argument names
    while (getNextToken() == tok_identifier)
        argNames.push_back(identifierStr); // add to vector
    if (currTok != ')')
    { // report error
        LogError("Expected ')' in prototype \n");
        return nullptr;
    }

    // success.
    getNextToken(); // eat ')'.

    return make_unique<PrototypeAST>(fnName, move(argNames)); // unique pointer to a prototype AST
}

// PARSING FUNCTION DEFINITIONS
static unique_ptr<FunctionAST> ParseDefinition()
{
    getNextToken(); // eat 'def' token
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;

    auto E = ParseExpression();
    if (E)
        return make_unique<FunctionAST>(move(Proto), move(E)); // unique pointer to a new function AST

    return nullptr; // otherwise return null pointer
}

// PARSING THE EXTERN KEYWORD
static unique_ptr<PrototypeAST> ParseExtern()
{
    getNextToken(); // eat extern token
    return ParsePrototype();
}

// PARSING TOP-LEVEL EXPRESSIONS
static unique_ptr<FunctionAST> ParseTopLevelExpr()
{
    auto E = ParseExpression();
    if (E)
    {
        // Make an anonymous proto.
        auto proto = make_unique<PrototypeAST>("", vector<string>());
        return make_unique<FunctionAST>(move(proto), move(E));
    }
    return nullptr;
}

// THE CODE GENERATOR
static unique_ptr<LLVMContext> TheContext; // owns core LLVM data structures
static unique_ptr<IRBuilder<>> Builder;    // helper object for generating LLVM instructions
static unique_ptr<Module> TheModule;       // LLVM construct with functions and global variables
static map<string, Value *> NamedValues;   // store defined identifiers -> symbol table
// optimizer
// static unique_ptr<legacy::FunctionPassManager> TheFPM;

Value *LogErrorV(const char *Str)
{
    LogError(Str);
    return nullptr;
}

// generate code for numeric literals
Value *NumberExprAST::codegen()
{
    return ConstantFP::get(*TheContext, APFloat(Val)); // holds numeric values
}

// code generation for variable expressions
Value *VariableExprAST::codegen()
{
    Value *V = NamedValues[Name]; // find in symbol table
    if (!V)
        LogErrorV("Unknown variable name - Sijui"); // not in table
    return V;
}

// code generation for binary expressions
Value *BinaryExprAST::codegen()
{
    Value *L = LHS->codegen();
    Value *R = RHS->codegen(); // emit code for left and right-hand sides
    if (!L || !R)
        return nullptr; // either does not exist

    switch (Op)
    {
    case '+':                                       // operator in binary expression (7 + 5) -> '+'
        return Builder->CreateFAdd(L, R, "addtmp"); // add
    case '-':
        return Builder->CreateFSub(L, R, "subtmp"); // subtract
    case '*':
        return Builder->CreateFMul(L, R, "multmp"); // multiply
    case '<':
        L = Builder->CreateFCmpULT(L, R, "cmptmp");                                 // comparison <>
        return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext), "booltmp"); // Convert bool 0/1 to double 0.0 or 1.0
    default:
        return LogErrorV("Invalid binary operator"); // report error
    }
}

// code generation for function calls
Value *CallExprAST::codegen()
{
    Function *CalleeF = TheModule->getFunction(Callee); // lookup name in symbol table
    if (!CalleeF)
        return LogErrorV("Unknown function referenced"); // report error

    if (CalleeF->arg_size() != Args.size())               // arguments mistmatch
        return LogErrorV("Incorrect # arguments passed"); // remort error
    // no errors, proceed
    vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i)
    {
        ArgsV.push_back(Args[i]->codegen()); // add arguments to vector
        if (!ArgsV.back())
            return nullptr; // return null pointer
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp"); // create call instruction, with function name and a set of arguments
}

// code generation for function prototypes
Function *PrototypeAST::codegen()
{
    vector<Type *> Doubles(Args.size(), Type::getDoubleTy(*TheContext));                  // type of each function argument, double fp numbers
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false); // types of argument list
    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get()); // create function based on function type

    // Set names for all arguments.
    unsigned idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[idx++]); // set function arguments names

    return F;
}

// code generation for function definition
Function *FunctionAST::codegen()
{
    Function *TheFunction = TheModule->getFunction(Proto->getName()); // get function from proto based on name

    if (!TheFunction)
        TheFunction = Proto->codegen(); // not in table, define a new function

    if (!TheFunction)
        return nullptr; // otherwise return a null pointer

    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction); // create new and name basic block -> insert into function
    Builder->SetInsertPoint(BB);                                            // insert new instructions to end of basic block

    NamedValues.clear();                  // clear map
    for (auto &Arg : TheFunction->args()) // add function arguments to map after clearing it
        NamedValues[string(Arg.getName())] = &Arg;

    Value *RetVal = Body->codegen(); // codegen function root expr
    if (RetVal)
    {
        Builder->CreateRet(RetVal); // completes function if no errors

        verifyFunction(*TheFunction); // verify generated code -> check consistency -> catch bugs

        // TheFPM->run(*TheFunction); // optmize

        return TheFunction; // return function
    }
    TheFunction->eraseFromParent(); // otherwise cleanup
    return nullptr;                 // return null pointer
}

// TOP_LEVEL PARSING
static void InitializeModule()
{
    TheContext = make_unique<LLVMContext>();                                    // new context
    TheModule = make_unique<Module>("JIT(Just In Time Compiler)", *TheContext); // new module

    Builder = make_unique<IRBuilder<>>(*TheContext); // create new builder
}

static void handleDefinition()
{
    if (auto FnAST = ParseDefinition())
    {
        if (auto *FnIR = FnAST->codegen()) // code in IR
        {
            fprintf(stderr, "Read function definition:");
            FnIR->print(errs()); // print IR code
            fprintf(stderr, "\n");
        }
    }
    else
    {
        getNextToken(); // skip token, error recovery
    }
}

static void handleExtern()
{
    if (auto ProtoAST = ParseExtern())
    {
        if (auto *FnIR = ProtoAST->codegen())
        {
            fprintf(stderr, "Read extern: ");
            FnIR->print(errs());
            fprintf(stderr, "\n");
        }
    }
    else
    {
        getNextToken(); // skip token, error recovery
    }
}

static void handleTopLevelExpression()
{
    if (auto FnAST = ParseTopLevelExpr()) // evaluate top-level expression into anonymous function
    {
        if (auto *FnIR = FnAST->codegen())
        {
            fprintf(stderr, "Read top-level expression:");
            FnIR->print(errs());
            fprintf(stderr, "\n");

            FnIR->eraseFromParent(); // remove anonymous expression
        }
    }
    else
    {
        getNextToken(); // skip token, error recovery
    }
}

// DRIVER CODE - repl
static void run()
{
    while (1)
    {
        fprintf(stderr, "ready> ");
        switch (currTok)
        {
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

// OPTIMIZATION

// void InitializeModuleAndPassManager(void)
// {
//     TheModule = make_unique<Module>("JIT AND OPTIMIZE", *TheContext); // create new module

//     TheFPM = make_unique<legacy::FunctionPassManager>(TheModule.get()); // attach a pass manager

//     TheFPM->add(createInstructionCombiningPass()); // peephole and bit-twiddling optimizations
//     TheFPM->add(createReassociatePass());          // reassociation expressions
//     TheFPM->add(createGVNPass());                  // common subexpression elimination
//     TheFPM->add(createCFGSimplificationPass());    // removing unreachable blocks -> simple control flow graph

//     TheFPM->doInitialization();
// }

int main()
{
    // test lexer
    // while(true)
    //     cout << "Token: " << getTok() << endl;
    // test parser
    // Prime the first token.
    // InitializeModuleAndPassManager();
    fprintf(stderr, "ready> ");
    getNextToken();
    InitializeModule(); // create module to hold code
    run();
    TheModule->print(errs(), nullptr); // print generated code
    return 0;
}

// compilation and execution
// ./build
// ./main.bin