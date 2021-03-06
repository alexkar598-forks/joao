#include "Forward.h"
#include "AST.h"
#include "Object.h"
#include "Table.h"
#include "Interpreter.h"
#include "Parser.h"

#define BIN_ENUMS(a,b,c) ( (uint32_t(a) << 16) | (uint32_t(b) << 8)  | uint32_t(c) )
#define UN_ENUMS(a,b) ((uint32_t(a) << 8)  | uint32_t(b) )


Value Value::dev_null = Value();

std::string Value::to_string()
{
	switch (t_vType)
	{
	case(vType::Null):
		return "NULL";
	case(vType::Bool):
		return std::to_string(t_value.as_bool);
	case(vType::Integer):
		return std::to_string(t_value.as_int);
	case(vType::Double):
		return std::to_string(t_value.as_double);
	case(vType::String):
		return *(t_value.as_string_ptr);
	case(vType::Object):
		return t_value.as_object_ptr->dump();
	default:
		return "???";
	}
}

std::string Value::typestring()
{
	switch (t_vType)
	{
	case(vType::Null):
		return "NULL";
	case(vType::Bool):
		return "Boolean";
	case(vType::Integer):
		return "Integer";
	case(vType::Double):
		return "Double";
	case(vType::String):
		return "String";
	case(vType::Object):
		return "Object";
	default:
		return "UNKNOWN!!";
	}
}

Value ASTNode::resolve(Interpreter& interp)
{
	interp.RuntimeError(this, "Attempted to resolve() an abstract ASTNode! (Type:" + class_name()  + ")");
	return Value();
}

Value ASTNode::const_resolve(Parser& parse, bool loudfail)
{
	if (loudfail)
	{
		parse.ParserError(nullptr, "Attempted to const_resolve() an abstract ASTNode! (Type:" + class_name() + ")");
	}
	return Value();
}

Value& ASTNode::handle(Interpreter& interp)
{
	interp.RuntimeError(this, "Attempted to turn " + class_name() + " into a variable handle!");
	return Value::dev_null; // Memory leak woooo
}


//resolve()
Value Literal::resolve(Interpreter& interp)
{
	return heldval;
}
Value Literal::const_resolve(Parser& parse, bool loudfail)
{
	return heldval;
}

Value Identifier::resolve(Interpreter& interp)
{
	// So to "resolve" this means to get its current value, if it has one.
	//Some uses of this class may actually not call the resolve function, and instead simply use it to store the string name of a variable,
	//so as to be able to set it within a scope or, whatever, like for assignment and assignment-y purposes.

	return interp.get_var(t_name,this);
}

Value& Identifier::handle(Interpreter& interp)
{
	//We actually have to evaluate here whether or not the identifier present is pointing to a function or not, at the current scope.

	Function* funky = interp.get_func(t_name,this,false);
	if (funky)
	{
		return *(new Value(funky)); // memory leak woo
	}
	else
	{
		return interp.get_var(t_name, this);
	}
}



Value AssignmentStatement::resolve(Interpreter& interp)
{
	Value rhs_val = rhs->resolve(interp);

	Value& lhs_val = id->handle(interp);

	if (lhs_val.t_vType == Value::vType::Function)
	{
		std::cout << "WARNING: Overwriting a Value which stores a function pointer!\n";
	}

	//While we do know our own assignment, we don't actually use that information in the AST (as of 7th May 2021)
	//That's assumed to be handled by the parser.

	lhs_val = rhs_val;

	return rhs_val; // If anything.
}

Value LocalAssignmentStatement::resolve(Interpreter& interp)
{
	Value rhs_val = rhs->resolve(interp);
	//std::cout << "Their name is " + id->get_str() + " and their value is " + std::to_string(rhs_val.t_value.as_int) + "\n";

	if (t_op != aOps::Assign)
	{
		interp.RuntimeError(this, "Attempt to call unimplemented Assignment operation: " + (int)t_op);
	}

	if (!typecheck(rhs_val))
	{
		interp.RuntimeError(this, "LocalAssignmentStatement failed typecheck!");
		return rhs_val; //FIXME: I'm not sure whether or not I want this runtime to cause the assignment to completely fail like this, or not.
	}
	interp.init_var(static_cast<Identifier*>(id)->get_str(), rhs_val, this);

	return rhs_val;
}
Value LocalAssignmentStatement::const_resolve(Parser& parser, bool loudfail)
{
	return rhs->const_resolve(parser, loudfail);
}

