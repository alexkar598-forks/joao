#pragma once

//#define NDEBUG
#include <assert.h> 

#include "Forward.h"
#include "SharedEnums.h"

class Value { // A general pseudo-typeless Value used to store data within the programming language.

public:
	enum class vType : uint8_t {
		Null,
		Bool,
		Integer,
		Double,
		String,
		Object
	}t_vType{ vType::Null };

	union {
		bool as_bool;
		int as_int;
		double as_double;
		std::string* as_string_ptr;
		Object* as_object_ptr;
	}t_value;

	//Constructors
	Value()
	{
		t_value.as_int = 0;
	}
	~Value();
	Value(size_t i)
	{
		t_value.as_int = i;
		t_vType = vType::Integer;
	}
	Value(int i)
	{
		t_value.as_int = i;
		t_vType = vType::Integer;
	}
	Value(double d)
	{
		t_value.as_double = d;
		t_vType = vType::Double;
	}
	Value(bool b)
	{
		t_value.as_bool = b;
		t_vType = vType::Bool;
	}
	Value(std::string s)
	{
		std::string* our_str = new std::string(s);
		t_value.as_string_ptr = our_str;
		t_vType = vType::String;
		//std::cout << "Construction successful: " + *(t_value.as_string_ptr);
	}
	Value(Object* o)
	{
		t_value.as_object_ptr = o;
		t_vType = vType::Object;
	}

	Value(Value::vType vt, int errcode)
	{
		if (vt != Value::vType::Null)
			return;

		t_value.as_int = errcode;
	}

	explicit operator bool() const { // Q-q-quadruple keyword!!
		//std::cout << "Casting to bool...\n";
		switch (t_vType)
		{
		case(vType::Null):
			return false;
		case(vType::Bool):
			return t_value.as_bool;
		case(vType::Integer):
			return t_value.as_int;
		case(vType::Double):
			return t_value.as_double;
		default: // If it's a more complicated vType
			return true; // Just return true.
		}
	}

	std::string to_string();
	std::string typestring();
};

//Used during assignment, member access, and method/function calling. Stores a handle to either
//1. the name of a localish variable that needs to be get_var()'d
//2. the name of a property/method of the objectscope
//3. the name of a global/function in globalscope
//4. a pointer to an object someplace
struct Handle
{
	std::string name;

	bool is_function = false;
	union
	{
		Object* obj;
		ObjectType* objtype;
	}data;
	enum class HType {
		Invalid,
		Name,
		Parent,
		Global,
		Obj,
		ObjType
	} type;
	Handle()
	{
		name = "?????";
		type = HType::Invalid;
	}
	Handle(std::string& n)
	{
		name = n;
		type = HType::Name;
	}
	Handle(Object* o)
	{
		data.obj = o;
		type = HType::Obj;
	}
	void qdel()
	{
		return;
	}
};

class ASTNode // ASTNodes are abstract symbols which together form a "flow chart" tree of symbols that the parser creates from the text that the interpreter then interprets.
{

public:
	virtual const std::string class_name() const { return "ASTNode"; }

	// Collapses this symbol into a real dang thing or process that the interpreter can do.
	virtual Value resolve(Interpreter&); 

	// Attempts to collapse this symbol in a constant and scope-less manner. Bool determines if it throws an error when this fails or not.
	virtual Value const_resolve(Parser&, bool);

	// Returns a Handle, typically used in the context of resolving a complex assignment, i.e. 'apple.taste.type = "succulent"'
	virtual Handle handle(Interpreter&);

	// Returns a Handle in a constant and scope-less manner. Will always throw an error if there's an error, which for most ASTNodes there would be.
	virtual Handle const_handle(Parser&);

	virtual std::string dump(int indent) { return std::string(indent, ' ') + class_name() + "\n"; } // Used for debugging

	virtual bool is_expression() { return false; }
};

class Literal : public ASTNode { // A node which denotes a plain ol' literal.
	Value heldval;
public:
	Literal(Value V)
		:heldval(V)
	{
		//std::cout << "Constructing literal...\n";
	}

	virtual Value resolve(Interpreter&) override;
	virtual Value const_resolve(Parser&, bool) override;
	virtual std::string dump(int indent) { return std::string(indent, ' ') + "Literal: " + heldval.to_string() + "\n"; }
};

// Expressions are ASTNodes that make sense to be done on their lonesome as a statement w/o other context.
class Expression : public ASTNode
{

	virtual bool is_expression() override  { return true; }
};

class Identifier : public ASTNode
{
	std::string t_name;
public:
	Identifier(std::string s)
		:t_name(s)
	{
		//std::cout << "I've been created with name " + s + "!\n";
	}
	std::string get_str()
	{
		return t_name;
	}

