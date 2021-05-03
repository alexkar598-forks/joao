#include "Parser.h"
#include "Object.h"
#include "Directory.h"

#define SYMBOL_ENUMS(a,b) ((a << 9) | b)



Program Parser::parse() // This Parser is w/o question the hardest part of this to write.
{
	assert(tokens.size() > 0);

	std::vector<ClassDefinition*> classdef_list;

	for (tokenheader = 0; tokenheader < tokens.size(); ++tokenheader)
	{

		switch (grammarstack.front())
		{
		case(GrammarState::program): // This state implies we're surfing the global scope, looking for things to def.
		{
			//Expecting: a bunch of classdefs and funcdefs
			//Both of these start with a directory that ends in a '/'Name, so lets read that in
			Token* t = tokens[tokenheader];
			if (t->class_enum() != Token::cEnum::DirectoryToken)
			{
				ParserError(t, "Unexpected Token at global-scope definition when Directory was expected!");
			}

			std::string dir_name = static_cast<DirectoryToken*>(t)->dir;

			++tokenheader;
			//The next token has to be either a '(', which disambiguates us into being a funcdef,
			//or a '{', which brings us towards being a classdef.
			t = tokens[tokenheader];
			if (t->class_enum() != Token::cEnum::PairSymbolToken)
			{
				ParserError(t, "Unexpected Token at global-scope definition!");
				continue; // ?????????
			}

			PairSymbolToken st = *static_cast<PairSymbolToken*>(t); // Allah

			PairSymbolToken::pairOp pop = st.t_pOp;
			if (pop == PairSymbolToken::pairOp::Bracket || !st.is_start)
			{
				ParserError(t, "Unexpected PairSymbol at global-scope definition!");
				continue;
			}
			if (pop == PairSymbolToken::pairOp::Paren) // THIS IS A FUNCDEF! HOT DAMN we're getting somewhere
			{
				int close = find_closing_pairlet(PairSymbolToken::pairOp::Paren, tokenheader + 1);

				std::vector<std::string> pirate_noise;
				if (close != tokenheader + 1)
				{
					pirate_noise.push_back(readName(tokenheader+1)); // Updates tokenheader hopefully
					for (int where = tokenheader; where < close; ++where)
					{
						if (tokens[where]->class_enum() != Token::cEnum::CommaToken)
						{
							ParserError(tokens[where], "Unexpected Token when Comma expected!");
						}
						++where;
						if (where == close)
							ParserError(tokens[where], "Missing parameter name in FunctionDefinition!");
						if (tokens[where]->class_enum() != Token::cEnum::WordToken)
							ParserError(tokens[where], "Unexpected Token when ParameterName expected!");
						pirate_noise.push_back(static_cast<WordToken*>(tokens[where])->word);
					}
				}


				std::vector<Expression*> bluh = readBlock(BlockType::Function,close+1, tokens.size()-1);
				--tokenheader;

				Function* func = new Function(dir_name, bluh, pirate_noise);

				t_program.set_func(dir_name, func);

				continue;
			}
			else if (pop == PairSymbolToken::pairOp::Brace) // THIS IS A CLASSDEF!
			{
#ifdef LOUD_TOKENHEADER
				std::cout << "Classdefinition began read at tokenheader " << std::to_string(tokenheader) << std::endl;
#endif
				std::vector<LocalAssignmentStatement*> lasses = readClassDef(dir_name, tokenheader, tokens.size() - 1);

				classdef_list.push_back(new ClassDefinition(dir_name, lasses));

#ifdef LOUD_TOKENHEADER
				std::cout << "Classdefinition set tokenheader for globalscope to " << std::to_string(tokenheader) << std::endl;
#endif

				--tokenheader; // Handle imminent increment
				
				continue;

				//ParserError(t, "Classdefs are not implemented yet!");
			}
			else
			{
				ParserError(t, "Unexpected Pairsymbol at global-scope definition!");
				continue;
			}
		}
		case(GrammarState::block): // This state implies we're entering into a scope
		//Right now, it's the Interpreter's duty to comprehend how things scope out. We're just here to parse things, not run them.
		{
			ParserError(tokens[tokenheader], "Parsing block in an unknown context!");
			readBlock(BlockType::Unknown, tokenheader, tokens.size() - 1); // Has to be a function so as to allow itself to call itself recursively.
			--tokenheader;
			continue;
		}
		}

	}

	generate_object_tree(classdef_list);

	return t_program;
}

