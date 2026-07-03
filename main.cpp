#include "main.h"
#include <filesystem> // std::filesystem cross platform utility
#include <cstring>	  // std::strcmp
#include <algorithm>
#include <cctype>

struct Error
{
	const std::string message;
	const std::string filePath;
	const std::string function;
	const std::string source;
	const std::string line;
};
#ifdef __linux__

using HandleType = FILE *;
using PathAttributeType = struct stat;

using FlagType = int;

const char PATH_SEPARATOR = '/';

const int INVALID_HANDLE = -1;

#elif _WIN32

using HandleType = HANDLE;
using PathAttributeType = DWORD;

using FlagType = DWORD;

const char PATH_SEPARATOR = '\\';

const typeof(INVALID_FILE_HANDLE) INVALID_HANDLE = INVALID_FILE_HANDLE; // should be HANDLE -> LPVOID
#endif

std::string input();

inline std::string trim(const std::string &s)
{
   auto wsfront=std::find_if_not(s.begin(),s.end(),[](int c){return std::isspace(c);});
   auto wsback=std::find_if_not(s.rbegin(),s.rend(),[](int c){return std::isspace(c);}).base();
   return (wsback<=wsfront ? std::string() : std::string(wsfront,wsback));
}


static bool is_directory(PathAttributeType &attribute)
{
#ifdef _WIN32
	return attribute & FILE_ATTRIBUTE_DIRECTORY;
#else
	return S_ISDIR(attribute.st_mode) != 0;
#endif
}

static bool is_file(PathAttributeType &attribute)
{
#ifdef _WIN32
	return attribute & INVALID_FILE_ATTRIBUTES == 0 && is_directory(attribute) == false;
#else
	return S_ISREG(attribute.st_mode) != 0;
#endif
}

#ifdef __linux__
static const HandleType CONSOLE_OUTPUT = stdout;
static const HandleType CONSOLE_INPUT = stdin;
#elif _WIN32
static const HandleType CONSOLE_OUTPUT = GetStdHandle(STD_OUTPUT_HANDLE);
static const HandleType CONSOLE_INPUT = GetStdHandle(STD_INPUT_HANDLE);
#endif
static bool isCommandLine;
static bool isProgressBarActive = false;
static uint32_t filesSkipped = 0;

void WriteToConsole(HandleType h, const char *msg, size_t msglen)
{
#ifdef __linux__
	auto fd = fileno(h);
	if (fd == -1)
	{
		perror("fileno failure when writing to console");
		return;
	}

	auto wr = write(fd, msg, msglen);
	if (wr == -1)
	{
		perror("write failure when writing to console");
		return;
	}
#elif _WIN32
	WriteConsoleA(h, msg, msglen, NULL, NULL);
#endif
}

static struct
{
	bool showHelp = false;
	bool silentAssertions = false;
	bool forceOverwrite = false;
	bool ignoreDebugInfo = false;
	bool minimizeDiffs = false;
	bool unrestrictedAscii = false;
	bool orderTableAlphabetic = false;
	std::string inputPath;
	std::string outputPath;
	std::string extensionFilter;
} arguments;

struct Directory
{
	const std::string path;
	std::vector<Directory> folders;
	std::vector<std::string> files;
};

static std::string string_to_lowercase(const std::string &string)
{
	std::string lowercaseString = string;

	for (uint32_t i = lowercaseString.size(); i--;)
	{
		if (lowercaseString[i] < 'A' || lowercaseString[i] > 'Z')
			continue;
		lowercaseString[i] += 'a' - 'A';
	}

	return lowercaseString;
}

static std::string &append_file_separator(std::string &str)
{

	switch (str.back())
	{
	case '/':
#ifdef _WIN32
	case '\\':
#endif
		break;
	default:
		str += PATH_SEPARATOR;
		break;
	}

	return str;
}