Value UnaryExpression::resolve(Interpreter& interp)
{
	Value rhs = t_rhs->resolve(interp);
	Value::vType rtype = rhs.t_vType;


	uint32_t switcher = UN_ENUMS(t_op, rtype);

	switch (switcher)
	{
	//NEGATE
	case(UN_ENUMS(uOps::Negate,Value::vType::Integer)):
		return Value(-rhs.t_value.as_int);
	case(UN_ENUMS(uOps::Negate, Value::vType::Double)):
		return Value(-rhs.t_value.as_double);
	//NOT
	case(UN_ENUMS(uOps::Not, Value::vType::Null)): // !NULL === NULL
		return Value();
	case(UN_ENUMS(uOps::Not, Value::vType::Bool)):
		return Value(!rhs.t_value.as_bool);
	case(UN_ENUMS(uOps::Not, Value::vType::Integer)):
		return Value(!rhs.t_value.as_int);
	case(UN_ENUMS(uOps::Not, Value::vType::Double)):
		return Value(!rhs.t_value.as_double);
	//BITWISENOT
	case(UN_ENUMS(uOps::BitwiseNot, Value::vType::Integer)):
		return Value(~rhs.t_value.as_int);
	case(UN_ENUMS(uOps::BitwiseNot, Value::vType::Double)): // Does, indeed, flip the bits of the double
	{
		uint64_t fauxint = ~*(reinterpret_cast<uint64_t*>(&rhs.t_value.as_double));
		double newdouble = *(reinterpret_cast<double*>(&fauxint));
		return Value(newdouble);
	}
	//LENGTH
	case(UN_ENUMS(uOps::Length, Value::vType::String)):
		return Value(rhs.t_value.as_string_ptr->size());
	case(UN_ENUMS(uOps::Length, Value::vType::Object)):
		if (rhs.t_value.as_object_ptr->is_table())
			return Value(static_cast<Table*>(rhs.t_value.as_object_ptr)->length());
	//Rolls over into the default error case on length failure
	default:
		interp.RuntimeError(this, "Failed to do an Unary operation! (" + rhs.to_string() + ")\nType: (" + rhs.typestring() + ")");
		return Value();
	}
}

std::pair<std::string, Value> LocalAssignmentStatement::resolve_property(Parser& parser)
{
	//step 1. get string
	std::string suh = static_cast<Identifier*>(id)->get_str();

	//step 2. get value
	Value ruh = rhs->const_resolve(parser, true);

	if(ty == LocalType::Local)
		parser.ParserError(nullptr, "Unknown or underimplemented LocalType enum detected!");

	if(!typecheck(ruh))
		parser.ParserError(nullptr, "Const-resolved rvalue for ObjectType property failed typecheck!");

	//step 3. profit!
	return std::pair<std::string, Value>(suh,ruh);
}