void Parser::generate_object_tree(std::vector<ClassDefinition*>& cdefs)
{
	/*
	So in general we would like for the properties and methods of object types to be "cooked,"
	insofar that you can get a handle on, an ObjectType during runtime and it'll have all the properties and methods pre-inherited from object types higher in the hierarchy.

	Making this though is a bit of a headache, since the user programmer can declare functions and classdefs anywhere within the script, in any order.
	Further, a type may be poofed into existence by a singular function definition and nowhere else;
	an object type which inherits from its parent in all ways spare that one function.

	Here is my attempt at resolving the problem, written at three in the morning. Enjoy.
	*/

	//Step 1. Get a list of all classes that exist in some way
	std::unordered_map<std::string, ObjectType*> list_of_types;
	std::unordered_map<std::string, bool> list_of_funcs; // Not as used here, mostly for the ParserError

	std::unordered_map<std::string, Function*> fdefs = t_program.definedFunctions;

	for (auto it = cdefs.begin(); it != cdefs.end(); ++it) // Ask all the classdefs
	{
		ClassDefinition* cdptr = *it;
		std::string cdstr = cdptr->direct;

		if (list_of_types.count(cdstr))
		{
			ParserError(nullptr, "Duplicate class definition detected!"); // FIXME: Allow for this (with perhaps suppressable warnings regardless)
			continue;
		}

		ObjectType* objtype = new ObjectType(cdstr, cdptr->resolve_properties(*this));

		list_of_types[cdstr] = objtype; // Writing a null to here, I think, still works for creating the entry. Suck it, Lua!
	}
	for (auto it = fdefs.begin(); it != fdefs.end(); ++it) // Ask all the functions
	{
		Function* fptr = it->second;

		std::string function_fullname = it->second->get_name();

		std::string function_shortname = it->first;

		if (list_of_funcs.count(function_fullname))
		{
			ParserError(nullptr, "Duplicate function definition detected!"); // FIXME: Allow for this (with suppressable warnings anyways)
			continue;
		}
		list_of_funcs[function_fullname] = true;

		std::string dir_f = Directory::DotDot(function_fullname);

		if (dir_f == "" || dir_f == "/") // I don't trust std::string.empty()
		{
			std::cout << "This is a global function: " << function_fullname << std::endl;
			continue;
		}
		
		if (!list_of_types.count(dir_f)) // If our type doesn't exist
		{
			ObjectType* objtype = new ObjectType(dir_f);
			objtype->set_typemethod(*this, function_shortname, fptr);
			list_of_types[dir_f] = objtype; // Make it so!
		}
		else
		{
			list_of_types[dir_f]->set_typemethod(*this, function_shortname, fptr);
		}

	}
#ifdef LOUD_AST
	std::cout << "Here's my class tree:\n";
	for (auto it = list_of_types.begin(); it != list_of_types.end(); ++it)
	{
		std::cout << it->first << std::endl;
	}
#endif
	//2. Generate inheritences and stash them in the hashtables for easier retrieval
		//FIXME
	
	//3. dump eet


	t_program.definedObjTypes = list_of_types;
	return;
}

//Here-there-update; updates through readlvalue.
ASTNode* Parser::readUnary(int here, int there)
{
	if (tokens[here]->class_enum() != Token::cEnum::SymbolToken)
		return readPower(here, there);

	UnaryExpression::uOps uh = symbol_to_uOp(static_cast<SymbolToken*>(tokens[here]));
	if (uh == UnaryExpression::uOps::NoOp)
	{
		ParserError(tokens[here], "Unexpected symbol when unary operator was expected!");
	}



	return new UnaryExpression(uh, readPower(here+1,there));
	
}

