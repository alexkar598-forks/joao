#pragma once

#include "AST.h"

#include "Scanner.h"
#include "Program.h"

class Parser
{
	Program t_program;
	std::vector<Token*> tokens;
	
	/*
	Here's how this is gonna work:
	The Parser is going to semi-recursively iterate over the various grammar possibilities,
	based on the formal grammar that we have (which is loosely based on how our grammar description formats it)
	*/

	std::vector<Scanner::OperationPrecedence> lowest_ops;

	const std::unordered_map<BinaryExpression::bOps, Scanner::OperationPrecedence> bOp_to_precedence =
	{
		{BinaryExpression::bOps::Add,Scanner::OperationPrecedence::Term},
		{BinaryExpression::bOps::Subtract,Scanner::OperationPrecedence::Term},
		{BinaryExpression::bOps::Multiply,Scanner::OperationPrecedence::Factor},
		{BinaryExpression::bOps::Divide,Scanner::OperationPrecedence::Factor},
		//
		{BinaryExpression::bOps::FloorDivide,Scanner::OperationPrecedence::Factor},
		{BinaryExpression::bOps::Exponent,Scanner::OperationPrecedence::Power},
		{BinaryExpression::bOps::Modulo,Scanner::OperationPrecedence::Factor},
		//
		{BinaryExpression::bOps::BitwiseAnd,Scanner::OperationPrecedence::Bitwise},
		{BinaryExpression::bOps::BitwiseXor,Scanner::OperationPrecedence::Bitwise},
		{BinaryExpression::bOps::BitwiseOr,Scanner::OperationPrecedence::Bitwise},
		//
		{BinaryExpression::bOps::ShiftLeft,Scanner::OperationPrecedence::Bitwise},
		{BinaryExpression::bOps::ShiftRight,Scanner::OperationPrecedence::Bitwise},
		//
		{BinaryExpression::bOps::Concatenate,Scanner::OperationPrecedence::Concat},
		//
		{BinaryExpression::bOps::LessThan,Scanner::OperationPrecedence::Comparison},
		{BinaryExpression::bOps::LessEquals,Scanner::OperationPrecedence::Comparison},
		{BinaryExpression::bOps::Greater,Scanner::OperationPrecedence::Comparison},
		{BinaryExpression::bOps::GreaterEquals,Scanner::OperationPrecedence::Comparison},
		{BinaryExpression::bOps::Equals,Scanner::OperationPrecedence::Comparison},
		{BinaryExpression::bOps::NotEquals,Scanner::OperationPrecedence::Comparison},
		//
		{BinaryExpression::bOps::LogicalAnd,Scanner::OperationPrecedence::Logical},
		{BinaryExpression::bOps::LogicalOr,Scanner::OperationPrecedence::Logical},
		{BinaryExpression::bOps::LogicalXor,Scanner::OperationPrecedence::Logical}
	};

	enum class BlockType {
		Unknown, // ???
		Function,
		While,
		If,
		For
	};

	int tokenheader; // Which Token # are we on?
	//This whole thing is one big Turing machine, reading a roll of tokens in sequence, so having a master counter is important to the iteration.
	//However, most functions have their own internal headers for subiteration, sometimes updating tokenheader on exit.
	//This is unfortunately necessary due to the recursive and multi-stroke nature of this Parser and its grammar.