static void find_files_recursively(Directory &directory)
{
#ifdef _WIN32
#ifdef NO_STD_FILESYSTEM_USE
	WIN32_FIND_DATAA pathData;
	// find first file in directory, if invalid (none) stop recursion
	HANDLE handle = FindFirstFileA((arguments.inputPath + directory.path + '*').c_str(), &pathData);
	if (handle == INVALID_HANDLE_VALUE)
		return;

	// iterate over all files
	do
	{
		// check if file is a directory
		if (pathData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// skip "current folder", "folder below"
			if (!std::strcmp(pathData.cFileName, ".") || !std::strcmp(pathData.cFileName, ".."))
				continue;
			// add folder to dir and enter next recursion step with it
			directory.folders.emplace_back(Directory{.path = directory.path + pathData.cFileName + PATH_SEPARATOR});
			find_files_recursively(directory.folders.back());

			// if no files/folders inside this folder have been found, remove again
			if (!directory.folders.back().files.size() && !directory.folders.back().folders.size())
				directory.folders.pop_back();
			continue;
		}

		if (!arguments.extensionFilter.size() || arguments.extensionFilter == string_to_lowercase(PathFindExtensionA(pathData.cFileName)))
			directory.files.emplace_back(pathData.cFileName);
	} while (FindNextFileA(handle, &pathData));

	FindClose(handle);
	return;
#endif
#endif
	const auto curPath = arguments.inputPath + directory.path;
	for (const auto &fileEntry : std::filesystem::directory_iterator(curPath))
	{
		if (fileEntry.is_directory())
		{
			const std::string filename = fileEntry.path().filename().string();
			// skip "current folder", "folder below"
			if (!std::strcmp(filename.c_str(), ".") || !std::strcmp(filename.c_str(), ".."))
				continue;
			// add folder to dir and enter next recursion step with it
			directory.folders.emplace_back(Directory{.path = directory.path + filename + PATH_SEPARATOR});
			find_files_recursively(directory.folders.back());

			// if no files/folders inside this folder have been found, remove again
			if (!directory.folders.back().files.size() && !directory.folders.back().folders.size())
				directory.folders.pop_back();
			continue;
		}
		const std::string filename = fileEntry.path().filename().string();
		const auto fileExt = fileEntry.path().extension();
		if (!arguments.extensionFilter.size() || arguments.extensionFilter == string_to_lowercase(fileExt))
			directory.files.emplace_back(filename);
	}
}

static bool decompile_files_recursively(const Directory &directory)
{
	const std::string newDirName = arguments.outputPath + directory.path;

#if defined(_WIN32) && defined(NO_STD_FILESYSTEM_USE)
	CreateDirectoryA((arguments.outputPath + directory.path).c_str(), NULL);
#else
	std::filesystem::create_directory(newDirName);
#endif

	std::string outputFile;

	for (uint32_t i = 0; i < directory.files.size(); i++)
	{
		outputFile = directory.files[i];
#ifdef _WIN32
		PathRemoveExtensionA(outputFile.data());
#else
		outputFile = lnx::stripExtension(outputFile);
#endif
		outputFile = outputFile.c_str();
		outputFile += ".lua";

		Bytecode bytecode(arguments.inputPath + directory.path + directory.files[i]);
		Ast ast(bytecode, arguments.ignoreDebugInfo, arguments.minimizeDiffs);
		Lua lua(bytecode, ast, arguments.outputPath + directory.path + outputFile, arguments.forceOverwrite, arguments.minimizeDiffs, arguments.unrestrictedAscii, arguments.orderTableAlphabetic);

		try
		{
			print("--------------------\nInput file: " + bytecode.filePath + "\nReading bytecode...");
			bytecode();
			print("Building ast...");
			ast();
			print("Writing lua source...");
			lua();
			print("Output file: " + lua.filePath);
		}
		catch (const Error &assertion)
		{
			erase_progress_bar();

			if (arguments.silentAssertions)
			{
				print("\nError running " + assertion.function + "\nSource: " + assertion.source + ":" + assertion.line + "\n\n" + assertion.message);
				filesSkipped++;
				continue;
			}
#ifdef _WIN32
			switch (MessageBoxA(NULL, ("Error running " + assertion.function + "\nSource: " + assertion.source + ":" + assertion.line + "\n\nFile: " + assertion.filePath + "\n\n" + assertion.message).c_str(),
								PROGRAM_NAME, MB_ICONERROR | MB_CANCELTRYCONTINUE | MB_DEFBUTTON3))
			{
			case IDCANCEL:
				return false;
			case IDTRYAGAIN:
				print("Retrying...");
				i--;
				continue;
			case IDCONTINUE:
				print("File skipped.");
				filesSkipped++;
			}
#else
			print("\nError running " + assertion.function + "\nSource: " + assertion.source + ":" + assertion.line + "\n\nFile: " + assertion.filePath + "\n\n" + assertion.message);
			std::string r;
			do
			{
				print("\nRetry? [Y]es/[N]o/[S]top >");
				r = trim(input());
				std::transform(r.begin(), r.end(), r.begin(), [](char c)
							   { return std::tolower(c); });
			} while (r != "y" && r != "s" && r != "n");

			switch (r.at(0))
			{

			case 's':
				return false;
			case 'y':
				print("Retrying...");
				i--;
				continue;
			case 'n':
				print("File skipped.");
				filesSkipped++;
			}

#endif
		}
		catch (...)
		{
			std::string err = std::string("Unknown exception\n\nFile: " + bytecode.filePath);
#if _WIN32
			MessageBoxA(NULL, err.c_str(), PROGRAM_NAME, MB_ICONERROR | MB_OK);
#else
			perror(err.c_str());
#endif
			throw;
		}
	}

	for (uint32_t i = 0; i < directory.folders.size(); i++)
	{
		if (!decompile_files_recursively(directory.folders[i]))
			return false;
	}

	return true;
}