//Vaguely similar and correlated to the behavior of readBinExp's loop, except it collects all lvalues and then builds the exponent chain via iterating in reverse order
//Right-associativity, amirite?
ASTNode* Parser::readPower(int here, int there)
{
	std::vector<ASTNode*> lvalues;

	int where = here;
	int last_power = here - 1;
	for (; where <= there; ++where)
	{
		Token* t = tokens[where];
		switch (t->class_enum())
		{
		case(Token::cEnum::PairSymbolToken): // WARNING: THIS IS A DUMB CTRL+C CTRL+V OF THE ANALOGOUS BLOCK IN READBINEXP(); SHOULD BE IDENTICAL!
		{
			PairSymbolToken pst = *static_cast<PairSymbolToken*>(t);

			if (!pst.is_start)
				ParserError(t, "Unexpected closing pairlet in BinaryExpression!");

			int yonder = find_closing_pairlet(pst.t_pOp, where + 1);
			if (yonder > there) // If down yonder is truly yonder
			{
				ParserError(t, "Could not find closing pairlet for open pairlet in BinaryExpression!");
			}
			where = yonder; // Expecting an imminent increment to make this point the correct place
			continue;
		}
		case(Token::cEnum::EndLineToken):
			goto READPOWER_LEAVE_POWERSEARCH;
		case(Token::cEnum::KeywordToken):
			ParserError(tokens[where], "Unexpected keyword in Expression!");
			continue;
		case(Token::cEnum::SymbolToken):
		{
			//This should just be '^' and nothing else; anything else implies a parsing error in this context.

			SymbolToken* st = static_cast<SymbolToken*>(t);

			if (st->len > 1 || st->get_symbol()[0] != '^') // Hardcoded to '^' for now
			{
				ParserError(t, "Unexpected symbol when parsing PowerExpression!");
			}
			lvalues.push_back(readlvalue(last_power + 1, where - 1));
			last_power = where;
			continue;
		}
		default:
			continue;
		}
	}
READPOWER_LEAVE_POWERSEARCH:
	if (lvalues.empty()) // If we didn't find a power
		return readlvalue(here, there); // then this is just a normal lvalue

	// (a ^ (b ^ (c ^ d)))
	
	ASTNode* powertower = readlvalue(last_power + 1, where - 1); // Add the final lvalue of the series of exponentiations, the "c" of a ^ b ^ c
	for (auto it = lvalues.rbegin(); it != lvalues.rend(); ++it)
	{
		powertower = new BinaryExpression(BinaryExpression::bOps::Exponent, *it, powertower);
	}
	return powertower;
}

