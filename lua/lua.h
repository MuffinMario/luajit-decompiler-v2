#include <fstream>

class Lua {
public:

	Lua(const Bytecode& bytecode, const Ast& ast, const std::string& filePath, const bool& forceOverwrite, const bool& minimizeDiffs, const bool& unrestrictedAscii, const bool& orderTableAlphabetic);
	~Lua();

	void operator()();

	const std::string filePath;

private:

	static constexpr char UTF8_BOM[] = "\xEF\xBB\xBF";
	static constexpr char NEW_LINE[] = "\r\n";

	void write_header();
	void write_block(const Function& function, const std::vector<Statement*>& block);
	void write_expression(const Expression& expression, const bool& useParentheses);
	void write_prefix_expression(const Expression& expression, const bool& isLineStart);
	void write_variable(const Variable& variable, const bool& isLineStart);
	void write_function_call(const FunctionCall& functionCall, const bool& isLineStart);
	void write_assignment(const std::vector<Variable>& variables, const std::vector<Expression*>& expressions, const std::string& separator, const bool& isLineStart);
	void write_expression_list(const std::vector<Expression*>& expressions, const Expression* const& multres);
	void write_function_definition(const Function& function, const bool& isMethod);
	void write_number(const double& number);
	void write_string(const std::string& string);
	uint8_t get_operator_precedence(const Expression& expression);
	void write(const std::string& string);
	template <typename... Strings>
	void write(const std::string& string, const Strings&... strings);
	void write_indent();
	void create_file();
	void close_file();
	void write_file();

	const Bytecode& bytecode;
	const Ast& ast;
	const bool forceOverwrite;
	const bool minimizeDiffs;
	const bool unrestrictedAscii;
	const bool orderTableAlphabetic;
	std::ofstream file;
	std::string writeBuffer;
	uint32_t indentLevel = 0;
	uint64_t prototypeDataLeft = 0;
};