	virtual Value resolve(Interpreter&) override;
	virtual Handle handle(Interpreter&) override;
	virtual Handle const_handle(Parser& parse) override { return Handle(t_name); }
	
	virtual const std::string class_name() const override { return "Identifier"; }
	virtual std::string dump(int indent) { return std::string(indent, ' ') + "Identifier: " + t_name + "\n"; }
};

class AssignmentStatement : public Expression
{
protected:
	ASTNode* id;
	ASTNode* rhs;
public:
	enum class aOps : uint8_t {
		NoOp, // More a Parser abstraction than anything else
		Assign, // =
		AssignAdd, // +=
		AssignSubtract, // -=
		AssignMultiply, // *=
		AssignDivide // /=
	}t_op;
	AssignmentStatement(ASTNode* i, ASTNode* r, aOps o = aOps::Assign)
		:id(i),
		rhs(r),
		t_op(o)
	{
		//std::cout << "My identifier has the name " + id->get_str() + "!\n";
	}
	virtual Value resolve(Interpreter&) override;
	virtual std::string dump(int indent)
	{ 
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "AssignmentStatement, Operation: Assign\n";

		str += id->dump(indent + 1);
		str += rhs->dump(indent + 1);

		return str;
	}
};

class LocalAssignmentStatement final : public AssignmentStatement // "Value x = 3;", which has a distinct ASTNode type from "x = 3;"
{
	LocalType ty = LocalType::Value;
public:
	LocalAssignmentStatement(Identifier* i, ASTNode* r, aOps o = aOps::Assign)
		:AssignmentStatement(i,r,o)
	{
		//std::cout << "My identifier has the name " + id->get_str() + "!\n";
	}

	virtual Value resolve(Interpreter&) override;
	virtual Value const_resolve(Parser&, bool) override;

	//A special-snowflake resolver for LocalAssignmentStatement which returns the key-value pair of the property it describes.
	std::pair<std::string, Value> resolve_property(Parser&);

	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "LocalAssignmentStatement, Type: Value, Operation: Assign\n";

		str += id->dump(indent + 1);
		str += rhs->dump(indent + 1);

		return str;
	}
};

class UnaryExpression : public Expression
{
	public:
	enum class uOps : uint8_t {
		NoOp,
		Not,
		Negate,
		BitwiseNot,
		Length
	}t_op;
	ASTNode* t_rhs;
	UnaryExpression(uOps Operator, ASTNode* r)
		:t_op(Operator)
		,t_rhs(r)
	{

	}
	virtual Value resolve(Interpreter&) override;
};

class BinaryExpression : public Expression 
{ // An ASTNode which is an operation between two, smaller Expressions.

public:
	enum class bOps : uint8_t {
		NoOp, // Used by the parser to store what is basically a null into one of these bOps enums.
		//
		Add,
		Subtract,
		Multiply,
		Divide,
		//
		FloorDivide,
		Exponent,
		Modulo,
		//
		BitwiseAnd,
		BitwiseXor,
		BitwiseOr,
		//
		ShiftRight,
		ShiftLeft,
		//
		Concatenate,
		//
		LessThan,
		LessEquals,
		Greater,
		GreaterEquals,
		Equals,
		NotEquals,
		//
		LogicalAnd,
		LogicalOr,
		LogicalXor
	}t_op;
	ASTNode* t_lhs, *t_rhs;

	BinaryExpression(bOps Operator, ASTNode* l, ASTNode* r)
		:t_lhs(l) // What the fuck is this syntax holy shit
		,t_rhs(r)
		,t_op(Operator)
	{
		
	}
	virtual Value resolve(Interpreter&) override;
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string sop;
		switch (t_op)
		{
		case(bOps::Add):
			sop = "Add";
			break;
		case(bOps::Subtract):
			sop = "Subtract";
			break;
		case(bOps::Multiply):
			sop = "Multiply";
			break;
		case(bOps::Divide):
			sop = "Divide";
			break;
		//
		case(bOps::Exponent):
			sop = "Exponent";
			break;
		//
		case(bOps::Concatenate):
			sop = "Concatenate";
			break;
		//
		case(bOps::Equals):
			sop = "Equals";
			break;
		default:
			sop = "????";
		}
		std::string str = ind + "BinaryStatement, Operation: " + sop + "\n";
		
		str += t_lhs->dump(indent + 1);
		str += t_rhs->dump(indent + 1);

		return str;
	}
};

class ReturnStatement : public Expression {
	ASTNode *held_expr;
public:
	bool has_expr{ false };

	virtual const std::string class_name() const override { return "ReturnStatement"; }

