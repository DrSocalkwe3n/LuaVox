/*
 * TOSLib.cpp
 *
 *  Created on: 22 янв. 2023 г.
 *      Author: mr_s
 */

#include "TOSLib.hpp"
#include "boost/regex/v5/icu.hpp"
#include "boost/regex/v5/regex.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string>
#define DBOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>
#include <boost/regex.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <boost/uuid/detail/sha1.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/nowide/fstream.hpp>

namespace TOS {

namespace Str {

std::vector<std::string> split(const std::string &in, const std::string &delimeter, bool useRegex) {
    if (useRegex) {
        if (maxUnicode(in) != 1) {
            std::u32string u32in = toUStr(in), u32delimeter = toUStr(delimeter);

            std::vector<std::string> out;
            size_t prev = 0, next = 0;

            for (boost::u32regex_iterator<std::string::const_iterator> begin(in.begin(), in.end(), boost::make_u32regex(toUStr(delimeter))), end; begin != end; ++begin) {
                out.push_back(std::string(in.data() + prev, begin->position() - prev));
                prev = begin->position() + begin->length(0);
            }

            out.push_back(std::string(in.data() + prev));

            return out;
        } else {
            boost::regex pattern(delimeter.begin(), delimeter.end());
            std::vector<std::string> out;
			size_t prev = 0;

            for (boost::cregex_iterator begin(in.data(), in.data() + in.size(), pattern), end; begin != end; ++begin) {
                if (prev < begin->position())
                    out.push_back(std::string(in.data() + prev, begin->position() - prev));
                
                prev = begin->position() + begin->length(0);
            }

            if (prev < in.size())
                out.push_back(std::string(in.data() + prev, in.size() - prev));

            return out;
        }
    } else {
        size_t prev = 0, next, count = 0;

        while ((next = in.find(delimeter, prev)) != std::string::npos) {
            count++;
            prev = next + delimeter.size();
        }

        std::vector<std::string> out(count + 1);
        prev = next = count = 0;

        while ((next = in.find(delimeter, prev)) != std::string::npos) {
            out[count++] = {in.data() + prev, next - prev};
            prev = next + delimeter.size();
        }

        out[count] = {in.data() + prev, in.size() - prev};

        return out;
    }
}

std::string replace(const std::string &in, const std::string &pattern, const std::string &to, bool useRegex)
{
	std::vector<std::string> views = split(in, pattern, useRegex);
	std::string out;

	{
		size_t size = views.size() * to.size();
		for(auto &view : views)
			size += view.size();

		out.reserve(size);
	}

	for(size_t iter = 0; iter < views.size(); iter++)
	{
		out.append(views[iter]);
		if(iter < views.size()-1)
			out.append(to);
	}

	return out;
}

bool contains(const std::string &in, const std::string &pattern, bool useRegex)
{
	if(useRegex)
		return boost::cregex_iterator(in.data(), in.data()+in.size(), boost::regex(pattern.begin(), pattern.end())) != boost::cregex_iterator();
	else 
		return in.find(pattern) != std::string::npos;
}

size_t count(const std::string &in, const std::string &pattern)
{
	return split(in, pattern, false).size();
}

std::optional<std::vector<std::optional<std::string>>> match(const std::string &in, const std::string &pattern)
{
	boost::smatch xResults;
	bool matched;

	if(maxUnicode(in) != 1)
	{
		matched = boost::u32regex_match(in, xResults, boost::make_u32regex(toUStr(pattern)));
	} else {
		matched = boost::regex_match(in, xResults, boost::regex(pattern.begin(), pattern.end()));
	}

	if(!matched)
		return {};

	std::vector<std::optional<std::string>> data(xResults.size());

	for(size_t index = 0; index<xResults.size(); index++)
		if(xResults[index].matched)
			data[index].emplace(xResults[index].str());

	return data;
}

std::wstring toWStr(const std::string &view)
{
	std::wstring out;
	out.reserve(view.size());

	for(size_t ret = 0; ret < view.size(); ret++)
	{
		char c = view[ret];
		if((c & 0x80) == 0)
			out += c;
		else if((c & 0xC0) == 0xC0)
		{
			if(ret+1 >= view.size())
				out += L'?';
			else if((view[ret+1] & 0x80) == 0x80)
				out += (view[ret+1] & 0x3F) | ((view[ret] & 0x1F) << 6);
			else
			{
				out += L'?';
				ret--;
			}

			ret++;
		} else if((c & 0xE0) == 0xE0)
		{
			if(ret+2 >= view.size())
				out += L'?';
			else if((view[ret+1] & 0x80) == 0x80 && (view[ret+2] & 0x80) == 0x80)
				out += (view[ret+2] & 0x3F) + ((view[ret+1] & 0x3F) << 6) | ((view[ret] & 0xF) << 12);
			else
			{
				out += L'?';
				ret -= 2;
			}

			ret += 2;
		} else if((c & 0xF0) == 0xF0)
		{
			if(ret+3 >= view.size())
				out += L'?';
			else if((view[ret+1] & 0x80) == 0x80 && (view[ret+2] & 0x80) == 0x80 && (view[ret+3] & 0x80) == 0x80)
				out += (view[ret+3] & 0x3F) | ((view[ret+2] & 0x3F) << 6) | ((view[ret+1] & 0x3F) << 12) | ((view[ret] & 0xF) << 18);
			else
			{
				out += L'?';
				ret -= 3;
			}

			ret += 3;
		}
	}

	out.shrink_to_fit();

	return out;
}

std::string toStr(const std::wstring &view)
{
	int size = 0;

	for(wchar_t sym : view)
	{
		if(sym & (~0x7F))
		{
			if(sym & (~0x7FF))
				size += 3;
			else
				size += 2;
		} else
			size++;
	}

	std::string out;
	out.reserve(size);

	for(wchar_t sym : view)
	{
		if(sym & (~0x7F))
		{
			if(sym & (~0x7FF))
			{
				out += 0xE0 | ((sym >> 12) & 0x0F);
				out += 0x80 | ((sym >> 6) & 0x3F);
				out += 0x80 | (sym & 0x3F);
			} else {
				out += 0xC0 | ((sym >> 6) & 0x1F);
				out += 0x80 | (sym & 0x3F);
			}
		} else
			out += sym;
	}

	return out;
}

std::u32string toUStr(const std::string &view)
{
	std::u32string out;
	out.reserve(view.size());

	for(size_t ret = 0; ret < view.size(); ret++)
	{
		char c = view[ret];
		if((c & 0x80) == 0)
			out += c;
		else if((c & 0xC0) == 0xC0)
		{
			if(ret+1 >= view.size())
				out += L'?';
			else if((view[ret+1] & 0x80) == 0x80)
				out += (view[ret+1] & 0x3F) | ((view[ret] & 0x1F) << 6);
			else
			{
				out += L'?';
				ret--;
			}

			ret++;
		} else if((c & 0xE0) == 0xE0)
		{
			if(ret+2 >= view.size())
				out += L'?';
			else if((view[ret+1] & 0x80) == 0x80 && (view[ret+2] & 0x80) == 0x80)
				out += (view[ret+2] & 0x3F) | ((view[ret+1] & 0x3F) << 6) | ((view[ret] & 0xF) << 12);
			else
			{
				out += L'?';
				ret-=2;
			}

			ret += 2;
		} else if((c & 0xF0) == 0xF0)
		{
			if(ret+3 >= view.size())
				out += L'?';
			else if((view[ret+1] & 0x80) == 0x80 && (view[ret+2] & 0x80) == 0x80 && (view[ret+3] & 0x80) == 0x80)
				out += (view[ret+3] & 0x3F) | ((view[ret+2] & 0x3F) << 6) | ((view[ret+1] & 0x3F) << 12) | ((view[ret] & 0xF) << 18);
			else
			{
				out += L'?';
				ret -= 3;
			}

			ret += 3;
		}
	}

	out.shrink_to_fit();

	return out;
}

std::string toStr(const std::u32string &view)
{
	int size = 0;

	for(wchar_t sym : view)
	{
		if(sym & (~0x7F))
		{
			if(sym & (~0x7FF))
				size += 4;
			else
				size += 2;
		} else
			size++;
	}

	std::string out;
	out.reserve(size);

	for(char32_t sym : view)
	{
		if(sym & (~0x7F))
		{
			if(sym & (~0x7FF))
			{
				out += 0xF0;
				out += 0x80 | ((sym >> 12) & 0x3F);
				out += 0x80 | ((sym >> 6) & 0x3F);
				out += 0x80 | (sym & 0x3F);
			} else {
				out += 0xC0 | ((sym >> 6) & 0x1F);
				out += 0x80 | (sym & 0x3F);
			}
		} else
			out += sym;
	}

	return out;
}

int maxUnicode(const std::string &view)
{
	int max = 1;
	char sym;

	for(size_t iter = 0; iter < view.size(); iter++)
	{
		char c = view[iter];
		if((c & 0xC0) == 0xC0)
		{
			if(2 > max)
				max = 2;

			iter++;
		} else if((c & 0xE0) == 0xE0)
		{
			if(3 > max)
				max = 3;

			iter += 2;
		} else if((c & 0xF0) == 0xF0)
		{
			if(4 > max)
				max = 4;

			iter += 3;
		}
	}

	return max;
}

std::string toLowerCase(const std::string &view)
{
	std::u32string out = toUStr(view);

	for(char32_t &sym : out)
	{
		sym = std::tolower(sym);
		if(sym >= L'А' && sym <= L'Я')
			sym += 32;
	}

	return toStr(out);
}

std::string toUpperCase(const std::string &view)
{
	std::u32string out = toUStr(view);

	for(char32_t &sym : out)
	{
		sym = std::toupper(sym);
		if(sym >= L'а' && sym <= L'я')
			sym -= 32;
	}

	return toStr(out);
}

float compareRelative(const std::string &left, const std::string &right)
{
	size_t equals = 0, minSize = std::min(left.size(), right.size());
	for(size_t iter = 0; iter < minSize; iter++)
		if(left[iter] == right[iter])
			equals++;

	if(!minSize)
		return left.size() == right.size();

	return float(equals) / float(std::max(left.size(), right.size()));
}

}


namespace Enc {

std::string toHex(const uint8_t *begin, size_t size)
{
	std::string out(size, ' ');
	char *data = out.data();

	for(const uint8_t *end = begin + size; begin != end; begin++)
	{
		*(data++) = "0123456789abcdf"[*begin & 0xf];
		*(data++) = "0123456789abcdf"[(*begin >> 4) & 0xf];
	}

	return out;
}

void fromHex(const std::string &in, uint8_t *data)
{
	const char *begin = in.data(), *end = begin + in.size();
	for(; begin != end;)
	{
		*data = 0;
		if(*begin >= 'a')
			*data |= *begin - 'a' + 10;
		else
			*data |= *begin - '0';

		begin++;

		if(*begin >= 'a')
			*data |= (*begin - 'a' + 10) << 4;
		else
			*data |= (*begin - '0') << 4;

		begin++;
		data++;
	}
}

std::string toBase64(const uint8_t *data, size_t size)
{
	using namespace boost::archive::iterators;
	using It = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
	auto tmp = std::string(It((const char*) data), It((const char*) data+size));
	return std::string(tmp.append((3 - size % 3) % 3, '=')); //.replace("/", "_").replace("+", "-");
}

ByteBuffer fromBase64(const std::string &view)
{
	//std::string val = view.replace("_", "/").replace("-", "+").toStr();
	using namespace boost::archive::iterators;
	using It = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
	std::string out(It(view.begin()), It(view.end()));
	return {out.size(), (const uint8_t*) out.data()};
}

void base64UrlConvert(std::string &in)
{
	for(char &sym : in)
	{
		if(sym == '_')
			sym = '/';
		else if(sym == '-')
			sym = '+';
		else if(sym == '/')
			sym = '_';
		else if(sym == '+')
			sym = '-';
	}
}

std::string md5(const uint8_t *data, size_t size)
{
	boost::uuids::detail::md5 md5;
	md5.process_bytes((const char*) data, size);
	unsigned hash[4] = {0};
	md5.get_digest((boost::uuids::detail::md5::digest_type&)hash);

	char buf[33] = {0};

	for (int i = 0; i < 4; i++)
		std::sprintf(buf + (i << 3), "%08x", hash[i]);

	return Str::toLowerCase(buf);
}

std::string sha1(const uint8_t *data, size_t size)
{
	boost::uuids::detail::sha1 sha1;
	sha1.process_bytes((const char*) data, size);
	unsigned hash[5] = {0};
	sha1.get_digest((boost::uuids::detail::sha1::digest_type&)hash);

	char buf[41] = {0};

	for (int i = 0; i < 5; i++)
		std::sprintf(buf + (i << 3), "%08x", hash[i]);

	return Str::toLowerCase(buf);
}

}


namespace Time {

std::string getDateAsString()
{
	auto time = boost::posix_time::second_clock::local_time().date();
	std::stringstream out;
	out << std::to_string(time.year());
	std::string temp = std::to_string(time.month());

	out << '.';
	if(temp.size() == 1)
		out << '0';
	out << temp;

	temp = std::to_string(time.day());
	out << '.';
	if(temp.size() == 1)
		out << '0';
	out << temp;

	return out.str();
}

std::string getTimeAsString()
{
	auto day = boost::posix_time::second_clock::local_time().time_of_day();
	std::stringstream out;
	std::string temp = std::to_string(day.hours());

	if(temp.size() == 1)
		out << '0';
	out << temp;

	out << ':';
	temp = std::to_string(day.minutes());
	if(temp.size() == 1)
		out << '0';
	out << temp;

	out << ':';
	temp = std::to_string(day.seconds());
	if(temp.size() == 1)
		out << '0';
	out << temp;

	return out.str();
}

}

std::string makeStacktrace(int stack_up)
{
	std::vector<std::string> spl = Str::split(boost::stacktrace::to_string(boost::stacktrace::stacktrace()), "\n");
	std::string out;
	for(size_t i = 1+stack_up; i<spl.size(); i++)
	{
		std::stringstream line;
		
		if(out.size())
			line.str("\n");

		if(spl[i].size() > 128)
			line << spl[i].substr(0, 128);
		else
			line << spl[i];

		if(Str::contains(line.str(), "boost::asio::asio_handler_invoke"))
			break;
		else
		 	out += line.str();
	}

	return out;
}

void DynamicLibrary::symbolOut()
{
	throw std::runtime_error("Символ не привязан");
}

bool DynamicLibrary::IsOverWine = DynamicLibrary::checkWine();
bool DynamicLibrary::checkWine()
{
	try {
		DynamicLibrary ntdll("ntdll.dll");
		void *sym = ntdll.getSymbol("wine_get_version");
		return sym;
	} catch(...) {}

	return false;
}

DynamicLibrary::DynamicLibrary(const std::string &name)
	: Name(name)
{
#ifdef _WIN32
	FD = (uint64_t) LoadLibrary(name.toWStr().c_str());
	if (!FD)
	{
		uint64_t err = (uint64_t) GetLastError();
		MAKE_ERROR << "Не удалось загрузить разделяемую библиотеку " << name << ", код ошибки " << err;
	}

	wchar_t name2[256];
	GetModuleFileName((HMODULE) FD, name2, 256);
	Name = std::wstring(name2);
#else
	const char* error_msg;
	FD = uint64_t(dlopen(name.data(), RTLD_NOW | RTLD_GLOBAL));
	if(!FD)
	{
		error_msg = dlerror();

		std::stringstream info;
		info << "Библиотека " << name << " не найдена";
		if(error_msg)
			info << ": " << error_msg;
		
		throw std::runtime_error(info.str());
	}
	
	try {
		std::ifstream r("/proc/self/maps");
		std::string str;
		char path[256] = "";

		while (std::getline(r, str)) {
			if (sscanf(str.c_str(), "%*llx-%*llx %*s %*s %*s %*s %s", path) == 1) {
				if(Str::contains(path, name))
				{
					Path = path;
					break;
				}
			}
		}
		
	} catch(...) {
		Path = name;
	}
	
	if(Path.empty())
		Path = name;


// 	if(!FD && (!error_msg || String(error_msg).contains("No such file or directory")))
// 	{

// 		auto lock = LD_LIBRARY_PATH.lockRead();
// 		for(int id = 0; id<3 && !FD; id++)
// 		for(const File &str : *lock)
// 		{
// 			if(str.isExist() && str.isDir())
// 			for(File f : str.getSubFiles())
// {
// 	if(f.isDir() || !f.isReal())
// 		continue;

// 	if((id == 0 && f.getFilename().starts_with("lib" << name << ".so"))
// 			|| (id == 1 && TOS::REGEX::match("^lib" << name << ".*" << "\\.so", f.getFilename()))
// 			|| (id == 2 && f.getFilename().starts_with(name) && f.getFilename().contains(".so"))
// 			)
// 	{
// 		FD = uint64_t(dlopen(f.getFullPathFile().getPath().toCStr(), RTLD_LAZY | RTLD_GLOBAL));

// 		if(FD)
// 		{
// 			loaded = f;
// 			Path = f.getFullPathFile().getPath();
// 			break;
// 		} else {
// 			error_msg = dlerror();
// 			MAKE_ERROR << "Не удалось загрузить разделяемую библиотеку " << name << ", ошибка: " << error_msg;
// 		}
// 	}
// }
// 			if(FD)
// 				break;
// 		}
// 	}

// 	if(!FD)
// 		MAKE_ERROR << "Не удалось загрузить разделяемую библиотеку " << name << ", ошибка: " << error_msg;

// 	Dl_info info;
// 	if(!Path.size())
// 	{
// 		if(dladdr((void*) FD, &info) && info.dli_fname && (!*info.dli_fname || *info.dli_fname == '/'))
// 			Path = info.dli_fname;
// 		else
// 			Path = File(name).getPath();
// 	}

// 	LOGGER.debug() << "Запрос " << name << "; найдено " << Path;

#endif
}

DynamicLibrary::~DynamicLibrary()
{
	if(!FD)
		return;

#ifdef _WIN32
	FreeLibrary((HMODULE) FD);
#else
	dlclose((void*) FD);
#endif
}

void* DynamicLibrary::getSymbol(const std::string &name) const
{
#ifdef _WIN32
	void *sym = (void*) GetProcAddress((HMODULE) FD, name.toCStr());
	if(!sym)
	{
		uint64_t error = (uint64_t) GetLastError();
		MAKE_ERROR("Ошибка загрузки символа " << name << " в библиотеке " << Name << ", код ошибки " << error);
	}
	return sym;
#else
	void *sym = dlsym((void*) FD, name.data());
	if(!sym)
	{
		const char* str = dlerror();
		MAKE_ERROR("Ошибка загрузки символа " << name << " в библиотеке " << Name << ", ошибка: " << str);
	}
	return sym;
#endif
}

void* DynamicLibrary::takeSymbol(const std::string &name) const
{
#ifdef _WIN32
	void *sym = (void*) GetProcAddress((HMODULE) FD, name.toCStr());
	if(!sym)
		sym = (void*) &symbolOut;
	return sym;
#else
	void *sym = dlsym((void*) FD, name.data());
	if(!sym)
		sym = (void*) &symbolOut;
	return sym;
#endif
}

void* DynamicLibrary::getDynamicSymbol(const std::string &name)
{
#ifdef _WIN32
	void *sym = (void*) GetProcAddress(GetModuleHandle(nullptr), name.toCStr());
	if(!sym)
	{
		uint64_t error = (uint64_t) GetLastError();
		MAKE_ERROR("Ошибка загрузки символа " << name << " в общем пространстве символов, код ошибки " << error);
	}
	return sym;
#else
	void *sym = dlsym((void*) RTLD_DEFAULT, name.data());
	if(!sym)
	{
		const char* str = dlerror();
		MAKE_ERROR("Ошибка загрузки символа " << name << " в общем пространстве символов, ошибка: " << str);
	}
	return sym;
#endif
}

void* DynamicLibrary::takeDynamicSymbol(const std::string &name)
{
#ifdef _WIN32
	void *sym = (void*) GetProcAddress(GetModuleHandle(nullptr), name.toCStr());
	if(!sym)
		sym = (void*) &symbolOut;
	return sym;
#else
	void *sym = dlsym((void*) RTLD_DEFAULT, name.data());
	if(!sym)
		sym = (void*) &symbolOut;
	return sym;
#endif
}

void* DynamicLibrary::hasDynamicSymbol(const std::string &name)
{
#ifdef _WIN32
	return (void*) GetProcAddress(GetModuleHandle(nullptr), name.toCStr());
#else
	return dlsym((void*) RTLD_DEFAULT, name.data());
#endif
}

void DynamicLibrary::needGlobalLibraryTemplate(const std::string &name)
{
}


std::vector<std::filesystem::path> DynamicLibrary::getLdPaths()
{
	std::vector<std::filesystem::path> paths;
#ifdef _WIN32
#else
	const char *str = getenv("PATH");

	if(str)
		for(auto &iter : Str::split(str, ":"))
			paths.push_back(iter);

	str = getenv("LD_LIBRARY_PATH");

	if(str)
		for(auto &iter : Str::split(str, ":"))
			paths.push_back(iter);
#endif

	return paths;
}

struct LogFile {
	boost::u32regex RegexForPath;
	boost::nowide::ofstream Write;
	EnumLogType Levels;
	uint32_t LastLoggedDay = 0;