Value BinaryExpression::resolve(Interpreter& interp)
{
	//The Chef's ingredients: t_op, t_lhs, t_rhs
	Value lhs = t_lhs->resolve(interp);
	Value rhs = t_rhs->resolve(interp); //TODO: This fails the principle of short-circuiting but we'll be fine for now

	Value::vType lhs_type = lhs.t_vType;
	Value::vType rhs_type = rhs.t_vType;

	/*
	So for this we're going to do something a little wacky.
	I want this to be only one switch statement, but there's three dimensions of combinatorics:
	the operator used, the type of lhs, and the type of rhs.
	To do this, I am going to synthesize these three UInt8 enums into one Uint32 that is then switched against to find what to do.
	*/

	uint32_t switcher = BIN_ENUMS(t_op, lhs_type, rhs_type);

	switch (switcher)
	{
	//DOUBLE & DOUBLE
	case(BIN_ENUMS(bOps::Add, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double + rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Subtract, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double - rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Multiply, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double * rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Divide, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double / rhs.t_value.as_double);
	//
	case(BIN_ENUMS(bOps::FloorDivide, Value::vType::Double, Value::vType::Double)):
		return Value(floor(lhs.t_value.as_double / rhs.t_value.as_double));
	case(BIN_ENUMS(bOps::Exponent, Value::vType::Double, Value::vType::Double)):
		return Value(pow(lhs.t_value.as_double, rhs.t_value.as_double));
	case(BIN_ENUMS(bOps::Modulo, Value::vType::Double, Value::vType::Double)):
	{
		double nowhere;
		return Value(modf(lhs.t_value.as_double / rhs.t_value.as_double, &nowhere) * rhs.t_value.as_double);
	}
	//
		//Bitwise :(
	//
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::Double, Value::vType::Double)):
		return Value(std::to_string(lhs.t_value.as_double) + std::to_string(rhs.t_value.as_double));
	//
	case(BIN_ENUMS(bOps::LessThan, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double < rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LessEquals, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double <= rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Greater, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double > rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::GreaterEquals, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double >= rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Equals, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double == rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::NotEquals, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double != rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double && rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Double, Value::vType::Double)):
		return Value(lhs.t_value.as_double || rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Double, Value::vType::Double)):
		return Value((lhs.t_value.as_double || rhs.t_value.as_double) && !(lhs.t_value.as_double && rhs.t_value.as_double));


	//DOUBLE & INT
	case(BIN_ENUMS(bOps::Add, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double + rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Subtract, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double - rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Multiply, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double * rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Divide, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double / rhs.t_value.as_int);
	//
	case(BIN_ENUMS(bOps::FloorDivide, Value::vType::Double, Value::vType::Integer)):
		return Value(floor(lhs.t_value.as_double / rhs.t_value.as_int));
	case(BIN_ENUMS(bOps::Exponent, Value::vType::Double, Value::vType::Integer)):
		return Value(pow(lhs.t_value.as_double, rhs.t_value.as_int));
	case(BIN_ENUMS(bOps::Modulo, Value::vType::Double, Value::vType::Integer)):
	{
		double nowhere;
		return Value(modf(lhs.t_value.as_double / rhs.t_value.as_int, &nowhere) * rhs.t_value.as_int);
	}
	//
		//Bitwise :(
	//
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::Double, Value::vType::Integer)):
		return Value(std::to_string(lhs.t_value.as_double) + std::to_string(rhs.t_value.as_int));
	//
	case(BIN_ENUMS(bOps::LessThan, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double < rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LessEquals, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double <= rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Greater, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double > rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::GreaterEquals, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double >= rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Equals, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double == rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::NotEquals, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double != rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double && rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Double, Value::vType::Integer)):
		return Value(lhs.t_value.as_double || rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Double, Value::vType::Integer)):
		return Value((lhs.t_value.as_double || rhs.t_value.as_int) && !(lhs.t_value.as_double && rhs.t_value.as_int));



	//INT & DOUBLE
	case(BIN_ENUMS(bOps::Add, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int + rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Subtract, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int - rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Multiply, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int * rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Divide, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int / rhs.t_value.as_double);
	//
	case(BIN_ENUMS(bOps::FloorDivide, Value::vType::Integer, Value::vType::Double)):
		return Value(floor(lhs.t_value.as_int / rhs.t_value.as_double));
	case(BIN_ENUMS(bOps::Exponent, Value::vType::Integer, Value::vType::Double)):
		return Value(pow(lhs.t_value.as_int, rhs.t_value.as_double));
	case(BIN_ENUMS(bOps::Modulo, Value::vType::Integer, Value::vType::Double)):
		return Value(modf(lhs.t_value.as_int / rhs.t_value.as_double, nullptr));
	//
		//TODO: Bitwise stuff with doubles, too
	//
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::Integer, Value::vType::Double)):
		return Value(std::to_string(lhs.t_value.as_int) + std::to_string(rhs.t_value.as_double));
	//
	case(BIN_ENUMS(bOps::LessThan, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int < rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LessEquals, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int <= rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Greater, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int > rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::GreaterEquals, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int >= rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::Equals, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int == rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::NotEquals, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int != rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int && rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Integer, Value::vType::Double)):
		return Value(lhs.t_value.as_int || rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Integer, Value::vType::Double)):
		return Value((lhs.t_value.as_int || rhs.t_value.as_double) && !(lhs.t_value.as_int && rhs.t_value.as_double));


	//INT & INT
	case(BIN_ENUMS(bOps::Add, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int + rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Subtract, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int - rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Multiply, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int * rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Divide, Value::vType::Integer, Value::vType::Integer)):
	//
	case(BIN_ENUMS(bOps::FloorDivide, Value::vType::Integer, Value::vType::Integer)): // :)
		return Value(lhs.t_value.as_int / rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Exponent, Value::vType::Integer, Value::vType::Integer)):
		return Value(static_cast<int>(pow(lhs.t_value.as_int, rhs.t_value.as_int)));
	case(BIN_ENUMS(bOps::Modulo, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int % rhs.t_value.as_int);
	//
	case(BIN_ENUMS(bOps::BitwiseAnd, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int & rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::BitwiseXor, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int ^ rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::BitwiseOr, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int | rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::ShiftRight, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int >> rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::ShiftLeft, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int << rhs.t_value.as_int);
	//
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::Integer, Value::vType::Integer)):
		return Value(std::to_string(lhs.t_value.as_int) + std::to_string(rhs.t_value.as_int));
	//
	case(BIN_ENUMS(bOps::LessThan, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int < rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LessEquals, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int <= rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Greater, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int > rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::GreaterEquals, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int >= rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::Equals, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int == rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::NotEquals, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int != rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int && rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Integer, Value::vType::Integer)):
		return Value(lhs.t_value.as_int || rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Integer, Value::vType::Integer)):
		return Value((lhs.t_value.as_int || rhs.t_value.as_int) && !(lhs.t_value.as_int && rhs.t_value.as_int));

	//BOOL & BOOL
	case(BIN_ENUMS(bOps::Equals, Value::vType::Bool, Value::vType::Bool)):
		return Value(lhs.t_value.as_bool == rhs.t_value.as_bool);
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Bool, Value::vType::Bool)):
		return Value(lhs.t_value.as_bool && rhs.t_value.as_bool);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Bool, Value::vType::Bool)):
		return Value(lhs.t_value.as_bool || rhs.t_value.as_bool);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Bool, Value::vType::Bool)):
		return Value((lhs.t_value.as_bool || rhs.t_value.as_bool) && !(lhs.t_value.as_bool && rhs.t_value.as_bool));

	//BOOL & INT
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Bool, Value::vType::Integer)):
		return Value(lhs.t_value.as_bool && rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Bool, Value::vType::Integer)):
		return Value(lhs.t_value.as_bool || rhs.t_value.as_int);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Bool, Value::vType::Integer)):
		return Value((lhs.t_value.as_bool || rhs.t_value.as_int) && !(lhs.t_value.as_bool && rhs.t_value.as_int));

	//BOOL & DOUBLE
	case(BIN_ENUMS(bOps::LogicalAnd, Value::vType::Bool, Value::vType::Double)):
		return Value(lhs.t_value.as_bool && rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalOr, Value::vType::Bool, Value::vType::Double)):
		return Value(lhs.t_value.as_bool || rhs.t_value.as_double);
	case(BIN_ENUMS(bOps::LogicalXor, Value::vType::Bool, Value::vType::Double)):
		return Value((lhs.t_value.as_bool || rhs.t_value.as_double) && !(lhs.t_value.as_bool && rhs.t_value.as_double));

	//STRING & STRING
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::String, Value::vType::String)):
	{
		std::string newstr = *(lhs.t_value.as_string_ptr) + *(rhs.t_value.as_string_ptr);
		return Value(newstr);
	}
	case(BIN_ENUMS(bOps::Equals, Value::vType::String, Value::vType::String)):
	{
		return Value(*(lhs.t_value.as_string_ptr) == *(rhs.t_value.as_string_ptr));
	}
	case(BIN_ENUMS(bOps::Greater, Value::vType::String, Value::vType::String)):
	{
		return Value(*(lhs.t_value.as_string_ptr) > *(rhs.t_value.as_string_ptr));
	}
	case(BIN_ENUMS(bOps::GreaterEquals, Value::vType::String, Value::vType::String)):
	{
		return Value(*(lhs.t_value.as_string_ptr) >= * (rhs.t_value.as_string_ptr));
	}
	case(BIN_ENUMS(bOps::LessEquals, Value::vType::String, Value::vType::String)):
	{
		return Value(*(lhs.t_value.as_string_ptr) <= *(rhs.t_value.as_string_ptr));
	}
	case(BIN_ENUMS(bOps::LessThan, Value::vType::String, Value::vType::String)):
	{
		return Value(*(lhs.t_value.as_string_ptr) < *(rhs.t_value.as_string_ptr));
	}

	//STRING & INT
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::String, Value::vType::Integer)):
	{
		std::string newstr = *(lhs.t_value.as_string_ptr) + std::to_string(rhs.t_value.as_int);
		return Value(newstr);
	}
	//INT & STRING
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::Integer, Value::vType::String)):
	{
		std::string newstr = std::to_string(lhs.t_value.as_int) + *(rhs.t_value.as_string_ptr);
		return Value(newstr);
	}
	//STRING & DOUBLE
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::String, Value::vType::Double)):
	{
		std::string newstr = *(lhs.t_value.as_string_ptr) + std::to_string(rhs.t_value.as_double);
		return Value(newstr);
	}
	//DOUBLE & STRING
	case(BIN_ENUMS(bOps::Concatenate, Value::vType::Double, Value::vType::String)):
	{
		std::string newstr = std::to_string(lhs.t_value.as_double) + *(rhs.t_value.as_string_ptr);
		return Value(newstr);
	}

	default:
		interp.RuntimeError(this, "Failed to do a binary operation! (" + lhs.to_string() + ", " + rhs.to_string() + ")\nTypes: (" + lhs.typestring() + ", " + rhs.typestring() + ")");
		return Value();
	}
}