	ReturnStatement()
		:held_expr(&Literal(Value()))
	{

	}
	ReturnStatement(ASTNode* node)
		:held_expr(node)
	{
		has_expr = true;
	}
	virtual Value resolve(Interpreter& interp) override
	{
		return held_expr->resolve(interp);
	}
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "ReturnStatement:\n";

		str += held_expr->dump(indent + 1);

		return str;
	}
};

class CallExpression : public Expression {
	ASTNode* func_expr;

	std::vector<ASTNode*> args;
public:
	CallExpression(ASTNode* f)
		:func_expr(f)
	{

	}
	CallExpression(ASTNode* f, std::vector<ASTNode*> arr)
		:func_expr(f),
		args(arr) // Arr!!
	{

	}
	CallExpression* append_arg(ASTNode* expr)
	{
		args.push_back(expr);
		return this;
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "CallExpression"; }
	virtual std::string dump(int indent) override {
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "CallExpression\n";
		str += ind +"@Function:\n" + func_expr->dump(indent+1);
		str += ind + "(Args:\n";
		for (int i = 0; i < args.size(); ++i)
		{
			str += args[i]->dump(indent + 1);
		}
		str += ind + ")\n";
		return str;
	}
};

class Function : public ASTNode
{
protected:
	Value returnValue = Value(); // My return value
	std::vector<Expression*> statements; // The statements which're executed when I am run
	std::vector<Value> t_args;
	std::vector<std::string> t_argnames;
	//std::vector<Expression> args;
	std::string t_name; // My name
	Object* obj = nullptr;

	Function()
	{

	}
public:
	Function(std::string name, Expression* expr)
	{
		statements = std::vector<Expression*>{ expr };
	}
	Function(std::string name, std::vector<Expression*> exprs) // argument-less-ness
	{
		t_name = name;
		statements = exprs;
	}
	Function(std::string name, std::vector<Expression*> &exprs, std::vector<std::string>& sargs) // Hopefully this works. :(
	{
		t_name = name;
		statements = exprs;
		t_argnames = sargs;
	}
	void give_args(Interpreter&, std::vector<Value>&, Object*);
	Function* append(Expression* expr)
	{
		statements.push_back(expr);
		return this;
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "Function"; }
	virtual std::string dump(int indent) override
	{
		//std::string ind = std::string(indent, ' ');
		std::string str = "Function, name: " + t_name + "\n";

		str += "@Params:\n";
		for (int i = 0; i < t_argnames.size(); ++i)
		{
			str += " " + t_argnames[i] + "\n";
		}

		str += "=Statements:\n";
		for (int i = 0; i < statements.size(); ++i)
		{
			str += statements[i]->dump(indent + 1);
		}

		return str;
	}
};

class NativeFunction final : public Function
{
	/*
	So this kinda overrides a lot of the typical behavior of a function, instead deferring it into some kickass lambda stored within.
	*/
	Value(*lambda)(std::vector<Value>) = nullptr;
	// Value lambda() {};
public:
	NativeFunction(std::string n)
	{
		t_name = n;
		lambda = [](std::vector<Value> args)
		{
			return Value();
		};
	}
	NativeFunction(std::string n, Value(*luh)(std::vector<Value>))
	{
		t_name = n;
		lambda = luh;
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "NativeFunction"; }
	virtual std::string dump(int indent) override
	{
		return "NativeFunction, name: " + t_name + "\n";
	}
};


class Block : public Expression
{
protected:
	std::vector<Expression*> statements;

	Value iterate_statements(Interpreter&);
};

class IfBlock final : public Block
{
	ASTNode* condition = nullptr;
	IfBlock* Elseif = nullptr;
public:
	IfBlock(std::vector<Expression*>& st)
	{
		statements = st;
	}
	IfBlock(ASTNode* cond, std::vector<Expression*>& st)
		:condition(cond)
	{
		statements = st;
	}

	void append_else(IfBlock* elif)
	{
		Elseif = elif;
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "IfBlock"; }
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "IfBlock\n";

		str += ind + "?Cond:\n" + condition->dump(indent + 2);

		for (int i = 0; i < statements.size(); ++i)
		{
			str += statements[i]->dump(indent + 1);
		}

		return str;
	}
};

class ForBlock final : public Block
{
	ASTNode* initializer = nullptr;
	ASTNode* condition = nullptr;
	ASTNode* increment = nullptr;
	
public:
	ForBlock(std::vector<Expression*>& st)
	{
		statements = st;
	}
	ForBlock(ASTNode* init, ASTNode* cond, ASTNode* inc, std::vector<Expression*>& st)
		:condition(cond),
		initializer(init),
		increment(inc)
	{
		statements = st;
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "ForBlock"; }
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "ForBlock\n";
		