	BinaryExpression::bOps readbOpOneChar (char* c)
	{
		switch (c[0])
		{
			//This Switch tries to keep the binOps in the order in which they appear in JoaoGrammar.txt.
		case('+')://multichar needed
			return BinaryExpression::bOps::Add;
			break;
		case('-')://multichar needed
			return BinaryExpression::bOps::Subtract;
			break;
		case('*'):
			return BinaryExpression::bOps::Multiply;
			break;
		case('/')://multichar
			return BinaryExpression::bOps::Divide;
			break;
		case('^'):
			return BinaryExpression::bOps::Exponent;
			break;
		case('%'):
			return BinaryExpression::bOps::Modulo;
			break;
		case('&')://multichar needed
			return BinaryExpression::bOps::BitwiseAnd;
			break;
		case('~')://multichar needed
			return BinaryExpression::bOps::BitwiseXor;
			break;
		case('|')://multichar needed
			return BinaryExpression::bOps::BitwiseOr;
			break;
		case('>')://multichar needed
			return BinaryExpression::bOps::Greater;
			break;
		case('<')://multichar needed
			return BinaryExpression::bOps::LessThan;
			break;
		default:
			return BinaryExpression::bOps::NoOp;
		}
		return BinaryExpression::bOps::NoOp;
	}
	BinaryExpression::bOps readbOpTwoChar(char* c)
	{
		switch (c[1])
		{
			//This Switch tries to keep the binOps in the order in which they appear in JoaoGrammar.txt.
		case('/'):
			return BinaryExpression::bOps::FloorDivide;
			break;
		case('&')://&&
			return BinaryExpression::bOps::LogicalAnd;
			break;
		case('~')://~~
			return BinaryExpression::bOps::LogicalXor;
			break;
		case('|')://||
			return BinaryExpression::bOps::LogicalOr;
			break;
		case('>')://>>
			return BinaryExpression::bOps::ShiftRight;
			break;
		case('<')://<<
			return BinaryExpression::bOps::ShiftLeft;
			break;
		case('='):
			switch (c[0])
			{
			case('>'):// >=
				return BinaryExpression::bOps::GreaterEquals;
			case('<'):// <=
				return BinaryExpression::bOps::LessEquals;
			case('!'): // !=
				return BinaryExpression::bOps::NotEquals;
			case('='): // == 
				return BinaryExpression::bOps::Equals;
			default:
				return BinaryExpression::bOps::NoOp;
			}
			break;
		case('.'):
			return BinaryExpression::bOps::Concatenate;
		default:
			return BinaryExpression::bOps::NoOp;
		}
		return BinaryExpression::bOps::NoOp;
	}
	AssignmentStatement::aOps symbol_to_aOp(SymbolToken* st)
	{
		char* c = st->get_symbol();

		if (st->len == 1)
		{
			if (c[0] == '=')
			{
				return AssignmentStatement::aOps::Assign;
			}
			else
			{
				return AssignmentStatement::aOps::NoOp;
			}
		}

		return AssignmentStatement::aOps::NoOp;
	}

	UnaryExpression::uOps symbol_to_uOp(SymbolToken* st)
	{
		if (st->len != 1)
			return UnaryExpression::uOps::NoOp;

		switch (st->get_symbol()[0])
		{
		case('!'):
			return UnaryExpression::uOps::Not;
		case('-'):
			return UnaryExpression::uOps::Negate;
		case('#'):
			return UnaryExpression::uOps::Length;
		case('~'):
			return UnaryExpression::uOps::BitwiseNot;
		default:
			return UnaryExpression::uOps::NoOp;
		}
	}

	
	
	ASTNode* readlvalue(int,int);
	ASTNode* readUnary(int, int);
	ASTNode* readPower(int, int);
	ASTNode* readBinExp(Scanner::OperationPrecedence,int,int);
	ASTNode* readExp(int,int);
	Construction* readConstruction(int, int);

	//Here-there-update; does not update if no last bracket found
	std::vector<Expression*> readBlock(BlockType, int, int);

	//Here-there-update; does not update if no last bracket found
	std::vector<LocalAssignmentStatement*> readClassDef(std::string, int, int);

	//Reads a strongly-expected LocalAssignment (of form, "Value x = 3" or whatever). Does not consume a semicolon.
	LocalAssignmentStatement* readLocalAssignment(int, int);

