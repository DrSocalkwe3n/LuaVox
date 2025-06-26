#pragma once

#include <boost/timer/timer.hpp>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <sstream>
#include <thread>

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

namespace TOS {


template<typename T>
class MutexObject {
public:
    template<typename... Args>
    explicit MutexObject(Args&&... args)
        : value(std::forward<Args>(args)...) {}

    class SharedLock {
    public:
        SharedLock(MutexObject* obj, std::shared_lock<std::shared_mutex> lock)
            : obj(obj), lock(std::move(lock)) {}
        
        const T& get() const { return obj->value; }
		const T& operator*() const { return obj->value; }
		const T* operator->() const { return &obj->value; }

        void unlock() { lock.unlock(); }

		operator bool() const {
			return lock.owns_lock();
		}

    private:
        MutexObject* obj;
        std::shared_lock<std::shared_mutex> lock;
    };

    class ExclusiveLock {
    public:
        ExclusiveLock(MutexObject* obj, std::unique_lock<std::shared_mutex> lock)
            : obj(obj), lock(std::move(lock)) {}
        
        T& get() const { return obj->value; }
		T& operator*() const { return obj->value; }
		T* operator->() const { return &obj->value; }

        void unlock() { lock.unlock(); }

		operator bool() const {
			return lock.owns_lock();
		}

    private:
        MutexObject* obj;
        std::unique_lock<std::shared_mutex> lock;
    };

    SharedLock shared_lock() {
        return SharedLock(this, std::shared_lock(mutex));
    }

    SharedLock shared_lock(const std::try_to_lock_t& tag) {
        return SharedLock(this, std::shared_lock(mutex, tag));
    }

    SharedLock shared_lock(const std::adopt_lock_t& tag) {
        return SharedLock(this, std::shared_lock(mutex, tag));
    }

    SharedLock shared_lock(const std::defer_lock_t& tag) {
        return SharedLock(this, std::shared_lock(mutex, tag));
    }

    ExclusiveLock exclusive_lock() {
        return ExclusiveLock(this, std::unique_lock(mutex));
    }

    ExclusiveLock exclusive_lock(const std::try_to_lock_t& tag) {
        return ExclusiveLock(this, std::unique_lock(mutex, tag));
    }

    ExclusiveLock exclusive_lock(const std::adopt_lock_t& tag) {
        return ExclusiveLock(this, std::unique_lock(mutex, tag));
    }

    ExclusiveLock exclusive_lock(const std::defer_lock_t& tag) {
        return ExclusiveLock(this, std::unique_lock(mutex, tag));
    }

private:
    T value;
    mutable std::shared_mutex mutex;
};

template<typename T>
class SpinlockObject {
public:
    template<typename... Args>
    explicit SpinlockObject(Args&&... args)
        : value(std::forward<Args>(args)...) {}

    class Lock {
    public:
        Lock(SpinlockObject* obj, std::atomic_flag& lock)
            : obj(obj), lock(lock) {
            while (lock.test_and_set(std::memory_order_acquire));
        }

        ~Lock() {
			if(obj)
            	lock.clear(std::memory_order_release);
        }

        T& get() const { assert(obj); return obj->value; }
		T* operator->() const { assert(obj); return &obj->value; }
		T& operator*() const { assert(obj); return obj->value; }

		void unlock() { obj = nullptr; lock.clear(std::memory_order_release);}

    private:
        SpinlockObject* obj;
        std::atomic_flag& lock;
    };

    Lock lock() {
        return Lock(this, mutex);
    }

	const T& get_read() { return value; }

private:
    T value;
    std::atomic_flag mutex = ATOMIC_FLAG_INIT;
};

#if __BYTE_ORDER == __LITTLE_ENDIAN
	template <typename T>
	static inline T swapEndian(const T &u) { return u; }
#else
	template <typename T>
	static inline T swapEndian(const T &u)
	{
		union {
			T u;
			byte u8[sizeof(T)];
		} source, dest;

		source.u = u;

		for (size_t k = 0; k < sizeof(T); k++)
			dest.u8[k] = source.u8[sizeof(T) - k - 1];

		return dest.u;
	}