		str += ind + "=Init:\n" + initializer->dump(indent + 2);
		str += ind + "?Cond:\n" + condition->dump(indent + 2);
		str += ind + "+Inc:\n" + increment->dump(indent + 2);

		for (int i = 0; i < statements.size(); ++i)
		{
			str += statements[i]->dump(indent + 1);
		}

		return str;
	}
};

class WhileBlock final : public Block
{
	ASTNode* condition = nullptr;
public:
	WhileBlock(std::vector<Expression*>& st)
	{
		statements = st;
	}
	WhileBlock(ASTNode* cond, std::vector<Expression*>& st)
		:condition(cond)
	{
		statements = st;
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "WhileBlock"; }
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "WhileBlock\n";

		str += ind + "?Cond:\n" + condition->dump(indent + 2);

		for (int i = 0; i < statements.size(); ++i)
		{
			str += statements[i]->dump(indent + 1);
		}

		return str;
	}
};


class BreakStatement final : public Expression
{
	int breaknum;
public:
	BreakStatement(int br = 1)
		:breaknum(br)
	{
	}

	virtual Value resolve(Interpreter&) override;
	virtual const std::string class_name() const override { return "BreakStatement"; }
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "Break " + std::to_string(breaknum) + ";\n";

		return str;
	}
};

class MemberAccess final : public ASTNode
{
	ASTNode* front;
	ASTNode* back;
public:
	MemberAccess(ASTNode* f, ASTNode* b)
		:front(f)
		 ,back(b)
	{

	}

	virtual Value resolve(Interpreter&) override;

	//virtual Handle handle(Interpreter&) override;

	virtual const std::string class_name() const override { return "MemberAccess"; }
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "MemberAccess\n";

		str += front->dump(indent + 1);
		str += back->dump(indent + 1);

		return str;
	}



};

class ClassDefinition final : public ASTNode
{
	std::vector<LocalAssignmentStatement*> statements;
public:
	std::string direct;
	ClassDefinition(std::string& d, std::vector<LocalAssignmentStatement*> &s)
		:direct(d),
		statements(s)
	{

	}

	//virtual Value resolve(Interpreter&) override;

	std::unordered_map<std::string, Value> resolve_properties(Parser&);

	virtual const std::string class_name() const override { return "ClassDefinition"; }
	virtual std::string dump(int indent)
	{
		std::string ind = std::string(indent, ' ');
		std::string str = ind + "ClassDefinition " + direct + ";\n";
		for (int i = 0; i < statements.size(); ++i)
		{
			str += statements[i]->dump(indent + 1);
		}
		return str;
	}
};

class Construction : public ASTNode
{

	std::string type;
	std::vector<ASTNode*> args;
public:
	Construction(std::string t, std::vector<ASTNode*> a)
		:type(t)
		,args(a)
	{

	}

	virtual const std::string class_name() const override { return "Construction"; }
	virtual std::string dump(int indent) override
	{
		return std::string(indent, ' ') + "Construction, type: " + type + "\n";
	}
	virtual Value resolve(Interpreter&) override;
};

class ParentAccess : public ASTNode
{
public:
	std::string prop;

	ParentAccess(std::string p)
		:prop(p)
	{

	}
	virtual const std::string class_name() const override { return "ParentAccess"; }
	virtual std::string dump(int indent) override
	{
		return std::string(indent, ' ') + "ParentAccess, property: " + prop + "\n";
	}
	virtual Value resolve(Interpreter&) override;
	virtual Handle handle(Interpreter&) override;
};

class GrandparentAccess : public ASTNode
{
	unsigned int depth;
	std::string prop;

public:
	GrandparentAccess(unsigned int d,std::string p)
		:prop(p)
		,depth(d)
	{

	}
	virtual const std::string class_name() const override { return "GrandparentAccess"; }
	virtual std::string dump(int indent) override
	{
		return std::string(indent, ' ') + "GrandparentAccess, property: " + prop + "; depth: " + std::to_string(depth) + "\n";
	}
	virtual Value resolve(Interpreter&) override;
	virtual Handle handle(Interpreter&) override;
};

class GlobalAccess : public ASTNode
{
public:
	std::string var;

	GlobalAccess(std::string v)
		:var(v)
	{

	}
	virtual const std::string class_name() const override { return "GlobalAccess"; }
	virtual std::string dump(int indent) override
	{
		return std::string(indent, ' ') + "GlobalAccess, property: " + var + "\n";
	}
	virtual Value resolve(Interpreter&) override;
	virtual Handle handle(Interpreter&) override;
};