void Function::give_args(Interpreter& interp, std::vector<Value>& args, Object* o = nullptr)
{
	if (o)
	{
		obj = o;
	}
	t_args = args;
	if (t_args.size() < t_argnames.size()) // If we were not given enough arguments
	{
		for (size_t i = t_args.size(); i < t_argnames.size(); ++i)
		{
			t_args.push_back(Value());
		}
		//interp.RuntimeError(*this, "Insufficient amonut of arguments given!");
	}
}
Value Function::resolve(Interpreter & interp)
{
	interp.push_stack(Directory::lastword(t_name),obj);

	for (int i = 0; i < t_args.size() && i < t_argnames.size(); ++i)
	{
		interp.init_var(t_argnames[i], t_args[i], this);
	}
	t_args = {}; // Reset args
	for (auto it = statements.begin(); it != statements.end(); ++it)
	{
		//it is a pointer to a pointer to an Expression.
		Expression* ptr = *it;
		if (ptr->class_name() == "ReturnStatement")
		{
			ReturnStatement rt = *(static_cast<ReturnStatement*>(ptr));

			if (rt.has_expr) // If this actually has something to return
			{
				Value ret = rt.resolve(interp); // Resolve it first
				interp.pop_stack(); // THEN pop the stack

				if (interp.BREAK_COUNTER)
				{
					interp.RuntimeError(this, "Break statement integer too large!");
				}

				return ret; // THEN return it
			}
			break;// otherwise use the default returnValue
		}
		else
		{
			Value vuh = ptr->resolve(interp); // I dunno.
			if (interp.FORCE_RETURN)
			{
				interp.FORCE_RETURN = false;
				interp.pop_stack();
				if (interp.BREAK_COUNTER)
				{
					interp.RuntimeError(this, "Break statement integer too large!");
				}
				return vuh;
			}
		}
	}
	interp.pop_stack();
	obj = nullptr; // Reset object
	if (interp.BREAK_COUNTER)
	{
		interp.RuntimeError(this, "Break statement integer too large!");
	}
	return returnValue;
}