	template<> inline uint8_t swapEndian(const uint8_t &value) { return value; }
	template<> inline int8_t swapEndian(const int8_t &value) { return swapEndian(*(uint8_t*) &value); }
	template<> inline uint16_t swapEndian(const uint16_t &value) { return __bswap_16(value); }
	template<> inline int16_t swapEndian(const int16_t &value) { return swapEndian(*(uint16_t*) &value); }
	template<> inline uint32_t swapEndian(const uint32_t &value) { return __bswap_32(value); }
	template<> inline int32_t swapEndian(const int32_t &value) { return swapEndian(*(uint32_t*) &value); }
	template<> inline uint64_t swapEndian(const uint64_t &value) { return __bswap_64(value); }
	template<> inline int64_t swapEndian(const int64_t &value) { return swapEndian(*(uint64_t*) &value); }
#endif


class ByteBuffer : public std::vector<uint8_t> {
	protected:
		typedef std::vector<uint8_t> base;

	public:
		class Reader;
		class Writer;

	public:
		ByteBuffer() = default;
		template<typename ...Args, typename = typename std::enable_if_t<std::is_constructible_v<base, Args...>>>
		ByteBuffer(Args&& ... args)
			: base(std::forward<Args>(args)...)
		{}
		
		ByteBuffer(std::istream &&stream)
		{
			size_t gs = stream.tellg();
			stream.seekg(0, std::ios::end);
			resize(std::min<size_t>(size_t(stream.tellg()) - gs, 1024*1024));

			stream.seekg(gs, std::ios::beg);

			char buff[4096];

			size_t offset = 0;

			while((gs = stream.readsome(buff, 4096)))
			{
				if(offset+gs > size())
					resize(offset+gs);

				std::copy(buff, buff+gs, data() + offset);
				offset += gs;
			}

			resize(offset);
			shrink_to_fit();
		}


		ByteBuffer(size_t size, const uint8_t *ptr = nullptr)
			: base(size)
		{
			if(ptr)
				std::copy(ptr, ptr+size, data());
		}

		~ByteBuffer() = default;

		ByteBuffer(const ByteBuffer&) = default;
		ByteBuffer(ByteBuffer&&) = default;
		ByteBuffer& operator=(const ByteBuffer&) = default;
		ByteBuffer& operator=(ByteBuffer&&) = default;

		Reader reader() const;
		static Writer writer();
	};

	class ByteBuffer::Reader {
		const ByteBuffer *Obj;
		size_t Index = 0;

		template<typename T> inline T readOffset() { 
			if(Index + sizeof(T) > Obj->size())
				throw std::runtime_error("Вышли за пределы буфера");

			const uint8_t *ptr = Obj->data()+Index; 
			Index += sizeof(T); 
			return swapEndian(*(const T*) ptr); 
		}

	public:
		Reader(const ByteBuffer &buff)
			: Obj(&buff)
		{}

		Reader(const Reader&) = delete;
		Reader(Reader&&) = default;
		Reader& operator=(const Reader&) = delete;
		Reader& operator=(Reader&&) = default;

		inline Reader& operator>>(int8_t &value)   	{ value = readOffset<int8_t>(); 	return *this; }
		inline Reader& operator>>(uint8_t &value)  	{ value = readOffset<uint8_t>(); 	return *this; }
		inline Reader& operator>>(int16_t &value) 	{ value = readOffset<int16_t>(); 	return *this; }
		inline Reader& operator>>(uint16_t &value)	{ value = readOffset<uint16_t>();	return *this; }
		inline Reader& operator>>(int32_t &value) 	{ value = readOffset<int32_t>();	return *this; }
		inline Reader& operator>>(uint32_t &value) 	{ value = readOffset<uint32_t>();	return *this; }
		inline Reader& operator>>(int64_t &value) 	{ value = readOffset<int64_t>();	return *this; }
		inline Reader& operator>>(uint64_t &value) 	{ value = readOffset<uint64_t>();	return *this; }
		inline Reader& operator>>(bool &value) 		{ value = readOffset<uint8_t>(); 	return *this; }
		inline Reader& operator>>(float &value) 	{ return operator>>(*(uint32_t*) &value); }
		inline Reader& operator>>(double &value) 	{ return operator>>(*(uint64_t*) &value); }