//lvalue ::= 'null' | 'false' | 'true' | Numeral | LiteralString | tableconstructor | var_access | functioncall |'(' exp ')'
ASTNode* Parser::readlvalue(int here, int there) // Read an Expression where we know for certain that there's no damned binary operator within it.
{
#ifdef LOUD_TOKENHEADER
	std::cout << "readlvalue starting at " << std::to_string(here) << std::endl;
#endif
	Token* t = tokens[here];

	ASTNode* lvalue = nullptr;
	//First things first, lets find the "lvalue" of this expression, the thing on the left
	switch (t->class_enum())
	{
	case(Token::cEnum::LiteralToken): // 'null' | 'false' | 'true'
	{
		LiteralToken lt = *static_cast<LiteralToken*>(t);
		switch (lt.t_literal)
		{
		case(LiteralToken::Literal::Null):
		{
			lvalue = new Literal(Value());
			break;
		}
		case(LiteralToken::Literal::False):
		{
			lvalue = new Literal(Value(false));
			break;
		}
		case(LiteralToken::Literal::True):
		{
			lvalue = new Literal(Value(true));
			break;
		}
		default:
			ParserError(t, "Unknown LiteralToken type!");
			break;
		}
		tokenheader = here + 1;
#ifdef LOUD_TOKENHEADER
		std::cout << "readlvalue setting tokenheader to " << std::to_string(tokenheader) << std::endl;
#endif
		break;
	}
	case(Token::cEnum::NumberToken): // Numeral
	{
		NumberToken nt = *static_cast<NumberToken*>(t);
		if (nt.is_double)
		{
			lvalue = new Literal(Value(nt.num.as_double));
		}
		else
		{
			lvalue = new Literal(Value(nt.num.as_int));
		}
		tokenheader = here + 1;
#ifdef LOUD_TOKENHEADER
		std::cout << "readlvalue setting tokenheader to " << std::to_string(tokenheader) << std::endl;
#endif
		break;
	}
	case(Token::cEnum::StringToken): // LiteralString
	{
		StringToken st = *static_cast<StringToken*>(t);
		lvalue = new Literal(Value(st.word));
		tokenheader = here + 1;
#ifdef LOUD_TOKENHEADER
		std::cout << "readlvalue setting tokenheader to " << std::to_string(tokenheader) << std::endl;
#endif
		break;
	}
	case(Token::cEnum::GrandparentToken):
	case(Token::cEnum::ParentToken):
	case(Token::cEnum::DirectoryToken):
	case(Token::cEnum::WordToken): //functioncall | var_access
	{
		lvalue = readVarAccess(here, there);
		if (tokenheader > there)
		{
			break;
		}
		//Now check to see if this is actually a function call
		Token* nexttoken = tokens[tokenheader];
		if (nexttoken->class_enum() == Token::cEnum::PairSymbolToken)
		{
			PairSymbolToken* pst = static_cast<PairSymbolToken*>(nexttoken);
			if (pst->is_start && pst->t_pOp == PairSymbolToken::pairOp::Paren)
			{
				int close = find_closing_pairlet(PairSymbolToken::pairOp::Paren, tokenheader + 1);
				if (close != tokenheader + 1)
					lvalue = new CallExpression(lvalue, readArgs(tokenheader + 1, close - 1));
				else
					lvalue = new CallExpression(lvalue, {});

				tokenheader = close + 1;
				break;
			}
		}

		break;
	}
	case(Token::cEnum::EndLineToken):
		ParserError(t, "Endline found when lvalue was expected!");
		break;
	case(Token::cEnum::SymbolToken):
	{
		//This can basically only be a unary operator.

		SymbolToken* st = static_cast<SymbolToken*>(t);

		ParserError(t, "Unexpected or underimplemented unary operation!");
		break;
	}
	case(Token::cEnum::ConstructionToken): // Not legally an lvalue but it makes sense to stash this here for now
	{
		ConstructionToken ct = *static_cast<ConstructionToken*>(t);
		lvalue = new Construction(ct.dir, {}); // FIXME: Allow for constructors to take operands
		tokenheader = here + 3; // Jump over the impending (). Hackish!
		break;
	}
	case(Token::cEnum::PairSymbolToken): // tableconstructor | '(' exp ')'
	{
		PairSymbolToken pst = *static_cast<PairSymbolToken*>(t);
		switch (pst.t_pOp)
		{
		default:
			ParserError(t, "Unexpected or underimplemented use of PairSymbolToken!");
			tokenheader = here + 1;
			lvalue = new Literal(Value());
			break;
		case(PairSymbolToken::pairOp::Brace): // tableconstructor
			ParserError(t, "Unexpected or underimplemented use of brace token!");
			tokenheader = here + 1;
			lvalue = new Literal(Value());
			break;
		case(PairSymbolToken::pairOp::Paren): // '(' exp ')'
			int close = find_closing_pairlet(PairSymbolToken::pairOp::Paren, here+1);
			
			lvalue = readExp(here + 1, close - 1); // ReadExp will increment tokenheader for us, hopefully. Can't say for sure, this recursion is confusing.
		}
		break;
	}
	default:
		ParserError(t, "Unexpected Token when reading lvalue! " + t->class_name());
		break;
	}

	if (!lvalue)
	{
		ParserError(t, "Failed to comprehend lvalue of Expression!");
	}

	
	return lvalue;
}