static char *parse_arguments(const int &argc, char **const &argv)
{
	if (argc < 2)
		return nullptr;
	arguments.inputPath = argv[1];
#ifndef _DEBUG
	// support for file drag on windows exclusive behavior, on linux this breaks when no tty is applied
#ifdef _WIN32
	if (!isCommandLine)
		return nullptr;
#endif
#endif
	bool isInputPathSet = true;

	if (arguments.inputPath.size() && arguments.inputPath.front() == '-')
	{
		arguments.inputPath.clear();
		isInputPathSet = false;
	}

	std::string argument;

	for (uint32_t i = isInputPathSet ? 2 : 1; i < argc; i++)
	{
		argument = argv[i];

		if (argument.size() >= 2 && argument.front() == '-') {
			if (argument[1] == '-') {
				argument = argument.c_str() + 2;

				if (argument == "extension")
				{
					if (i <= argc - 2)
					{
						i++;
						arguments.extensionFilter = argv[i];
						continue;
					}
				}
				else if (argument == "force_overwrite")
				{
					arguments.forceOverwrite = true;
					continue;
				}
				else if (argument == "help")
				{
					arguments.showHelp = true;
					continue;
				}
				else if (argument == "ignore_debug_info")
				{
					arguments.ignoreDebugInfo = true;
					continue;
				}
				else if (argument == "minimize_diffs")
				{
					arguments.minimizeDiffs = true;
					continue;
				} else if (argument == "output") {
					if (i <= argc - 2) {
						i++;
						arguments.outputPath = argv[i];
						continue;
					}
				}
				else if (argument == "silent_assertions")
				{
					arguments.silentAssertions = true;
					continue;
				}
				else if (argument == "unrestricted_ascii")
				{
					arguments.unrestrictedAscii = true;
					continue;
				}
				else if (argument == "order_table_alphabetic")
				{
					arguments.orderTableAlphabetic = true;
					continue;
				}
			}
			else if (argument.size() == 2)
			{
				switch (argument[1])
				{
				case 'e':
					if (i > argc - 2)
						break;
					i++;
					arguments.extensionFilter = argv[i];
					continue;
				case 'f':
					arguments.forceOverwrite = true;
					continue;
				case '?':
				case 'h':
					arguments.showHelp = true;
					continue;
				case 'i':
					arguments.ignoreDebugInfo = true;
					continue;
				case 'm':
					arguments.minimizeDiffs = true;
					continue;
				case 'o':
					if (i > argc - 2)
						break;
					i++;
					arguments.outputPath = argv[i];
					continue;
				case 's':
					arguments.silentAssertions = true;
					continue;
				case 'u':
					arguments.unrestrictedAscii = true;
					continue;
				case 't':
					arguments.orderTableAlphabetic = true;
					continue;
				}
			}
		}

		return argv[i];
	}

	return nullptr;
}

static void wait_for_exit()
{
	if (isCommandLine)
		return;
	print("Press enter to exit.");
	input();
}