		inline int8_t 	readInt8() 		{ int8_t value; this->operator>>(value); return value; }
		inline uint8_t 	readUInt8() 	{ uint8_t value; this->operator>>(value); return value; }
		inline int16_t 	readInt16() 	{ int16_t value; this->operator>>(value); return value; }
		inline uint16_t readUInt16() 	{ uint16_t value; this->operator>>(value); return value; }
		inline int32_t 	readInt32() 	{ int32_t value; this->operator>>(value); return value; }
		inline uint32_t readUInt32() 	{ uint32_t value; this->operator>>(value); return value; }
		inline int64_t 	readInt64() 	{ int64_t value; this->operator>>(value); return value; }
		inline uint64_t readUInt64() 	{ uint64_t value; this->operator>>(value); return value; }
		inline bool 	readBool() 		{ bool value; this->operator>>(value); return value; }
		inline float 	readFloat() 	{ float value; this->operator>>(value); return value; }
		inline double 	readDouble() 	{ double value; this->operator>>(value); return value; }

		inline void readSize(size_t &size)
		{
			uint8_t bytes[6];
			int readed = 0;
			do {
				*this >> bytes[readed++];
			} while(((bytes[readed-1] >> 7) & 1) && readed < 6);

			size = 0;
			for(int iter = 0; iter < readed; iter++)
				if(iter != 4)
					size |= size_t(bytes[iter] & 0x7f) << (iter*7);
				else
					size |= size_t(bytes[iter] & 0xf) << (iter*7);
		}

		inline void readBuffer(ByteBuffer &buff)
		{
			size_t size;
			readSize(size);

			if(Index + size > Obj->size())
				throw std::runtime_error("Вышли за пределы буфера");

			const uint8_t *ptr = Obj->data() + Index;
			Index += size;
			buff.resize(size);
			std::copy(ptr, ptr+size, buff.data());
		}

		inline void readString(std::string &str)
		{
			size_t size;
			readSize(size);

			if(Index + size > Obj->size())
				throw std::runtime_error("Вышли за пределы буфера");

			const uint8_t *ptr = Obj->data() + Index;
			Index += size;
			str.resize(size);
			std::copy(ptr, ptr+size, str.data());
		}

		inline size_t readSize() { size_t size; readSize(size); return size; }
		inline ByteBuffer readBuffer() { ByteBuffer buff; readBuffer(buff); return buff; }
		inline std::string readString() { std::string str; readString(str); return str; }

		Reader& operator>>(ByteBuffer &buff) { readBuffer(buff); return *this; }
		Reader& operator>>(std::string &str) { readString(str); return *this; }

		void setPos(size_t pos) { Index = pos; }
		void offset(ssize_t offset) { setPos(Index+size_t(offset)); }

		bool checkBorder() const { return Index >= Obj->size(); }

		ByteBuffer cutToBuffer()
		{
			ByteBuffer out(Obj->size()-Index);

			std::copy(Obj->data() + Index, Obj->data() + Obj->size(), out.data());
			Index += out.size();
			
			return out;
		}
	};

	class ByteBuffer::Writer {
		ByteBuffer Obj;
		size_t Index = 0;
		uint16_t BlockSize = 256;


		inline uint8_t* checkBorder(size_t count)
		{
			if(Index+count >= Obj.capacity())
				Obj.reserve(size_t(BlockSize)*(std::ceil(float(Index+count)/float(BlockSize))+1));
			Obj.resize(Index+count);

			uint8_t *ptr = Obj.data()+Index;
			Index += count;
			return ptr;
		}

	public:
		Writer() = default;

		Writer(const Writer&) = default;
		Writer(Writer&&) = default;
		Writer& operator=(const Writer&) = default;
		Writer& operator=(Writer&&) = default;

		inline Writer& operator<<(const int8_t &value) 		{ *(int8_t*) checkBorder(sizeof(value)) = value; return *this; }
		inline Writer& operator<<(const uint8_t &value) 	{ *(uint8_t*) checkBorder(sizeof(value)) = value; return *this; }
		inline Writer& operator<<(const int16_t &value)  	{ *(int16_t*) checkBorder(sizeof(value)) = swapEndian(value); return *this; }
		inline Writer& operator<<(const uint16_t &value) 	{ *(uint16_t*) checkBorder(sizeof(value)) = swapEndian(value); return *this; }
		inline Writer& operator<<(const int32_t &value)  	{ *(int32_t*) checkBorder(sizeof(value)) = swapEndian(value); return *this; }
		inline Writer& operator<<(const uint32_t &value) 	{ *(uint32_t*) checkBorder(sizeof(value)) = swapEndian(value); return *this; }
		inline Writer& operator<<(const int64_t &value)  	{ *(int64_t*) checkBorder(sizeof(value)) = swapEndian(value); return *this; }
		inline Writer& operator<<(const uint64_t &value) 	{ *(uint64_t*) checkBorder(sizeof(value)) = swapEndian(value); return *this; }
		inline Writer& operator<<(const bool &value)  		{ *(uint8_t*) checkBorder(sizeof(value)) = uint8_t(value ? 1 : 0); return *this; }
		inline Writer& operator<<(const float &value) 		{ *(uint32_t*) checkBorder(sizeof(value)) = swapEndian(*(uint32_t*) &value); return *this; }
		inline Writer& operator<<(const double &value) 		{ *(uint64_t*) checkBorder(sizeof(value)) = swapEndian(*(uint64_t*) &value); return *this; }