// Reads, from here to there, scanning for BinaryExpressions of its OperationPrecedence and lower
ASTNode* Parser::readBinExp(Scanner::OperationPrecedence op, int here, int there) 
{
#ifdef LOUD_TOKENHEADER
	std::cout << "readBinExp(" << Scanner::precedence_tostring(op) << ") starting at " << std::to_string(here) << std::endl;
#endif
	/*
	if (expect_close_paren)
		std::cout << "I expect close paren!\n";
	else
		std::cout << "I don't expect close paren!\n";
	*/
	//std::cout << "Starting to search for operation " << Scanner::precedence_tostring(op) << "...\n";

	if (op == Scanner::OperationPrecedence::Unary) // If we're at the near-bottom, evaluate a unary
	{
		return readUnary(here, there); // Hear-hear!
	}
	else if (op == Scanner::OperationPrecedence::Power)
	{
		return readPower(here, there);
	}


	ASTNode* lhs = nullptr;

	int where = here;

	//Now lets try to read the binary operation in question
	for (; where <= there; ++where)
	{
		Token* t2 = tokens[where];
		switch (t2->class_enum())
		{
		case(Token::cEnum::PairSymbolToken): // So the 'exp' grammarthingie doesn't really... care? about these pairsymbols?
		{//So what we are to do, is leave them for readlvalue to pick up and think about. All we need to do is find our operators and do the recursive splitting around them.
			PairSymbolToken pst = *static_cast<PairSymbolToken*>(t2);
			
			if(!pst.is_start)
				ParserError(t2, "Unexpected closing pairlet in BinaryExpression!");

			int yonder = find_closing_pairlet(pst.t_pOp, where + 1);
			if (yonder > there) // If down yonder is truly yonder
			{
				ParserError(t2, "Could not find closing pairlet for open pairlet in BinaryExpression!");
			}
			where = yonder; // Expecting an imminent increment to make this point the correct place
			continue;
		}
		case(Token::cEnum::EndLineToken):
			//++where; // Because all semicolons are explicitly consumed thru consume_semicolon(), hopefully, BinExp must leave pointing to the semicolon it found, if it did find one
			goto READBOP_LEAVE_BOPSEARCH;
		case(Token::cEnum::KeywordToken):
			ParserError(tokens[where], "Unexpected keyword in Expression!");
			continue;
		case(Token::cEnum::SymbolToken):
		{
			SymbolToken st = *static_cast<SymbolToken*>(t2);

			BinaryExpression::bOps boopitybeep;

			if (st.len == 1)
			{
				boopitybeep = readbOpOneChar(st.get_symbol(), t2);
			}
			else
			{
				boopitybeep = readbOpTwoChar(st.get_symbol(), t2);
			}

			if (bOp_to_precedence.at(boopitybeep) == op) // WE GOT A HIT!
			{
				
				//ALL THIS ASSUMES LEFT-ASSOCIATIVITY, AS IN ((1 + 2) + 3) + 4


				if (!lhs)
				{
					lhs = readBinExp(static_cast<Scanner::OperationPrecedence>(static_cast<uint8_t>(op) - 1), here, where-1);
				}

				ASTNode* right = readBinExp(static_cast<Scanner::OperationPrecedence>(static_cast<uint8_t>(op) - 1), where+1, there);
				
				lhs = new BinaryExpression(boopitybeep, lhs, right);
				
				continue;
			}
			else if (bOp_to_precedence.at(boopitybeep) > op)
			{
				goto READBOP_LEAVE_BOPSEARCH;
			}

			//we don't got a hit. :(
			continue;
		}
		default:
			continue;
		}
	}
READBOP_LEAVE_BOPSEARCH:
	//std::cout << "Exiting search for " << Scanner::precedence_tostring(op) << " at token " << where << "...\n";
#ifdef LOUD_TOKENHEADER
	std::cout << "readBinExp(" << Scanner::precedence_tostring(op) << ") setting tokenheader to " << std::to_string(where) << std::endl;
#endif
	tokenheader = where; // FIXME: I don't even really know what exactly there is to fix here, just know that readBinExp does some funky bullshit with the tokenheader that may cause it to malpoint in anything readBinExp calls
	if (lhs)
		return lhs;

	//ParserError(tokens[here], "readBinExp failed to read binary expression!");


	//If we're here then it seems the operation(s) we're looking for doesn't exist
	//So lets just return... whatever it is in the lower stacks
	lhs = readBinExp(static_cast<Scanner::OperationPrecedence>(static_cast<uint8_t>(op) - 1), here, there);


	
	return lhs;
	
}

ASTNode* Parser::readExp(int here, int there)
{
#ifdef LOUD_TOKENHEADER
	std::cout << "Expression starts at " << std::to_string(here) << std::endl;
#endif

	//Okay, god, we're really getting close to actually being able to *resolve* something.

	Token* t = tokens[here];

	Scanner::OperationPrecedence lowop = lowest_ops[t->syntactic_line];

	if (lowop == Scanner::OperationPrecedence::NONE) // If we know for a fact that no binary operation takes place on this syntactic line
	{
		//++tokenheader;
#ifdef LOUD_TOKENHEADER
		ASTNode* ans = readlvalue(here, there);
		std::cout << "Expression leaves at " << std::to_string(tokenheader) << std::endl;
		return ans;
#else
		return readlvalue(here, there);
#endif
	}

	//we know (or at least kinda think) that there's a binop afoot.
#ifdef LOUD_TOKENHEADER
	ASTNode* ans = readBinExp(lowop, here, there);
	std::cout << "Expression leaves at " << std::to_string(tokenheader) << std::endl;
	return ans;
#else
	return readBinExp(lowop, here, there);
#endif
	
}

