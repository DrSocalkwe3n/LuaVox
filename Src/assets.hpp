#pragma once

#include <istream>
#include <memory>

namespace AL {

namespace detail {
struct membuf : std::streambuf {
    membuf(char* begin, size_t size) {
        setg(begin, begin, begin+size);
    }
};
}

struct iBinaryStream : detail::membuf {
    std::istream Stream;

    iBinaryStream(const char* begin, size_t size)
        : detail::membuf(const_cast<char*>(begin), size), Stream(this)
    {}
};


class Resource {
protected:
    const uint8_t* Data;
    size_t Size;


public:
    Resource();
    Resource(const uint8_t* data, size_t size)
        : Data(data), Size(size)
    {}

    virtual ~Resource();

    Resource(const Resource&) = delete;
    Resource(Resource&&) = delete;
    Resource& operator=(const Resource&) = delete;
    Resource& operator=(Resource&&) = delete;

    const uint8_t* getData() const { return Data; }
    size_t getSize() const { return Size; }

    std::string_view makeView() const { return std::string_view((const char*) Data, Size); }
    iBinaryStream makeStream() const { return iBinaryStream((const char*) Data, Size); }

};

std::shared_ptr<Resource> getResource(const std::string &path);

}