		inline void writeInt8(const int8_t &value) 			{ this->operator<<(value); }
		inline void writeUInt8(const uint8_t &value) 		{ this->operator<<(value); }
		inline void writeInt16(const int16_t &value) 		{ this->operator<<(value); }
		inline void writeUInt16(const uint16_t &value) 		{ this->operator<<(value); }
		inline void writeInt32(const int32_t &value) 		{ this->operator<<(value); }
		inline void writeUInt32(const uint32_t &value) 		{ this->operator<<(value); }
		inline void writeInt64(const int64_t &value) 		{ this->operator<<(value); }
		inline void writeUInt64(const uint64_t &value) 		{ this->operator<<(value); }
		inline void writeBool(const bool &value) 			{ this->operator<<(value); }
		inline void writeFloat(const float &value) 			{ this->operator<<(value); }
		inline void writeDouble(const double &value) 		{ this->operator<<(value); }

		inline void writeSize(size_t size)
		{
			size_t temp = size;
			int count = 1;
			for(; count < 6; count++)
			{
				temp >>= 7;
				if(!temp)
					break;
			}

			temp = size;
			for(int iter = 0; iter < count; iter++)
			{
				if(iter != count-1)
					writeUInt8((temp & 0x3f) | (0b1 << 7));
				else
					writeUInt8((temp & 0x3f));

				temp >>= 7;
			}
		}

		inline void writeBuffer(const ByteBuffer &buff)
		{
			writeSize(buff.size());
			uint8_t *ptr = checkBorder(buff.size());
			std::copy(buff.data(), buff.data()+buff.size(), ptr);
		}

		inline void writeString(const std::string &str)
		{
			writeSize(str.size());
			uint8_t *ptr = checkBorder(str.size());
			std::copy(str.data(), str.data()+str.size(), ptr);
		}

		inline void putBuffer(const ByteBuffer &buff)
		{
			uint8_t *ptr = checkBorder(buff.size());
			std::copy(buff.data(), buff.data()+buff.size(), ptr);
		}

		inline Writer& operator<<(const ByteBuffer &buff) { writeBuffer(buff); return *this; }
		inline Writer& operator<<(const std::string &str) { writeString(str); return *this; }
		inline Writer& operator<<(const char *str) { writeString(std::string(str)); return *this; }

		ByteBuffer complite() { Obj.shrink_to_fit(); return std::move(Obj); Index = 0; }
	};

	inline ByteBuffer::Reader ByteBuffer::reader() const { return *this; }
	inline ByteBuffer::Writer ByteBuffer::writer() { return {}; }


namespace Str {

	std::vector<std::string> split(const std::string &in, const std::string &delimeter, bool useRegex = false);
	
	std::string replace(const std::string &in, const std::string &pattern, const std::string &to, bool useRegex = false);
	bool contains(const std::string &in, const std::string &pattern, bool useRegex = false);
	size_t count(const std::string &in, const std::string &pattern);
	std::optional<std::vector<std::optional<std::string>>> match(const std::string &in, const std::string &pattern);

	std::wstring toWStr(const std::string &view);
	std::string toStr(const std::wstring &view);
	std::u32string toUStr(const std::string &view);
	std::string toStr(const std::u32string &view);
	int maxUnicode(const std::string &view);

	std::string toLowerCase(const std::string &view);
	std::string toUpperCase(const std::string &view);

	float compareRelative(const std::string &left, const std::string &right);