//Here-there-update; updates through ReadExp().
LocalAssignmentStatement* Parser::readLocalAssignment(int here, int there) // Value x = 3;
{
	Token* t = tokens[here];
	if (t->class_enum() != Token::cEnum::LocalTypeToken)
		ParserError(t, "Unexpected Token where LocalTypeToken was expected!");

	switch (static_cast<LocalTypeToken*>(t)->t_type) // I feel like a lvl 10 Wizard when I write lines of C++ like this
	{
	case(LocalType::Value):
		break;
	case(LocalType::Local):
		ParserError(t, "'local' is a reserved word; use 'Local' instead!");
		return nullptr;
	default:
		ParserError(t, "Underimplemented LocalTypeToken detected!");
		return nullptr;
	}

	Identifier* id = new Identifier(readName(here+1));

	AssignmentStatement::aOps aesop = readaOp(here+2);

	ASTNode* rvalue = readExp(here+3,there);

	return new LocalAssignmentStatement(id, rvalue, aesop);
}

std::vector<Expression*> Parser::readBlock(BlockType bt, int here, int there) // Tokenheader state should point to the opening brace of this block.
{
	//Blocks are composed of a starting brace, following by statements, and are ended by an end brace.

	//Starting brace checks


	Token* t = tokens[here];
	consume_open_brace(here);

	//In AST land, blocks are a list of expressions associated with a particular scope.
	std::vector<Expression*> ASTs;
	//++tokenheader;
	//I wanna point out that block is the only grammar object that has stats, so we can unroll the description of stats into this for-and-switch.
	int where = here+1;
	for (; where <= there; ++where)
	{
		t = tokens[where];
		switch (t->class_enum())
		{
			//this switch kinda goes from most obvious implementation to least obvious, heh
		case(Token::cEnum::EndLineToken):
			ParserError(t, "Unexpected semicolon in block!"); // Yes I'm *that* picky, piss off
			continue;
		case(Token::cEnum::KeywordToken):
		{
			KeywordToken kt = *static_cast<KeywordToken*>(t);
			switch (kt.t_key)
			{
			case(KeywordToken::Key::For):
			{
				++where; tokenheader = where;// Move the header past the for keyword
				consume_paren(true); // (
				int yonder = find_closing_pairlet(PairSymbolToken::pairOp::Paren, tokenheader);

				size_t semicolon = find_first_semicolon(tokenheader, yonder-1);
				ASTNode* init;
				if (find_aOp(tokenheader, semicolon))
				{
					if (tokens[tokenheader]->class_enum() == Token::cEnum::LocalTypeToken)
						init = readLocalAssignment(tokenheader, semicolon);
					else
						init = readAssignmentStatement(tokenheader, semicolon);
				}
				else
				{
					init = readExp(tokenheader, semicolon);
				}

				where = semicolon + 1;
				semicolon = find_first_semicolon(where, yonder-1);
				ASTNode* cond = readExp(where, semicolon); // Assignments do not evaluate to anything in Jo�o so putting one in a conditional is silly
				where = semicolon + 1;
				ASTNode* inc;
				if (find_aOp(where, yonder-1))
				{
					inc = readAssignmentStatement(where, yonder-1);
				}
				else
				{
					inc = readExp(where, yonder - 1);
				}
				 
				consume_paren(false); // )

				std::vector<Expression*> for_block = readBlock(BlockType::For,tokenheader,there); // { ...block... }

				ASTs.push_back(new ForBlock(init, cond, inc, for_block));

				where = tokenheader - 1; // decrement to counteract imminent increment
				

				//ParserError(t, "For-loops are not implemented!");
				continue;
			}
			case(KeywordToken::Key::If):
			{
				++where; tokenheader = where;
				consume_paren(true); // (
				int yonder = find_closing_pairlet(PairSymbolToken::pairOp::Paren, tokenheader);
				ASTNode* cond = readExp(tokenheader, yonder-1);
				consume_paren(false); // )

				std::vector<Expression*> if_block = readBlock(BlockType::If,tokenheader,there);

				ASTs.push_back(new IfBlock(cond, if_block));

				where = tokenheader - 1;
				continue;
			}
			case(KeywordToken::Key::Elseif):
				ParserError(t, "Unexpected Elseif keyword with no corresponding If!");
				continue;
			case(KeywordToken::Key::Else):
				ParserError(t, "Unexpected Else keyword with no corresponding If!");
				continue;
			case(KeywordToken::Key::Break):
			{
				if (bt == BlockType::Function)
					ParserError(t, "Unexpected Break statement in Function block!");
				++where; tokenheader = where; // Consume break token
				Token* t2 = tokens[where];

				int brk = 1;

				switch (t2->class_enum())
				{
				case(Token::cEnum::NumberToken):
				{
					NumberToken* nt = static_cast<NumberToken*>(t);
					if(nt->is_double)
						ParserError(t, "Unexpected double literal after Break keyword; 'break' may only take expressionless integer literals as input!");
					brk = nt->num.as_int;

					if (brk < 1) // No "break 0;" please, thanks
					{
						ParserError(t, "Non-positive integer given as argument to Break statement!");
					}

					++where; tokenheader = where; // Consume Number token
					consume_semicolon();
					break;
				}//Rolls over into EndLineToken case
				case(Token::cEnum::EndLineToken):
					consume_semicolon();
					break;
				default:
					ParserError(t, "Unexpected Token after Break keyword; 'break' may only take expressionless integer literals as input!");
					continue;
				}

				ASTs.push_back(new BreakStatement(brk));

				where = tokenheader - 1;
				//ParserError(t, "Break statements are not implemented!");
				continue;
			}
			case(KeywordToken::Key::While):
			{				
				++where; tokenheader = where;
				consume_paren(true); // (
				int yonder = find_closing_pairlet(PairSymbolToken::pairOp::Paren, tokenheader);
				ASTNode* cond = readExp(tokenheader, yonder-1);
				consume_paren(false); // )

				std::vector<Expression*> while_block = readBlock(BlockType::While,tokenheader,there);

				ASTs.push_back(new WhileBlock(cond, while_block));

				where = tokenheader - 1;

				//ParserError(t, "While-loops are not implemented!");
				continue;
			}
			case(KeywordToken::Key::Return):
			{
				++where; tokenheader = where; // Consume this return token
				ReturnStatement* rs = new ReturnStatement(readExp(where, there-1));
				ASTs.push_back(rs);
				consume_semicolon();

				where = tokenheader - 1;
				continue;
			}
			default:
				ParserError(t, "Unknown keyword!");
				continue;
			}
		}
		case(Token::cEnum::PairSymbolToken):
		{
			PairSymbolToken pt = *static_cast<PairSymbolToken*>(t);
			if (pt.t_pOp == PairSymbolToken::pairOp::Brace && !pt.is_start)
			{
				//This pretty much has to be the end of the block; lets return our vector of shit.
				tokenheader = where + 1;
				goto BLOCK_RETURN_ASTS; // Can't break because we're in a switch in a for-loop :(
			}
			ParserError(t, "Unexpected PairSymbol while traversing block!");
		}
		
		
		case(Token::cEnum::SymbolToken):
		{
			ParserError(t, "Unexpected or underimplemented Symbol while traversing block!"); //FIXME: Allow for unary expressions to be their own statements (important for ++ and -- when they are implemented)
			break;
		}
		case(Token::cEnum::WordToken):
		case(Token::cEnum::DirectoryToken):
		//If the Grammar serves me right, this is either a varstat or a functioncall.
		//The main way to disambiguate is to check if the var_access is if any assignment operation takes place on this line.
		{
			int yonder = find_first_semicolon(where+1, there);
			int found_aop = find_aOp(where + 1, yonder - 1);

			if(found_aop) // varstat
				ASTs.push_back(readAssignmentStatement(where, there));
			else // functioncall
			{
				ASTNode* luh = readlvalue(where, yonder - 1);
				if (luh->is_expression())
					ASTs.push_back(static_cast<Expression*>(luh));
				else
					ParserError(t, "Unexpected expression when Statement was expected!");
			}
			consume_semicolon();
			where = tokenheader-1; // Decrement so that the impending increment puts us in the correct place.
			continue;
		}
		case(Token::cEnum::LocalTypeToken): // So this implies we're about to read in an initialization.
		{
			//std::cout << "Before: " << std::to_string(tokenheader) << std::endl ;
			LocalAssignmentStatement* localassign = readLocalAssignment(where, there);
			consume_semicolon();
			if(localassign) // if not nullptr
				ASTs.push_back(localassign);
			where = tokenheader - 1; // decrement to counteract imminent increment
			//std::cout << "After: " << std::to_string(tokenheader) << std::endl;
			continue;
		}
		case(Token::cEnum::StringToken):
		case(Token::cEnum::NumberToken):
			ParserError(t, "Misplaced literal detected while traversing block!");
			break;
		default:
			ParserError(t, "Unknown Token type found when traversing block!");
		}
	}
	ParserError(t, "Unable to find ending brace of block!");

BLOCK_RETURN_ASTS:
	if (ASTs.size() == 0)
	{
		ParserError(t, "Block created with no Expressions inside!");
	}

#ifdef LOUD_TOKENHEADER
	std::cout << "Exiting block with header pointed at " << std::to_string(tokenheader) << ".\n";
#endif
	return ASTs;
}