Value CallExpression::resolve(Interpreter& interp)
{
	Value func = func_expr->handle(interp);
	if (func.t_vType != Value::vType::Function)
	{
		interp.RuntimeError(this, "Attempted to call something that isn't a function or method!");
		return Value();
	}

	Function* fptr = func.t_value.as_function_ptr;
	std::vector<Value> vargs;
	for (auto it = args.begin(); it != args.end(); ++it)
	{
		ASTNode* e = *it;
		vargs.push_back(e->resolve(interp));
	}

	Object* obj = fptr->get_obj();
	if (obj)
	{
		return obj->call_method(interp, Directory::lastword(fptr->get_name()), vargs);
	}

	fptr->give_args(interp, vargs);
	return fptr->resolve(interp);
}

Value NativeFunction::resolve(Interpreter& interp)
{
	Value result = lambda(t_args);
	if (result.t_vType == Value::vType::Null && result.t_value.as_int)
	{
		switch (result.t_value.as_int)
		{
		case(static_cast<Value::JoaoInt>((Program::ErrorCode::NoError))): // An expected null, function returned successfully.
			break;
		case(static_cast<Value::JoaoInt>(Program::ErrorCode::BadArgType)):
			interp.RuntimeError(this, "Args of improper type given to NativeFunction!");
			break;
		case(static_cast<Value::JoaoInt>(Program::ErrorCode::NotEnoughArgs)):
			interp.RuntimeError(this, "Not enough args provided to NativeFunction!");
			break;
		default:
			interp.RuntimeError(this, "Unknown RuntimeError in NativeFunction!");
		}
	}
	return result; // Woag.
}

