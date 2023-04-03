#pragma once




struct entire_file
{
	uint8* content;
	uint64 size;
	uint64 readOffset;

	template <typename T>
	T* consume(uint32 count = 1)
	{
		uint32 readSize = sizeof(T) * count;
		if (readSize > size - readOffset)
		{
			return 0;
		}

		T* result = (T*)(content + readOffset);
		readOffset += readSize;
		return result;
	}
};

static entire_file loadFile(const fs::path& path)
{
	FILE* f = fopen(path.string().c_str(), "rb");
	if (!f)
	{
		return {};
	}
	fseek(f, 0, SEEK_END);
	uint32 fileSize = ftell(f);
	if (fileSize == 0)
	{
		fclose(f);
		return {};
	}
	fseek(f, 0, SEEK_SET);

	uint8* buffer = (uint8*)malloc(fileSize);
	fread(buffer, fileSize, 1, f);
	fclose(f);

	return { buffer, fileSize };
}

static void freeFile(entire_file file)
{
	free(file.content);
}


struct sized_string
{
	const char* str;
	uint32 length;

	sized_string() : str(0), length(0) {}
	sized_string(const char* str, uint32 length) : str(str), length(length) {}
	template<uint32 len> sized_string(const char(&str)[len]) : str(str), length(len - 1) {}
};

static bool operator==(sized_string a, sized_string b)
{
	return a.length == b.length && strncmp(a.str, b.str, a.length) == 0;
}

static std::string nameToString(sized_string str)
{
	std::string name;
	name.reserve(str.length);
	for (uint32 j = 0; j < str.length; ++j)
	{
		if (str.str[j] == 0x0 || str.str[j] == 0x1)
		{
			break;
		}
		name.push_back(str.str[j]);
	}

	size_t posOfFirstOr = name.find_last_of('|');
	if (posOfFirstOr != std::string::npos)
	{
		name = name.substr(posOfFirstOr + 1);
	}

	return name;
}

static std::string relativeFilepath(sized_string str, const fs::path& scenePath)
{
	fs::path p = std::string(str.str, str.length);
	fs::path abs = (p.is_absolute()) ? p : scenePath.parent_path() / p;
	fs::path rel = fs::relative(abs, fs::current_path());
	return rel.string();
}