	AssignmentStatement::aOps readaOp(int here = 0, bool loud = true)
	{
#ifdef LOUD_TOKENHEADER
		std::cout << "readaOp starting at " << std::to_string(tokenheader) << std::endl;
#endif
		Token* t;
		if (here)
			t = tokens[here];
		else
			t = tokens[tokenheader];


		if (t->class_enum() != Token::cEnum::SymbolToken)
		{
			if(loud)
				ParserError(t, "Unexpected Token when aOp was expected!");
			return AssignmentStatement::aOps::NoOp;
		}
		SymbolToken st = *static_cast<SymbolToken*>(t);

		char* c = st.get_symbol();

		if (st.len == 1)
		{
			if (c[0] == '=')
			{
				if(!here)
					++tokenheader;
				return AssignmentStatement::aOps::Assign;
			}
			else
			{
				if (loud)
				{
					ParserError(t, "Unexpected single-char SymbolToken; aOp was expected!");
				}
				return AssignmentStatement::aOps::NoOp;
			}
		}
		if (loud)
			ParserError(t, "Two-char assignment operations are not implemented!");

		return AssignmentStatement::aOps::NoOp;

		//FIXME: two-char assignment operations!
		if(!here)
			++tokenheader;
#ifdef LOUD_TOKENHEADER
		std::cout << "readaOp setting tokenheader to " << std::to_string(tokenheader) << std::endl;
#endif
		return AssignmentStatement::aOps::NoOp;
	}

	BinaryExpression::bOps readbOp(SymbolToken* st)
	{
		char* symbol = st->get_symbol();
		if (st->len == 1)
			return readbOpOneChar(symbol);
		else
			return readbOpTwoChar(symbol);

		return BinaryExpression::bOps::NoOp;
	}

	// VAR_ACCESS ::=
	// scoped_access | var_access property | var_access element
	// scoped_access ::= './' Name | {'../'}'../' Name | '/' Name | Name
	// property ::= {'.' Name }
	// element ::= {'[' exp ']'}
	//
	// HERE-THERE-UPDATE!
	ASTNode* readVarAccess( int here, int there)
	{
#ifdef LOUD_TOKENHEADER
		std::cout << "readVarAccess starting at " << std::to_string(here) << std::endl;
#endif
		ASTNode* scoped_access = nullptr;

		Token* t = tokens[here];

		switch (t->class_enum())
		{
		case(Token::cEnum::DirectoryToken):
			if (static_cast<DirectoryToken*>(t)->dir != "/" + Directory::lastword(static_cast<DirectoryToken*>(t)->dir))
				ParserError(t, "Unexpected directory when variable access expected!");
			scoped_access = new GlobalAccess(Directory::lastword(static_cast<DirectoryToken*>(t)->dir));
			tokenheader = here + 1;
			break;
		case(Token::cEnum::WordToken): // Name
			scoped_access = new Identifier(static_cast<WordToken*>(t)->word);
			tokenheader = here + 1;
			break;
		case(Token::cEnum::ParentToken):
		{
			if (there == here || tokens[here + 1]->class_enum() != Token::cEnum::WordToken)
				ParserError(t, "ParentToken found with no corresponding Name!");
			scoped_access = new ParentAccess(static_cast<WordToken*>(tokens[here + 1])->word);
			tokenheader = here + 2;
			break;
		}
		case(Token::cEnum::GrandparentToken):
		{
			int depth = 1;
			for(int where = here +1; where <= there; ++where)
			{
				Token* tuk = tokens[here]; // KARH EN TUK

				if (tuk->class_enum() == Token::cEnum::GrandparentToken)
				{
					++depth;
				}
				else if (tuk->class_enum() == Token::cEnum::WordToken)
				{
					scoped_access = new GrandparentAccess(depth, static_cast<WordToken*>(tuk)->word);
					tokenheader = where + 1;
					break;
				}
				else
				{
					ParserError(tuk, "Unexpected Token while reading GrandparentAccess!");
				}
			}
			if(!scoped_access)
				ParserError(t, "GrandparentToken found with no corresponding Name!");
			break;
		}
		default:
			ParserError(t, "Unexpected Token while reading scoped_access in readVarAccess()!");
		}

		if (tokenheader+1 > there) // If we can't access at least 2 more tokens
			return scoped_access; // Just return the scoped_access find.
		//Now lets check for property or element
		Token* propeller = tokens[tokenheader]; // PROPerty or ELLERment. I guess. Shut up.

		switch (propeller->class_enum())
		{
		case(Token::cEnum::MemberToken):
			++tokenheader;
			return new MemberAccess(scoped_access, readVarAccess(tokenheader, there)); // I'm *pretty* sure this ends up being left-associative.
		case(Token::cEnum::PairSymbolToken):
		{
			PairSymbolToken pst = *static_cast<PairSymbolToken*>(propeller);
			if (pst.t_pOp == PairSymbolToken::pairOp::Bracket)
			{
				int yonder = find_closing_pairlet(PairSymbolToken::pairOp::Bracket, tokenheader + 1);
				IndexAccess* ack = new IndexAccess(scoped_access, readExp(tokenheader + 1, yonder - 1));
				tokenheader = yonder + 1;
				return ack;

			}
			//If it's a parenthesis then we're probably about to do a function call but, that's none of *our* business in readVarAccess() so just return
			return scoped_access;
		}
		default: // I dunno what this is, just return what you have and hope the higher stacks know what it is
			return scoped_access;
		}
	}