Value NativeMethod::resolve(Interpreter& interp)
{
	if(!obj && !is_static)
		interp.RuntimeError(this, "Cannot call NativeMethod without an Object!");

	Value result = lambda(t_args,obj);
	if (result.t_vType == Value::vType::Null && result.t_value.as_int)
	{
		switch (result.t_value.as_int)
		{
		case(static_cast<Value::JoaoInt>(Program::ErrorCode::NoError)): // An expected null, function returned successfully.
			break;
		case(static_cast<Value::JoaoInt>(Program::ErrorCode::BadArgType)):
			interp.RuntimeError(this, "Args of improper type given to NativeMethod!");
			break;
		case(static_cast<Value::JoaoInt>(Program::ErrorCode::NotEnoughArgs)):
			interp.RuntimeError(this, "Not enough args provided to NativeMethod!");
			break;
		default:
			interp.RuntimeError(this, "Unknown RuntimeError in NativeMethod!");
		}
	}
	return result; // Woag.
}

Value Block::iterate_statements(Interpreter& interp)
{
	for (auto it = statements.begin(); it != statements.end(); ++it)
	{
		//it is a pointer to a pointer to an Expression.
		Expression* ptr = *it;
		if (ptr->class_name() == "ReturnStatement")
		{
			ReturnStatement rt = *static_cast<ReturnStatement*>(ptr);

			interp.FORCE_RETURN = true;
			if (rt.has_expr) // If this actually has something to return
			{
				Value ret = rt.resolve(interp); // Get the return		
				interp.pop_block(); // THEN pop the stack
				return ret; // THEN return the value.
			}
			interp.pop_block();
			return Value();
		}
		else if (ptr->class_name() == "BreakStatement")
		{
			ptr->resolve(interp);
			interp.pop_block();
			return Value();
		}
		else
		{
			Value vuh = ptr->resolve(interp); // I dunno.
			if (interp.FORCE_RETURN)
			{
				interp.pop_block();
				return vuh;
			}
		}
	}
	return Value();
}