	template<typename T, typename = typename std::enable_if<
			std::__is_one_of<T, int, unsigned long, long, long long,
			unsigned long long, long double, float, double>::value>::type>
	inline T toVal(const std::string &view)
	{
		std::string str;

		if constexpr(std::is_same<T, int>::value)
			return std::stoi(view.data());
		else if constexpr(std::is_same<T, unsigned long>::value)
			return std::stoul(view.data());
		else if constexpr(std::is_same<T, long>::value)
			return std::stol(view.data());
		else if constexpr(std::is_same<T, long long>::value)
			return std::stoll(view.data());
		else if constexpr(std::is_same<T, unsigned long long>::value)
			return std::stoull(view.data());
		else if constexpr(std::is_same<T, long double>::value)
			return std::stold(view.data());
		else if constexpr(std::is_same<T, float>::value)
			return std::stof(view.data());
		else if constexpr(std::is_same<T, double>::value)
			return std::stod(view.data());

		return 0;
	}

	template<typename T, typename = typename std::enable_if<
			std::__is_one_of<T, int, unsigned long, long, long long,
			unsigned long long, long double, float, double>::value>::type>
	inline T toValOrDef(const std::string &view, T def = 0, bool *ok = nullptr)
	{
		if(ok)
			*ok = true;

		try {
			if constexpr(std::is_same<T, int>::value)
				return std::stoi(view.data());
			else if constexpr(std::is_same<T, unsigned long>::value)
				return std::stoul(view.data());
			else if constexpr(std::is_same<T, long>::value)
				return std::stol(view.data());
			else if constexpr(std::is_same<T, long long>::value)
				return std::stoll(view.data());
			else if constexpr(std::is_same<T, unsigned long long>::value)
				return std::stoull(view.data());
			else if constexpr(std::is_same<T, long double>::value)
				return std::stold(view.data());
			else if constexpr(std::is_same<T, float>::value)
				return std::stof(view.data());
			else if constexpr(std::is_same<T, double>::value)
				return std::stod(view.data());
		} catch(...) {
			if(ok)
				*ok = false;
		}

		return def;
	}
}

namespace Enc {

	template<typename T, typename = typename std::enable_if<
						std::is_integral_v<T>
					>::type>
	std::string toHex(T value)
	{
		std::stringstream stream;
		stream << std::setfill('0') << std::setw(sizeof(T)*2)
			   << std::hex << uint64_t(value);
		return stream.str();
	};

	std::string toHex(const uint8_t *data, size_t size);
	void fromHex(const std::string &in, uint8_t *data);

	std::string toBase64(const uint8_t *data, size_t size);
	ByteBuffer fromBase64(const std::string &view);
	void base64UrlConvert(std::string &in);

	// LowerCase
	std::string md5(const uint8_t *data, size_t size);
	inline std::string md5(const std::string &str) { return md5((const uint8_t*) str.data(), str.size()); }
	std::string sha1(const uint8_t *data, size_t size);
	inline std::string sha1(const std::string &str) { return sha1((const uint8_t*) str.data(), str.size()); }
}

using namespace std::chrono;

namespace Time {
	template<typename T>
	inline void sleep(T val)		 { std::this_thread::sleep_for(val); }
	inline void sleep3(uint64_t mls) { std::this_thread::sleep_for(std::chrono::milliseconds 	{ mls }); }
	inline void sleep6(uint64_t mcs) { std::this_thread::sleep_for(std::chrono::microseconds 	{ mcs }); }
	inline void sleep9(uint64_t nas) { std::this_thread::sleep_for(std::chrono::nanoseconds		{ nas }); }

	// Системное время, может изменяться в обратку
	inline uint64_t nowSystem() { return std::chrono::system_clock::now().time_since_epoch().count(); }
	// Время с запуска хоста, только растёт
	inline uint64_t nowSteady() { return std::chrono::steady_clock::now().time_since_epoch().count(); }
	// Максимально точное время
	inline uint64_t nowHigh()	{ return std::chrono::high_resolution_clock::now().time_since_epoch().count(); }

	inline std::time_t getTime() { return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); }
	// Количество секунд с начала эпохи на часах системы
	inline uint64_t getSeconds() { return std::chrono::system_clock::now().time_since_epoch().count() / 1000000000ull; }

	inline uint32_t getDay()     { return getSeconds() / 86400; }

	// yyyy.mm.dd
	std::string getDateAsString();
	// hh:mm:ss
	std::string getTimeAsString();
}

//[0.0,1.0)
inline double genRand(double min = 1, double max = 0)
{
	double res;
#ifdef _WIN32
	res = std::clamp(rand() / double(RAND_MAX), 0., 0.9999999);
#else
	res = drand48();
#endif
	return res*(max-min)+min;
}

std::string makeStacktrace(int stack_up = 1);

struct Timer
{
	boost::timer::cpu_timer Obj;