std::vector<LocalAssignmentStatement*> Parser::readClassDef(std::string name, int here, int there)
{
	Token* t = tokens[here];
	consume_open_brace(here);

	std::vector<LocalAssignmentStatement*> ASTs;

	//This grammar state is pretty simple: It's an endless list of LocalAssignStatement-compatible declarations that will later be read to form the object tree/hash.
	int where = here + 1;
	for (; where <= there; ++where)
	{
		t = tokens[where];
		switch (t->class_enum())
		// readLocalAssignment() does this check too, but doing it here lets me give a more specific ParserError. Parser errors aren't errors; they're merely failing with *style*!
		{
		case(Token::cEnum::LocalTypeToken):
		{
			LocalTypeToken* ltt_ptr = static_cast<LocalTypeToken*>(t);
			LocalAssignmentStatement* lassy = readLocalAssignment(where, there);

			ASTs.push_back(lassy);
			consume_semicolon();
			where = tokenheader - 1;
			break;
		}
		case(Token::cEnum::PairSymbolToken):
		{
			PairSymbolToken pt = *static_cast<PairSymbolToken*>(t);
			if (pt.t_pOp == PairSymbolToken::pairOp::Brace && !pt.is_start)
			{
				//This pretty much has to be the end of the block; lets return our vector of shit.
				tokenheader = where + 1;
				//std::cout << "Hello!\n";
				goto READ_CLASSDEF_RETURN_ASTS; // Can't break because we're in a switch in a for-loop :(
			}
			break;
		}
		default:
			ParserError(t, "Unexpected or underimplemented Token detected while traversing class definition!");
		}
	}
	ParserError(t, "Unable to find ending brace of block!");

READ_CLASSDEF_RETURN_ASTS:
	if (ASTs.size() == 0)
	{
		ParserError(t, "Classdef created with no Expressions inside!");
	}
#ifdef LOUD_TOKENHEADER
	std::cout << "Exiting Classdef with header pointed at " << std::to_string(tokenheader) << ".\n";
#endif
	return ASTs;
}

Construction* Parser::readConstruction(int here, int there)
{
	return nullptr;
	std::string dir = "";
	
	int where = here;
	bool looking_for_slash = true;
	for (; where <= there; ++where)
	{
		Token* t = tokens[where];
		
		switch (t->class_enum())
		{
		case(Token::cEnum::SymbolToken):
		{
			if (!looking_for_slash)
				ParserError(t, "Unexpected symbol when reading New() statement!");

			SymbolToken* st = static_cast<SymbolToken*>(t);
			if (st->len != 1 || st->get_symbol()[0] != '/')
			{
				ParserError(t, "Unexpected symbol when reading New() statement!");
			}

			dir.push_back('/');
			continue;
		}
		case(Token::cEnum::KeywordToken): // New, or not, I dunno, I'm a comment not a soothsayer
		{
			if (looking_for_slash)
				ParserError(t, "Unexpected keyword when reading New() statement!");

			
		}

		}
	}

}