Value IfBlock::resolve(Interpreter& interp)
{

	if (condition && !condition->resolve(interp)) // If our condition exists (so we're not an Else) and fails
	{
		if (Elseif) // If we have an Elseif to point to
			return Elseif->resolve(interp); // Return that
		return Value(); // Otherwise return Null, I guess.
	}
	//Getting here either means our condition is true or we have no condition because we're an else statement
	//Either way, lets go innit

	interp.push_block("if");
	Value blockret = iterate_statements(interp);
	
	if (interp.FORCE_RETURN || interp.BREAK_COUNTER)
		return blockret;
	else
		interp.pop_block();
	return Value();
}

Value ForBlock::resolve(Interpreter& interp)
{
	interp.push_block("for"); // Has to happen here since initializer takes place in the for-loops var stack
	if (initializer)
		initializer->resolve(interp);
	while (condition && condition->resolve(interp))
	{
		Value blockret = iterate_statements(interp);
		if (interp.BREAK_COUNTER)
		{
			interp.BREAK_COUNTER -= 1; // I don't trust the decrement operator with this and neither should you.
			return blockret;
		}
		if (interp.FORCE_RETURN)
			return blockret;
		increment->resolve(interp);
	}
	interp.pop_block();
	return Value();
}

Value WhileBlock::resolve(Interpreter& interp)
{
	interp.push_block("while");
	if(!condition) // if condition not defined
		interp.RuntimeError(this, "Missing condition in WhileBlock!");
	/*
	if (!(condition->resolve(interp)))
	{
		std::cout << "THIS! SENTENCE! IS! FALSE!dontthinkaboutitdontthinkaboutitdontthinkaboutitdontthinkaboutitdontthinkaboutit...\n"; // Have to inform the computer to not think about the paradox
	}
	*/
	while (condition->resolve(interp))
	{
		Value blockret = iterate_statements(interp);
		if (interp.BREAK_COUNTER)
		{
			interp.BREAK_COUNTER -= 1;
			return blockret;
		}
		if (interp.FORCE_RETURN)
		{
			//std::cout << "Whileloop got to FORCE_RETURN!\n";
			return blockret;
		}
	}
	interp.pop_block();
	return Value();
}

Value BreakStatement::resolve(Interpreter& interp)
{
	interp.BREAK_COUNTER = breaknum;
	return Value();
}

Value MemberAccess::resolve(Interpreter& interp)
{
	Value& fr = front->handle(interp);

	if (fr.t_vType != Value::vType::Object)
	{
		interp.RuntimeError(this, "Attempted to do MemberAccess on non-Object Value!");
		return Value();
	}
	if (back->class_name() == "Identifier") // This should pretty much always be the case.
	{
		Value* v = fr.t_value.as_object_ptr->has_property(interp, static_cast<Identifier*>(back)->get_str());
		if (v) return *v;
		return Value(fr.t_value.as_object_ptr->get_method(interp, static_cast<Identifier*>(back)->get_str()));
	}

	// This is something confusing, then
	interp.RuntimeError(this, "Unimplemented MemberAccess with second operand of type " + back->class_name() + "!");
	return Value();
}

Value& MemberAccess::handle(Interpreter& interp) // Tons of similar code to MemberAccess::resolve(). Could be merged somehow?
{
	Value& fr = front->handle(interp);
	if (fr.t_vType != Value::vType::Object)
	{
		interp.RuntimeError(this, "Attempted to do MemberAccess on non-Object Value!");
		return Value::dev_null;
	}
	if (back->class_name() == "Identifier")
	{
		Value* v = fr.t_value.as_object_ptr->has_property(interp, static_cast<Identifier*>(back)->get_str());
		if (v) return *v;
		Function* meth = fr.t_value.as_object_ptr->get_method(interp, static_cast<Identifier*>(back)->get_str());
		if (!meth)
		{
			interp.RuntimeError(this, "MemberAccess failed because member does not exist!");
			return Value::dev_null;
		}
		meth->set_obj(fr.t_value.as_object_ptr);
		return *(new Value(meth));
	}

	// This is something confusing, then
	interp.RuntimeError(this, "Unimplemented MemberAccess with second operand of type " + back->class_name() + "!");
	return Value::dev_null;
}