int main(int argc, char *argv[])
{
	print(std::string(PROGRAM_NAME) + "\nCompiled on " + __DATE__);

#ifdef __linux__
	{
#ifdef _DEBUG
		isCommandLine = false;
#else
		// determine it being in a command line if file descriptors
		// are attached to a terminal device
		isCommandLine = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
	}
#elif _WIN32
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	{
		HWND window = GetConsoleWindow();
		DWORD consoleProcessId;
		GetWindowThreadProcessId(window, &consoleProcessId);
#ifdef _DEBUG
		isCommandLine = false;
#else
		isCommandLine = consoleProcessId != GetCurrentProcessId();
		if (!isCommandLine) SetWindowTextA(window, PROGRAM_NAME);
#endif
	}

#endif

	if (parse_arguments(argc, argv))
	{
		print("Invalid argument: " + std::string(parse_arguments(argc, argv)) + "\nUse -? to show usage and options.");
		return EXIT_FAILURE;
	}

	if (arguments.showHelp)
	{
		print(
			"Usage: luajit-decompiler-v2.exe INPUT_PATH [options]\n"
			"\n"
			"Available options:\n"
			"  -h, -?, --help\t\tShow this message\n"
			"  -o, --output OUTPUT_PATH\tOverride default output directory\n"
			"  -e, --extension EXTENSION\tOnly decompile files with the specified extension\n"
			"  -s, --silent_assertions\tDisable assertion error pop-up window\n"
			"\t\t\t\t  and auto skip files that fail to decompile\n"
			"  -f, --force_overwrite\t\tAlways overwrite existing files\n"
			"  -i, --ignore_debug_info\tIgnore bytecode debug info\n"
			"  -m, --minimize_diffs\t\tOptimize output formatting to help minimize diffs\n"
			"  -u, --unrestricted_ascii\tDisable default UTF-8 encoding and string restrictions\n"
			"  -t, --order_table_alphabetic\tOutput table keys sorted alphabetically");
		return EXIT_SUCCESS;
	}

	if (!arguments.inputPath.size())
	{
		print("No input path specified!");
#ifdef _WIN32
		if (isCommandLine)
			return EXIT_FAILURE;
		arguments.inputPath.resize(MAX_PATH, NULL);
		OPENFILENAMEA dialogInfo = {
			.lStructSize = sizeof(OPENFILENAMEA),
			.hwndOwner = NULL,
			.lpstrFilter = NULL,
			.lpstrCustomFilter = NULL,
			.lpstrFile = arguments.inputPath.data(),
			.nMaxFile = (DWORD)arguments.inputPath.size(),
			.lpstrFileTitle = NULL,
			.lpstrInitialDir = NULL,
			.lpstrTitle = PROGRAM_NAME,
			.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
			.lpstrDefExt = NULL,
			.FlagsEx = NULL};
		print("Please select a valid LuaJIT bytecode file.");
		if (!GetOpenFileNameA(&dialogInfo))
			return EXIT_FAILURE;
		arguments.inputPath = arguments.inputPath.c_str();

#else
		// don't offer file open dialog outside of windows os
		return EXIT_FAILURE;

#endif
	}

	PathAttributeType pathAttributes;

	if (!arguments.outputPath.size()) // output path is empty
	{
#ifdef __linux__
	//arguments.outputPath = lnx::getExecutablePath() + "/output/";
		arguments.outputPath = lnx::getCWD() + "/output/";
		auto mres = mkdir(arguments.outputPath.c_str(),0755);
		if ((mres != 0 && errno != EEXIST) || stat(arguments.outputPath.c_str(), &pathAttributes) != 0)
		{
			perror("stat() call failed after creating dir or mkdir failed");
			return EXIT_FAILURE;
		}

#elif _WIN32
		arguments.outputPath.resize(MAX_PATH);
		GetModuleFileNameA(NULL, arguments.outputPath.data(), arguments.outputPath.size());
		*PathFindFileNameA(arguments.outputPath.data()) = '\x00';
		arguments.outputPath = arguments.outputPath.c_str();
		arguments.outputPath += "output\\";
		arguments.outputPath.shrink_to_fit();
#endif
	}
	else
	{
#ifdef _WIN32
		pathAttributes = GetFileAttributesA(arguments.outputPath.c_str());

		if (pathAttributes == INVALID_FILE_ATTRIBUTES)
		{
			print("Failed to open output path: " + arguments.outputPath);
			return EXIT_FAILURE;
		}

		if (!(pathAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			print("Output path is not a folder!");
			return EXIT_FAILURE;
		}
#else
		auto mres = mkdir(arguments.outputPath.c_str(),0755);
		
		if ((mres != 0 && errno != EEXIST) || stat(arguments.outputPath.c_str(), &pathAttributes) != 0)
		{
			perror("stat() call failed");
			return EXIT_FAILURE;
		}
		if (S_ISDIR(pathAttributes.st_mode) == 0)
		{
			perror("Output path is not a folder!");
			return EXIT_FAILURE;
		}
#endif

		// Append path separator in the end if not present
		append_file_separator(arguments.outputPath);
	}

	if (arguments.extensionFilter.size())
	{
		if (arguments.extensionFilter.front() != '.')
			arguments.extensionFilter.insert(arguments.extensionFilter.begin(), '.');
		arguments.extensionFilter = string_to_lowercase(arguments.extensionFilter);
	}

#ifdef _WIN32
	pathAttributes = GetFileAttributesA(arguments.inputPath.c_str());

	if (pathAttributes == INVALID_FILE_ATTRIBUTES)
	{
		print("Failed to open input path: " + arguments.inputPath);
		wait_for_exit();
		return EXIT_FAILURE;
	}
#else

	if (stat(arguments.inputPath.c_str(), &pathAttributes) != 0)
	{
		perror("stat() call failed");
		return EXIT_FAILURE;
	}
	if (S_ISDIR(pathAttributes.st_mode) == 0 && S_ISREG(pathAttributes.st_mode) == 0)
	{

		print("Failed to open input path: " + arguments.inputPath);
		wait_for_exit();
		return EXIT_FAILURE;
	}
#endif

	Directory root;

	if (is_directory(pathAttributes)) // dirtype
	{
		append_file_separator(arguments.inputPath);

		find_files_recursively(root);

		if (!root.files.size() && !root.folders.size())
		{
			print("No files " + (arguments.extensionFilter.size() ? "with extension " + arguments.extensionFilter + " " : "") + "found in path: " + arguments.inputPath);
			wait_for_exit();
			return EXIT_FAILURE;
		}
	}
	else // filetype
	{
#ifdef _WIN32
		root.files.emplace_back(PathFindFileNameA(arguments.inputPath.c_str()));
		*PathFindFileNameA(arguments.inputPath.c_str()) = '\x00';
		arguments.inputPath = arguments.inputPath.c_str();
#else
		std::string filename = lnx::getPathFileName(arguments.inputPath);
		std::string path = lnx::getPathDirectory(arguments.inputPath);
		root.files.push_back(filename);
		arguments.inputPath = path;
#endif
	}

	try
	{
		if (!decompile_files_recursively(root))
		{
			print("--------------------\nAborted!");
			wait_for_exit();
			return EXIT_FAILURE;
		}
	}
	catch (...)
	{
		throw;
	}

#ifndef _DEBUG
	print("--------------------\n" + (filesSkipped ? "Failed to decompile " + std::to_string(filesSkipped) + " file" + (filesSkipped > 1 ? "s" : "") + ".\n" : "") + "Done!");
	wait_for_exit();
#endif
	return EXIT_SUCCESS;
}

void print(const std::string &message)
{
	WriteToConsole(CONSOLE_OUTPUT, (message + '\n').data(), message.size() + 1);
}

std::string input()
{
	static char BUFFER[1024];
#ifdef __linux__

	auto fd = fileno(CONSOLE_INPUT);
	if (fd == -1)
	{

		perror("Cannot get console input file descriptor to capture input");
		return "";
	}
	tcflush(fd, TCIFLUSH);
	return fgets(BUFFER, sizeof(BUFFER), CONSOLE_INPUT);
#elif _WIN32
	FlushConsoleInputBuffer(CONSOLE_INPUT);
	DWORD charsRead;
	return ReadConsoleA(CONSOLE_INPUT, BUFFER, sizeof(BUFFER), &charsRead, NULL) && charsRead > 2 ? std::string(BUFFER, charsRead - 2) : "";
#endif
}

void print_progress_bar(const double &progress, const double &total)
{
	static char PROGRESS_BAR[] = "\r[====================]";

	const uint8_t threshold = std::round(20 / total * progress);

	for (uint8_t i = 20; i--;)
	{
		PROGRESS_BAR[i + 2] = i < threshold ? '=' : ' ';
	}

	WriteToConsole(CONSOLE_OUTPUT, PROGRESS_BAR, sizeof(PROGRESS_BAR) - 1);
	isProgressBarActive = true;
}

void erase_progress_bar()
{
	static constexpr char PROGRESS_BAR_ERASER[] = "\r                      \r";

	if (!isProgressBarActive)
		return;
	WriteToConsole(CONSOLE_OUTPUT, PROGRESS_BAR_ERASER, sizeof(PROGRESS_BAR_ERASER) - 1);
	isProgressBarActive = false;
}

void assert(const bool &assertion, const std::string &message, const std::string &filePath, const std::string &function, const std::string &source, const uint32_t &line)
{
	if (!assertion)
		throw Error{
			.message = message,
			.filePath = filePath,
			.function = function,
			.source = source,
			.line = std::to_string(line)};
}

std::string byte_to_string(const uint8_t &byte)
{
	char string[] = "0x00";
	uint8_t digit;

	for (uint8_t i = 2; i--;)
	{
		digit = (byte >> i * 4) & 0xF;
		string[3 - i] = digit >= 0xA ? 'A' + digit - 0xA : '0' + digit;
	}

	return string;
}