	/*
		VARSTAT:
		there's four ways of doing varstats: in the Local scope, Object Scope, or Global Scope. This block implements the last three.
			Value x = 3; ## Set local variable to 3
			/x = 3; ## Set global variable to 3
			./x = 3; ## Set property of object we're in called x to 3
			x = 3; ## Ambiguous, sets lowest-scoped x available to 3
	*/
	//Updates tokenheader via readExp().
	AssignmentStatement* readAssignmentStatement(int here, int there)
	{
		ASTNode* id = readVarAccess(here, there);

		AssignmentStatement::aOps aesop = readaOp();


		ASTNode* rvalue = readExp(tokenheader, there);

		return new AssignmentStatement(id, rvalue, aesop);
	}

	//A helper function for readArgs() that shouldn't really be called by anyone else
	int find_comma(int here, int there)
	{
		for (int where = here; where <= there; ++where)
		{
			switch (tokens[where]->class_enum())
			{
			case(Token::cEnum::CommaToken):
				return where;
			case(Token::cEnum::PairSymbolToken): // This is function being called within this function call, I guess? Or perhaps a tableconstructor being used as a parameter.
			//Regardless, we need to skip over it, as it may contain comma tokens that aren't for us.
				where = find_closing_pairlet(static_cast<PairSymbolToken*>(tokens[where])->t_pOp, where + 1);
				break;
			default:
				continue;
			}
				
		}
		return 0; // Safe because it is impossible for the first token of a valid Jo�o program to be a CommaToken
	}

	//Here-there-update;
	//assumes here < there
	std::vector<ASTNode*> readArgs(int here, int there)
	{
#ifdef LOUD_TOKENHEADER
		std::cout << "readVarAccess starting at " << std::to_string(here) << std::endl;
#endif
		int comma = find_comma(here, there);

		if (!comma) // If comman't
			return { readExp(here,there) };
		//exp {, exp}

		std::vector<ASTNode*> args{ readExp(here,comma - 1) };
		tokenheader = here;
		
		while (comma)
		{
			int newcomma = find_comma(comma + 1, there);
			int yonder = (newcomma ? newcomma - 1 : there);
			args.push_back(readExp(comma + 1, yonder));
			comma = newcomma;
		}

		return args;
	}

	std::string readName(int here)
	{
#ifdef LOUD_TOKENHEADER
		std::cout << "readName starting at " << std::to_string(here) << std::endl;
#endif
		Token* t = tokens[here];
		if (t->class_enum() != Token::cEnum::WordToken)
		{
			ParserError(t, "Unexpected token WordToken was expected for Name!");
		}
		tokenheader = here + 1;
#ifdef LOUD_TOKENHEADER
		std::cout << "readName setting tokenheader to " << std::to_string(tokenheader) << std::endl;
#endif
		return static_cast<WordToken*>(t)->word;
	}

	// If no args, assumes tokenheader is pointing where it should. Increments tokenheader when given no args.
	void consume_semicolon(Token* t = nullptr) 
	{
		if (!t)
		{
			t = tokens[tokenheader];
			++tokenheader;
		}

		switch (t->class_enum()) // This could be an if-statement but I've been switching like this everywhere to the point it feels like poor style to not do it here
		{
		case(Token::cEnum::EndLineToken):
			return;
		default:
			//std::cout << "Stalling...le attempting to consumle attempting to consumle attempting to consumle attempting to consumle attempting to consumle attempting to consumle attempting to consum";
			ParserError(t, "Unexpected Token while attempting to consume EndLineToken!");
		}
	}