	/* Возвращает прошедшее время в наносекундах 1/1'000'000'000 */
	uint64_t timePastNan()
	{
		auto time = Obj.elapsed();
		return time.wall;
	}
	/* Возвращает прошедшее время в милисекундах 1/1'000 */
	uint32_t timePastMil() { return timePastNan()/1000000; }

	/* Возвращает затраченное процессорное время в наносекундах 1/1'000'000'000  */
	uint64_t timeUsedNan()
	{
		auto time = Obj.elapsed();
		return time.system+time.user;
	}
	/* Возвращает затраченное процессорное время в милисекундах 1/1'000 */
	uint32_t timeUsedMil() { return timeUsedNan()/1000000; }

	/* Обнуляет счетчик таймера и запускает его */
	void start() { Obj.start(); }
	/* Запускает таймер */
	void resume() { Obj.resume(); }
	/* Останавливает таймер */
	void stop() { Obj.stop(); }
	bool isStopped() { return Obj.is_stopped(); }
};

class DynamicLibrary {
	std::string Name, Path;
	uint64_t FD;
	static bool IsOverWine;

	static void symbolOut();
	static bool checkWine();

public:
	DynamicLibrary(const std::string &name_or_path);
	DynamicLibrary(const char *name_or_path) : DynamicLibrary(std::string(name_or_path)) {}
	~DynamicLibrary();

	DynamicLibrary(const DynamicLibrary&) = delete;
	DynamicLibrary(DynamicLibrary&&);
	DynamicLibrary& operator=(const DynamicLibrary&) = delete;
	DynamicLibrary& operator=(DynamicLibrary&&);

	std::string getName() const { return Name; }
	std::string getPath() const { return Path; }
	uint64_t getHandler() const { return FD; }

	// Запрашивает адрес символа из библиотеки, иначе генерирует ошибку
	void* getSymbol(const std::string &name) const;
	template<typename Result, typename ...Args>
		inline void getSymbol(Result (*&to)(Args...), const std::string &name) const { to = (Result (*)(Args...)) getSymbol(name); }
	// Запрашивает адрес символа из библиотеки, если отсутствует ссылает на &symbolOut
	void* takeSymbol(const std::string &name) const;
	template<typename Result, typename ...Args>
		inline void takeSymbol(Result (*&to)(Args...), const std::string &name) const { to = (Result (*)(Args...)) takeSymbol(name); }

	// Запрашивает адрес из глобального пространства, если нет, то ошибка
	static void* getDynamicSymbol(const std::string &name);
	// Вместо ошибки даёт symbolOut
	static void* takeDynamicSymbol(const std::string &name);
	// 
	static void* hasDynamicSymbol(const std::string &name);
	// Проверяет наличие или загружает библиотеку с глобальной линковкой функций
	static void needGlobalLibraryTemplate(const std::string &name);
	static std::vector<std::filesystem::path> getLdPaths();
	static bool isOverWine() { return IsOverWine; }
};

enum struct EnumLogType : int {
	Debug = 1, Info = 2, Warn = 4, Error = 8, All = 15
};


class LogSession : public std::stringstream {
	bool NeedF = false, NeedC = false;
	std::string Path;

public:
	LogSession(const std::string &name, EnumLogType type);
	~LogSession();

	LogSession(const LogSession&) = delete;
	LogSession(LogSession&&) = delete;
	LogSession& operator=(const LogSession&) = delete;
	LogSession& operator=(LogSession&&) = delete;
};

class Logger {
	std::string Path;

public:
	Logger(const std::string &path) : Path(path) {}
	Logger(const char *path) : Path(path) {}
	~Logger() = default;

	inline LogSession print(EnumLogType type) const { return LogSession(Path, type); }
	inline LogSession debug() const { return print(EnumLogType::Debug); }
	inline LogSession info()  const { return print(EnumLogType::Info); 	}
	inline LogSession warn()  const { return print(EnumLogType::Warn); 	}
	inline LogSession error() const { return print(EnumLogType::Error); }

	void setName(const std::string &path) { Path = path; }

	static LogSession print(const std::string &path) { return LogSession(path, EnumLogType::All); }
	static void addLogFile(const std::string &regex_for_path, EnumLogType levels, const std::filesystem::path &file, bool rewrite = false);
	static void addLogOutput(const std::string &regex_for_path, EnumLogType levels);
};

}

#define MAKE_ERROR(arg) throw std::runtime_error((std::ostringstream() << arg).str())
