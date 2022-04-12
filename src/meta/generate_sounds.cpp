#include "pch.h"

#include "core/string.h"


#include <fstream>
#include <regex>
#include <set>

int main()
{
    std::regex re("SOUND_ID\\(\"(.*)\"\\)");

    std::set<std::string> sounds;

    for (const fs::directory_entry& dirEntry : fs::recursive_directory_iterator("src"))
    {
        if (fs::is_regular_file(dirEntry))
        {
            std::ifstream fin(dirEntry);
            std::string str((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());

            std::smatch match;

            while (std::regex_search(str, match, re))
            {
                sounds.insert(match[1]);
                str = match.suffix();
            }
        }
    }

    fs::path target = "src/generated/sound_ids.h";
    fs::create_directories(target.parent_path());

    std::ofstream fout(target);

    std::string actualCount = (sounds.size() > 0) ? std::to_string(sounds.size()) : "0";
    std::string arraySize = (sounds.size() > 0) ? actualCount : "1";

    fout << "#pragma once\n\n";
    fout << "static const sound_id soundIDs[" << arraySize << "] = \n{\n";
    
    std::cout << "Generating " << actualCount << " sound" << ((sounds.size() == 1) ? "" : "s") << ":\n";

    for (const std::string& s : sounds)
    {
        uint64 hash = hashString64(s.c_str());

        std::cout << "\t" << s << " " << hash << '\n';
        fout << "\t{ \"" + s + "\", " + std::to_string(hash) + "llu },\n";
    }

    fout << "};\n\n";

    fout << "static const uint32 numSoundIDs = " << actualCount << ";\n\n";

}