	LogFile(const std::string &regex_for_path, EnumLogType levels, const std::filesystem::path &path)
		: RegexForPath(boost::make_u32regex(Str::toUStr(regex_for_path))),
		  Levels(levels),
		  Write(path, std::ios::out | std::ios::binary | std::ios::app)
	{}

	LogFile(const LogFile&) = delete;
	LogFile(LogFile&&) = default;
	LogFile& operator=(const LogFile&) = delete;
	LogFile& operator=(LogFile&&) = default;
};

struct LogCmd {
	boost::u32regex RegexForPath;
	EnumLogType Levels;

	LogCmd(const std::string &view, EnumLogType levels)
		: RegexForPath(boost::make_u32regex(Str::toUStr(view))),
		  Levels(levels)
	{}

	LogCmd(const LogCmd&) = delete;
	LogCmd(LogCmd&&) = default;
	LogCmd& operator=(const LogCmd&) = delete;
	LogCmd& operator=(LogCmd&&) = default;
};

static std::mutex LogFilesMtx;
static std::vector<LogFile> LogFiles;
static std::vector<LogCmd> LogRegexs;
static uint32_t LogCmdLastDay = Time::getDay();

LogSession::LogSession(const std::string &path, EnumLogType type)
	: Path(path)
{
	{
		std::lock_guard lock(LogFilesMtx);

		for(auto &iter : LogRegexs)
			if(int(type) & int(iter.Levels) && boost::u32regex_match(path.data(), iter.RegexForPath))
			{
				NeedC = true;
				break;
			}

		for(auto &iter : LogFiles)
			if(int(type) & int(iter.Levels) && boost::u32regex_match(path.data(), iter.RegexForPath))
			{
				NeedF = true;
				break;
			}

		if(!NeedF && !NeedC)
		{
			return;
		}
	}

	*this << char(27) << '[';
	switch(type) {
	case EnumLogType::Debug: 	*this << "37"; break;
	case EnumLogType::Info: 	*this << "36"; break;
	case EnumLogType::Warn: 	*this << "33"; break;
	case EnumLogType::Error: 	*this << "31"; break;
	case EnumLogType::All: 		*this << "35"; break;
	default: break;
	}

	*this << "m[" << Time::getTimeAsString() << ' ' << path << '-';
	switch(type) {
	case EnumLogType::Debug: 	*this << 'D'; break;
	case EnumLogType::Info: 	*this << 'I'; break;
	case EnumLogType::Warn: 	*this << 'W'; break;
	case EnumLogType::Error: 	*this << 'E'; break;
	case EnumLogType::All: 		*this << '%'; break;
	default: break;
	}

	*this << "]: " << char(27) << "[0m";
}

LogSession::~LogSession()
{
	if(!NeedF && !NeedC)
		return;

	std::lock_guard lock(LogFilesMtx);
	
	if(NeedF)
	{
		for(auto &iter : LogFiles)
			if(boost::u32regex_match(Path.data(), iter.RegexForPath))
			{
				uint32_t day = Time::getDay();
				if(iter.LastLoggedDay != day)
				{
					iter.LastLoggedDay = day;
					std::stringstream date;
					date << " -*[ " << Time::getDateAsString() << " ]*-\n";
					iter.Write << date.str() << std::endl;
				}

				iter.Write << Str::replace(str(), "\n", "\n\t") << std::endl;
				iter.Write.flush();
			}
	}

	if(NeedC)
	{
		uint32_t day = Time::getDay();
		if(LogCmdLastDay != day)
		{
			LogCmdLastDay = day;
			std::stringstream date;
			date << " -*[ " << Time::getDateAsString() << " ]*-\n";
			std::cout << date.str() << std::endl;
		}

		std::cout << Str::replace(str(), "\n", "\n\t") << std::endl;
		std::cout.flush();
	}
}

void Logger::addLogFile(const std::string &regex_for_path, EnumLogType levels, const std::filesystem::path &file, bool rewrite)
{
	if(rewrite && std::filesystem::exists(file))
		std::filesystem::remove(file);
	
	std::lock_guard lock(LogFilesMtx);
	LogFiles.emplace_back(regex_for_path, levels, file);
}

void Logger::addLogOutput(const std::string &regex_for_path, EnumLogType levels)
{
	std::lock_guard lock(LogFilesMtx);
	LogRegexs.emplace_back(regex_for_path, levels);
}

static bool exec = [](){
	std::cout.sync_with_stdio(false);
	return false;
}();

namespace TIME {

size_t getSeconds()
{
	static size_t val1 = 0, val2 = 0;
	size_t time = boost::posix_time::second_clock::local_time().time_of_day().total_seconds();
	if(time < val1)
		val2++;
	val1 = time;
	return val2*86400+time;
}

}

} /* namespace TOS */