	void consume_paren(bool open, Token* t = nullptr)
	{
		if (!t)
		{
			t = tokens[tokenheader];
			++tokenheader;
		}

		switch (t->class_enum()) // This could be an if-statement but I've been switching like this everywhere to the point it feels like poor style to not do it here
		{
		case(Token::cEnum::PairSymbolToken):
		{
			PairSymbolToken pst = *static_cast<PairSymbolToken*>(t);
			if (pst.is_start != open || pst.t_pOp != PairSymbolToken::pairOp::Paren)
			{
				ParserError(t, "Unexpected PairSymbolToken while attempting to consume open Paren!");
			}
			return;
		}
		default:
			ParserError(t, "Unexpected Token while attempting to consume open Paren!");
		}
	}

	void consume_open_brace(int here)
	{
		Token* t = tokens[here];
		if (t->class_enum() != Token::cEnum::PairSymbolToken)
		{
			ParserError(t, "Unexpected character where open-brace was expected!");
		}
		PairSymbolToken pt = *static_cast<PairSymbolToken*>(t);
		if (!pt.is_start || pt.t_pOp != PairSymbolToken::pairOp::Brace)
		{
			ParserError(t, "Unexpected character where open-brace was expected!");
		}
	}

	size_t find_first_semicolon(int here, int there)
	{
		for (int where = here; where <= there; ++where)
		{
			Token* t = tokens[where];

			if (t->class_enum() == Token::cEnum::EndLineToken)
				return where;
		}
		ParserError(tokens[here], "Failed to find expected semicolon!");
		return tokens.size() - 1;
	}

	//Does not update tokenheader, and so is a "wanderer" sorta function. Finds the closing pairlet token of the type desired.
	int find_closing_pairlet(PairSymbolToken::pairOp pop, int here)
	{
		int count = 1;

		for (size_t where = here; where < tokens.size(); ++where)
		{
			Token* t = tokens[where];
			if (t->class_enum() != Token::cEnum::PairSymbolToken)
				continue;
			PairSymbolToken* pst = static_cast<PairSymbolToken*>(t);
			if (pst->t_pOp != pop)
				continue;
			if (pst->is_start) // We'll also now need to find the closer for this one
			{
				++count;
			}
			else
			{
				--count;
				if (!count)
				{
					//std::cout << "I return " << std::to_string(where);
					return where;
				}
			}
		}
		ParserError(tokens[static_cast<size_t>(here)-1], "Unable to find closing pairlet for this open pairlet!");
		return 0;
	}
	
	int find_aOp(int here, int there)
	{
		for (int where = here; where <= there; ++where)
		{
			Token* t = tokens[where];
			if (t->class_enum() != Token::cEnum::SymbolToken)
				continue;
			if (symbol_to_aOp(static_cast<SymbolToken*>(t)) != AssignmentStatement::aOps::NoOp)
			{
				return where;
			}
		}
		return 0;
	}
	
protected:
	//Does the nitty-gritty of filling out the function and objecttype tables.
	void generate_object_tree(std::vector<ClassDefinition*>&);
public:
	Parser(Scanner&t)
		:tokens(t.tokens)
		,lowest_ops(t.lowest_ops)
	{
	}
	void ParserError()
	{
		std::cout << "PARSER_ERROR: UNKNOWN!";
		exit(1);
	}
	void ParserError(Token* t, std::string what)
	{
		//This is just a basic setup while everything else is fleshed out.
		std::cout << "PARSER_ERROR: " << what << "\n";
		if (t)
			std::cout << t->dump();
		else
			std::cout << "ERROR_ERROR: No Token pointer provided to ParserError()!\n";
		exit(1);
	}
	void ParserError(Token& t, std::string what)
	{
		//This is just a basic setup while everything else is fleshed out.
		std::cout << "PARSER_ERROR: " << what << "\n";
		exit(1);
	}
	Program parse();
};