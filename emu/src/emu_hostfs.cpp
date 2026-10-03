/* Host directory listing, kept apart from FatFS (both define DIR). */
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace emu {
void ListHostDir(const std::string& path, std::vector<std::pair<std::string, bool>>& out)
{
    out.clear();
    DIR* d = opendir(path.c_str());
    if (!d) return;
    while (struct dirent* e = readdir(d))
    {
        if (e->d_name[0] == '.') continue;
        struct stat st;
        const std::string full = path + "/" + e->d_name;
        if (stat(full.c_str(), &st) != 0) continue;
        out.emplace_back(e->d_name, S_ISDIR(st.st_mode));
    }
    closedir(d);
}
} // namespace emu
