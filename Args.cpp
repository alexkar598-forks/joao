#include "Args.h"

#include "Scanner.h"
#include "Parser.h"
#include "Interpreter.h"

#ifdef __linux__ && __GNUG__
#include <strings.h>
#define _strcmpi strcasecmp
#endif

std::string Args::read_args(std::vector<Flags>& v, int argc, char** argv, int& file_start)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_strcmpi(argv[i], "-i") == 0)
		{
			v.push_back(Flags::Interactive);
			continue;
		}
		if (_strcmpi(argv[i], "-v") == 0)
		{
			v.push_back(Flags::Version);
			continue;
		}
		if (_strcmpi(argv[i], "-h") == 0)
		{
			v.push_back(Flags::Help);
			continue;
		}
		if (_strcmpi(argv[i], "-m") == 0)
		{
			v.push_back(Flags::Main);
			continue;
		}
		if (_strcmpi(argv[i], "-e") == 0)
		{
			v.push_back(Flags::Executetime);
			continue;
		}

		//If this isn't any of these argless flags then I guess it's the file
		file_start = i;
		return std::string(argv[i]);
	}
	return std::string();
}

void Args::print_version()
{
	std::cout << "Joao v" << VERSION_STRING <<"\tCopyright (C) 2021 Altoids1 (mom-puter@hotmail.com)\n";
}
void Args::print_help()
{
	std::cout << "Acceptable arguments: [option] [script [args]]\n";
	std::cout << "Available options are:\n";
	std::cout << "  -v\tshow version information\n";
	std::cout << "  -h\tshow help dialog\n";
	std::cout << "  -i\tenter interactive mode\n";
	std::cout << "  -m\tprints the output of /main() into stdout after program completion\n";
	std::cout << "  -e\tprints the execution time. On by default when compiled with _DEBUG.\n";
}

//Tries to run the given chunk of strings as if they were all statements in the /main(){} of a program with nothing else in it.
//Returns true if successful and false if an error occurred.
bool Args::run_code_block(std::vector<std::string*>& statements)
{
	const char* file_name = "__joao_interactive";

	std::fstream dummy_file = std::fstream(file_name, std::fstream::in | std::fstream::out | std::fstream::trunc);

	dummy_file << "/main(){\n";

	for (std::string* s : statements)
	{
		dummy_file << *(s) << std::endl;
	}

	dummy_file << "return 0;\n}\n";

	Scanner scn(true);
	scn.scan(dummy_file, file_name);

	if (scn.is_malformed)
		return false;

	Parser prs(scn);
	Program prog = prs.parse();

	if (prog.is_malformed)
		return false;

	Interpreter interp(prog,true);
	Value jargs = interp.makeBaseTable();
	interp.execute(prog, jargs);

	dummy_file.close();
	std::remove(file_name);
	return !(interp.did_error);
}

void Args::interactive_mode()
{
	std::vector<std::string*> statements;

	while (true)
	{
		std::cout << "> ";
		std::string input = "";
		std::getline(std::cin, input);

		//std::cout << input << std::endl;

		if (input == "")
			continue;
		if (input == "quit()")
			return;

		statements.push_back(new std::string(input));
		if (!run_code_block(statements))
		{
			statements.pop_back();
			std::cout << std::endl;
		}
	}
}