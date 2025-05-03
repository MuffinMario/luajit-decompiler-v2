#include <unistd.h>
#include <iostream>
#include <string>
#include <limits.h>
#include <filesystem>
#include <system_error>
#include <sys/stat.h>
#include <errno.h>

#include "linux_util.h"

namespace lnx {

    namespace fs = std::filesystem;
    
    std::string getExecutablePath() {
        constexpr std::string_view SELF_PATH_LN = "/proc/self/exe";
        
        std::error_code ec;
        auto p = fs::canonical(SELF_PATH_LN,ec);
        if(ec) 
        throw std::system_error(ec,"Couldn't get executable path. fs::canonical returned error code." );
        return p.parent_path().string();
    }
    
    std::string getPathFileName(std::string path) {
        auto p = fs::path(path);
        return p.filename();
    }
    std::string getPathDirectory(std::string path) {
        auto p = fs::path(path);
        struct stat s;
        if(int err = stat(path.c_str(),&s))
            throw std::system_error(errno,std::system_category());
        if(S_ISDIR(s.st_mode))
            return p.string();
        else 
            return p.parent_path().string();
    }
    std::string stripExtension(std::string path) {
        auto p = fs::path(path);
        return p.replace_extension("").string();
    }
}
/*
int main() {
    std::cout << lnx::getExecutablePath() << std::endl;
    std::cout << lnx::getPathFileName(std::filesystem::canonical("/proc/self/exe").string()) << std::endl;
}
*/