std::unordered_map<std::string, Value> ClassDefinition::resolve_properties(Parser& parse)
{
	std::unordered_map<std::string, Value> svluh;

	for (auto it = statements.begin(); it != statements.end(); ++it)
	{
		LocalAssignmentStatement* lassy = *it;

		svluh.insert(lassy->resolve_property(parse));
	}

	return svluh;
}

void ClassDefinition::append_properties(Parser& parse, ObjectType* objtype)
{

	for (auto it = statements.begin(); it != statements.end(); ++it)
	{
		LocalAssignmentStatement* lassy = *it;

		std::pair<std::string, Value> pear = lassy->resolve_property(parse);

		objtype->set_typeproperty(parse, pear.first,pear.second);	
	}
}

Value Construction::resolve(Interpreter& interp)
{
	return interp.makeObject(type,args,this);
}

Value ParentAccess::resolve(Interpreter& interp)
{
	return interp.get_property(prop,this);
}

Value& ParentAccess::handle(Interpreter& interp)
{
	Object* o = interp.get_objectscope();
	if (!o)
	{
		interp.RuntimeError(this, "Cannot do ParentAccess in classless function!");
		return Value::dev_null;
	}

	return *(o->has_property(interp,prop));
}

Value GrandparentAccess::resolve(Interpreter& interp)
{
	return interp.grand_property(depth, prop, this);
}
Value& GrandparentAccess::handle(Interpreter& interp)
{
/*
	So we might be a function or a property.
	Lets find out!
*/
	return interp.grand_handle(depth, prop, this);
}


Value GlobalAccess::resolve(Interpreter& interp)
{
	return interp.get_global(var, this);
}

Value& GlobalAccess::handle(Interpreter& interp)
{
	return *interp.has_global(var, this);
}



Value IndexAccess::resolve(Interpreter& interp)
{
	Value& lhs = container->handle(interp);
	Value rhs = index->resolve(interp);

	switch (lhs.t_vType)
	{
	default:
		interp.RuntimeError(this, "Attempted to index a Value of type " + lhs.typestring() + "!");
		return Value();
	case(Value::vType::String):
		if (rhs.t_vType != Value::vType::Integer)
		{
			interp.RuntimeError(this, "Cannot index into a String with a Value of type " + rhs.typestring() + "!");
			return Value();
		}

		return Value(std::string({ lhs.t_value.as_string_ptr->at(rhs.t_value.as_int) })); // It's lines like these that remind me why I want to make my own programming language in the first place. Happy 1000th line, by the way!
	case(Value::vType::Object):
	{
		Object* obj = lhs.t_value.as_object_ptr;
		if (obj->is_table())
		{
			Table* la_table = static_cast<Table*>(obj);
			return la_table->at(interp, rhs);
		}

		if (rhs.t_vType != Value::vType::String)
		{
			interp.RuntimeError(this, "Cannot index into an Object with a Value of type " + rhs.typestring() + "!");
			return Value();
		}

		return obj->get_property(interp, *rhs.t_value.as_string_ptr);
	}
	}
}

Value& IndexAccess::handle(Interpreter& interp)
{
	Value& lhs = container->handle(interp);
	Value rhs = index->resolve(interp);

	if (lhs.t_vType != Value::vType::Object) // FIXME: Allow for index-based setting of string data
	{
		interp.RuntimeError(this, "Cannot get handle of anything except object properties!");
	}

	Object* obj = lhs.t_value.as_object_ptr;
	if (!obj->is_table())
		return MemberAccess(container, index).handle(interp); //FIXME: This is so fucked up but it makes so much sense; main issue is that runtimes will report themselves strangely
	//So it's a table then
	return static_cast<Table*>(obj)->at_ref(interp, rhs);
}