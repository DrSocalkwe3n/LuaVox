#include "Abstract.hpp"
#include "Common/Net.hpp"
#include "TOSLib.hpp"
#include "boost/json.hpp"
#include <algorithm>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <cstddef>
#include <endian.h>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>


namespace LV {


CompressedVoxels compressVoxels_byte(const std::vector<VoxelCube>& voxels) {
    std::u8string compressed;
    std::vector<DefVoxelId> defines;
    DefVoxelId maxValue = 0;
    defines.reserve(voxels.size());

    compressed.push_back(1);

    assert(voxels.size() <= 65535);

    for(const VoxelCube& cube : voxels) {
        defines.push_back(cube.VoxelId);
        if(cube.VoxelId > maxValue)
            maxValue = cube.VoxelId;
    }

    {
        std::sort(defines.begin(), defines.end());
        auto last = std::unique(defines.begin(), defines.end());
        defines.erase(last, defines.end());
        defines.shrink_to_fit();
    }

    // Количество байт на идентификатор в сыром виде
    uint8_t bytes_raw = std::ceil(std::log2(maxValue)/8);
    assert(bytes_raw >= 1 && bytes_raw <= 3);
    // Количество байт без таблицы индексов
    size_t size_in_raw = (bytes_raw+6)*voxels.size();
    // Количество байт на индекс
    uint8_t bytes_per_define = std::ceil(std::log2(defines.size())/8);
    assert(bytes_per_define == 1 || bytes_per_define == 2);
    // Количество байт после таблицы индексов
    size_t size_after_indices = (bytes_per_define+6)*voxels.size();
    // Размер таблицы индексов
    size_t indeces_size = 2+bytes_raw*defines.size();

    if(indeces_size+size_after_indices < size_in_raw) {
        // Выгодней писать с таблицей индексов

        // Индексы, размер идентификатора ключа к таблице, размер значения таблицы
        compressed.push_back(1 | (bytes_per_define << 1) | (bytes_raw << 2));
        compressed.push_back(defines.size() & 0xff);
        compressed.push_back((defines.size() >> 8) & 0xff);

        // Таблица
        for(DefVoxelId id : defines) {
            compressed.push_back(id & 0xff);
            if(bytes_raw > 1)
                compressed.push_back((id >> 8) & 0xff);
            if(bytes_raw > 2)
                compressed.push_back((id >> 16) & 0xff);
        }

        compressed.push_back(voxels.size() & 0xff);
        compressed.push_back((voxels.size() >> 8) & 0xff);

        for(const VoxelCube& cube : voxels) {
            size_t index = std::binary_search(defines.begin(), defines.end(), cube.VoxelId);
            compressed.push_back(index & 0xff);
            if(bytes_per_define > 1)
                compressed.push_back((index >> 8) & 0xff);
            
            compressed.push_back(cube.Meta);
            compressed.push_back(cube.Pos.x);
            compressed.push_back(cube.Pos.y);
            compressed.push_back(cube.Pos.z);
            compressed.push_back(cube.Size.x);
            compressed.push_back(cube.Size.y);
            compressed.push_back(cube.Size.z);
        }
    } else {
        compressed.push_back(0 | (0 << 1) | (bytes_raw << 2));

        compressed.push_back(voxels.size() & 0xff);
        compressed.push_back((voxels.size() >> 8) & 0xff);

        for(const VoxelCube& cube : voxels) {
            compressed.push_back(cube.VoxelId & 0xff);
            if(bytes_raw > 1)
                compressed.push_back((cube.VoxelId >> 8) & 0xff);
            if(bytes_raw > 2)
                compressed.push_back((cube.VoxelId >> 16) & 0xff);
            
            compressed.push_back(cube.Meta);
            compressed.push_back(cube.Pos.x);
            compressed.push_back(cube.Pos.y);
            compressed.push_back(cube.Pos.z);
            compressed.push_back(cube.Size.x);
            compressed.push_back(cube.Size.y);
            compressed.push_back(cube.Size.z);
        }
    }

    return {compressLinear(compressed), defines};
}

CompressedVoxels compressVoxels_bit(const std::vector<VoxelCube>& voxels) {
    std::vector<DefVoxelId> profile;
    std::vector<DefVoxelId> one_byte[7];

    DefVoxelId maxValueProfile = 0;
    DefVoxelId maxValues[7] = {0};

    profile.reserve(voxels.size());
    for(int iter = 0; iter < 7; iter++)
        one_byte[iter].reserve(voxels.size());

    assert(voxels.size() <= 65535);
    for(const VoxelCube& cube : voxels) {
        profile.push_back(cube.VoxelId);

        one_byte[0].push_back(cube.Meta);
        one_byte[1].push_back(cube.Pos.x);
        one_byte[2].push_back(cube.Pos.y);
        one_byte[3].push_back(cube.Pos.z);
        one_byte[4].push_back(cube.Size.x);
        one_byte[5].push_back(cube.Size.y);
        one_byte[6].push_back(cube.Size.z);

        if(cube.VoxelId > maxValueProfile)
            maxValueProfile = cube.VoxelId;
        if(cube.Meta > maxValues[0])
            maxValues[0] = cube.Meta;
        if(cube.Pos.x > maxValues[1])
            maxValues[1] = cube.Pos.x;
        if(cube.Pos.y > maxValues[2])
            maxValues[2] = cube.Pos.y;
        if(cube.Pos.z > maxValues[3])
            maxValues[3] = cube.Pos.z;
        if(cube.Size.x > maxValues[4])
            maxValues[4] = cube.Size.x;
        if(cube.Size.y > maxValues[5])
            maxValues[5] = cube.Size.y;
        if(cube.Size.z > maxValues[6])
            maxValues[6] = cube.Size.z;
    }

    {
        std::sort(profile.begin(), profile.end());
        auto last = std::unique(profile.begin(), profile.end());
        profile.erase(last, profile.end());
        profile.shrink_to_fit();
    }

    for (int iter = 0; iter < 7; iter++) {
        std::sort(one_byte[iter].begin(), one_byte[iter].end());
        auto last = std::unique(one_byte[iter].begin(), one_byte[iter].end());
        one_byte[iter].erase(last, one_byte[iter].end());
    }

    // Количество бит на идентификатор в сыром виде
    size_t bits_raw_profile = std::ceil(std::log2(maxValueProfile));
    assert(bits_raw_profile >= 1 && bits_raw_profile <= 24);
    size_t bits_index_profile = std::ceil(std::log2(profile.size()));
    bool indices_profile = 16+bits_raw_profile*profile.size()+bits_index_profile*voxels.size() < bits_raw_profile*voxels.size();

    size_t bits_raw[7];
    size_t bits_index[7];
    bool indices[7];

    for(int iter = 0; iter < 7; iter++) {
        bits_raw[iter] = std::ceil(std::log2(maxValues[iter]));
        assert(bits_raw[iter] >= 1 && bits_raw[iter] <= 8);
        bits_index[iter] = std::ceil(std::log2(one_byte[iter].size()));
    }

    std::vector<bool> buff;

    buff.push_back(indices_profile);
    for(int iter = 0; iter < 7; iter++)
        buff.push_back(indices[iter]);

    auto write = [&](size_t value, int count) {
        for(int iter = 0; iter < count; iter++)
            buff.push_back((value >> iter) & 0x1);
    };

    write(0, 8);

    // Таблицы
    if(indices_profile) {
        write(profile.size(), 16);
        write(bits_raw_profile, 5);
        write(bits_index_profile, 4);

        for(DefNodeId id : profile)
            write(id, bits_raw_profile);
    } else {
        write(bits_raw_profile, 5);
    }

    for(int iter = 0; iter < 7; iter++) {
        if(indices[iter]) {
            write(one_byte[iter].size(), 16);
            write(bits_raw[iter], 3);
            write(bits_index[iter], 3);

            for(uint8_t id : one_byte[iter])
                write(id, bits_raw[iter]);
        } else {
            write(bits_raw[iter], 3);
        }
    }

    // Данные

    write(voxels.size(), 16);

    for(const VoxelCube& cube : voxels) {
        if(indices_profile)
            write(std::binary_search(profile.begin(), profile.end(), cube.VoxelId), bits_index_profile);
        else
            write(cube.VoxelId, bits_raw_profile);

        for(int iter = 0; iter < 7; iter++) {
            uint8_t val;
            if(iter == 0) val = cube.Meta;
            else if(iter == 1) val = cube.Pos.x;
            else if(iter == 2) val = cube.Pos.y;
            else if(iter == 3) val = cube.Pos.z;
            else if(iter == 4) val = cube.Size.x;
            else if(iter == 5) val = cube.Size.y;
            else if(iter == 6) val = cube.Size.z;

            if(indices[iter])
                write(std::binary_search(one_byte[iter].begin(), one_byte[iter].end(), val), bits_index[iter]);
            else
                write(val, bits_raw[iter]);
        }
    }

    std::u8string compressed((buff.size()+7)/8, '\0');

    for(int begin = 0, end = compressed.size()*8-buff.size(); begin < end; begin++)
        compressed.push_back(0);

    for(size_t iter = 0; iter < buff.size(); iter++)
        compressed[iter / 8] |= (buff[iter] << (iter % 8));

    return {compressLinear(compressed), profile};
}

CompressedVoxels compressVoxels(const std::vector<VoxelCube>& voxels, bool fast) {
    if(fast)
        return compressVoxels_byte(voxels);
    else
        return compressVoxels_bit(voxels);
}

std::vector<VoxelCube> unCompressVoxels_byte(const std::u8string& compressed) {
    size_t pos = 1;

    auto read = [&]() -> size_t {
        assert(pos < compressed.size());
        return compressed[pos++];
    };

    uint8_t cmd = read();

    if(cmd & 0x1) {
        // Таблица
        uint8_t bytes_per_define = (cmd >> 1) & 0x1;
        uint8_t bytes_raw = (cmd >> 2) & 0x3;

        std::vector<DefVoxelId> defines;
        defines.resize(read() | (read() << 8));

        for(size_t iter = 0; iter < defines.size(); iter++) {
            DefVoxelId id = read();
            if(bytes_raw > 1)
                id |= read() << 8;
            if(bytes_raw > 2)
                id |= read() << 16;
        }

        std::vector<VoxelCube> voxels;
        voxels.resize(read() | (read() << 8));

        for(size_t iter = 0; iter < voxels.size(); iter++) {
            size_t index = read();

            if(bytes_per_define > 1)
                index |= read() << 8;

            VoxelCube &cube = voxels[iter];

            assert(index < defines.size());

            cube.VoxelId = defines[index];
            cube.Meta = read();
            cube.Pos.x = read();
            cube.Pos.y = read();
            cube.Pos.z = read();
            cube.Size.x = read();
            cube.Size.y = read();
            cube.Size.z = read();
        }

        return voxels;
    } else {
        uint8_t bytes_raw = (cmd >> 2) & 0x3;

        std::vector<VoxelCube> voxels;
        voxels.resize(read() | (read() << 8));

        for(size_t iter = 0; iter < voxels.size(); iter++) {
            VoxelCube &cube = voxels[iter];

            cube.VoxelId = read();
            if(bytes_raw > 1)
                cube.VoxelId |= read() << 8;
            if(bytes_raw > 2)
                cube.VoxelId |= read() << 16;

            cube.Meta = read();
            cube.Pos.x = read();
            cube.Pos.y = read();
            cube.Pos.z = read();
            cube.Size.x = read();
            cube.Size.y = read();
            cube.Size.z = read();
        }

        return voxels;
    }
}

std::vector<VoxelCube> unCompressVoxels_bit(const std::u8string& compressed) {
    size_t pos = 1;

    auto read = [&](int bits) -> size_t {
        size_t out = 0;
        for(int iter = 0; iter < bits; iter++, pos++) {
            assert(pos < compressed.size()*8);

            out |= (compressed[pos / 8] >> (pos % 8)) << iter;
        }

        return out;
    };

    bool indices_profile = read(1);
    bool indices[7];
    
    for(int iter = 0; iter < 7; iter++)
        indices[iter] = read(1);

    std::vector<DefVoxelId> profile;
    std::vector<DefVoxelId> one_byte[7];
    uint8_t bits_raw_profile;
    uint8_t bits_index_profile;
    size_t bits_raw[7];
    size_t bits_index[7];

    // Таблицы
    if(indices_profile) {
        profile.resize(read(16));
        bits_raw_profile = read(5);
        bits_index_profile = read(4);

        for(size_t iter = 0; iter < profile.size(); iter++)
            profile[iter] = read(bits_raw_profile);
    } else {
        bits_raw_profile = read(5);
    }

    for(int iter = 0; iter < 7; iter++) {
        if(indices[iter]) {
            one_byte[iter].resize(read(16));
            bits_raw[iter] = read(3);
            bits_index[iter] = read(3);

            for(size_t iter2 = 0; iter2 < one_byte[iter].size(); iter2++)
                 one_byte[iter][iter2] = read(bits_raw[iter]);
        } else {
            bits_raw[iter] = read(3);
        }
    }

    // Данные
    std::vector<VoxelCube> voxels;
    voxels.resize(read(16));

    for(size_t iter = 0; iter < voxels.size(); iter++) {
        VoxelCube &cube = voxels[iter];

        if(indices_profile)
            cube.VoxelId = profile[read(bits_index_profile)];
        else
            cube.VoxelId = read(bits_raw_profile);

        for(int iter = 0; iter < 7; iter++) {
            uint8_t val;

            if(indices[iter])
                val = one_byte[iter][read(bits_index[iter])];
            else
                val = read(bits_raw[iter]);

            if(iter == 0) cube.Meta = val;
            else if(iter == 1) cube.Pos.x = val;
            else if(iter == 2) cube.Pos.y = val;
            else if(iter == 3) cube.Pos.z = val;
            else if(iter == 4) cube.Size.x = val;
            else if(iter == 5) cube.Size.y = val;
            else if(iter == 6) cube.Size.z = val;
        }
    }

    return voxels;
}

std::vector<VoxelCube> unCompressVoxels(const std::u8string& compressed) {
    const std::u8string& next = unCompressLinear(compressed);
    if(next.front())
        return unCompressVoxels_byte(next);
    else
        return unCompressVoxels_bit(next);
}



CompressedNodes compressNodes_byte(const Node* nodes) {
    std::u8string compressed;

    std::vector<DefNodeId> profiles;

    profiles.reserve(16*16*16);

    compressed.push_back(1);

    DefNodeId maxValueProfile = 0;

    for(size_t iter = 0; iter < 16*16*16; iter++) {
        const Node &node = nodes[iter];

        profiles.push_back(node.NodeId);

        if(node.NodeId > maxValueProfile)
            maxValueProfile = node.NodeId;
    }

    {
        std::sort(profiles.begin(), profiles.end());
        auto last = std::unique(profiles.begin(), profiles.end());
        profiles.erase(last, profiles.end());
        profiles.shrink_to_fit();
    }


    // Количество байт на идентификатор в сыром виде
    uint8_t bytes_raw_profile = std::ceil(std::log2(maxValueProfile+1)/8);
    assert(bytes_raw_profile >= 0 && bytes_raw_profile <= 3);
    // Количество байт на индекс
    uint8_t bytes_indices_profile = std::ceil(std::log2(profiles.size())/8);
    assert(bytes_indices_profile >= 0 && bytes_indices_profile <= 2);

    bool indices_profile = 3+bytes_raw_profile*profiles.size()+bytes_indices_profile*16*16*16 < bytes_raw_profile*16*16*16;

    compressed.push_back(indices_profile | (bytes_raw_profile << 1) | (bytes_indices_profile << 3));
    
    if(indices_profile) {
        // Таблица
        compressed.push_back(profiles.size() & 0xff);
        compressed.push_back((profiles.size() >> 8) & 0xff);
        compressed.push_back((profiles.size() >> 16) & 0xff);

        for(DefNodeId id : profiles) {
            if(bytes_raw_profile > 0)
                compressed.push_back(id & 0xff);
            if(bytes_raw_profile > 1)
                compressed.push_back((id >> 8) & 0xff);
            if(bytes_raw_profile > 2)
                compressed.push_back((id >> 16) & 0xff);
        }

        // Данные
        for(size_t iter = 0; iter < 16*16*16; iter++) {
            const Node &node = nodes[iter];

            size_t index = std::binary_search(profiles.begin(), profiles.end(), node.NodeId);
            
            compressed.push_back(index & 0xff);
            if(bytes_indices_profile > 1)
                compressed.push_back((index >> 8) & 0xff);

            compressed.push_back(node.Meta);
        }
    } else {
        for(size_t iter = 0; iter < 16*16*16; iter++) {
            const Node &node = nodes[iter];

            if(bytes_raw_profile > 0)
                compressed.push_back(node.NodeId & 0xff);
            if(bytes_raw_profile > 1)
                compressed.push_back((node.NodeId >> 8) & 0xff);
            if(bytes_raw_profile > 2)
                compressed.push_back((node.NodeId >> 8) & 0xff);

            compressed.push_back(node.Meta);
        }
    }

    profiles.shrink_to_fit();
    
    return {compressLinear(compressed), profiles};
}

CompressedNodes compressNodes_bit(const Node* nodes) {
    std::u8string compressed;

    std::vector<DefNodeId> profiles;
    std::vector<DefNodeId> meta;

    profiles.reserve(16*16*16);
    meta.reserve(16*16*16);

    compressed.push_back(1);

    DefNodeId maxValueProfile = 0,
        maxValueMeta = 0;

    for(size_t iter = 0; iter < 16*16*16; iter++) {
        const Node &node = nodes[iter];

        profiles.push_back(node.NodeId);
        meta.push_back(node.Meta);

        if(node.NodeId > maxValueProfile)
            maxValueProfile = node.NodeId;

        if(node.Meta > maxValueMeta)
            maxValueMeta = node.Meta;
    }

    {
        std::sort(profiles.begin(), profiles.end());
        auto last = std::unique(profiles.begin(), profiles.end());
        profiles.erase(last, profiles.end());
        profiles.shrink_to_fit();
    }

    {
        std::sort(meta.begin(), meta.end());
        auto last = std::unique(meta.begin(), meta.end());
        meta.erase(last, meta.end());
        meta.shrink_to_fit();
    }


    // Количество бит на идентификатор в сыром виде
    uint8_t bits_raw_profile = std::ceil(std::log2(maxValueProfile+1));
    assert(bits_raw_profile >= 1 && bits_raw_profile <= 24);
    // Количество бит на индекс
    uint8_t bits_indices_profile = std::ceil(std::log2(profiles.size()));
    assert(bits_indices_profile >= 1 && bits_indices_profile <= 16);

    bool indices_profile = 3*8+bits_raw_profile*profiles.size()+bits_indices_profile*16*16*16 < bits_raw_profile*16*16*16;


    std::vector<bool> buff;

    auto write = [&](size_t value, int count) {
        for(int iter = 0; iter < count; iter++)
            buff.push_back((value >> iter) & 0x1);
    };


    write(indices_profile, 1);
    write(bits_raw_profile, 5);
    write(bits_indices_profile, 4);

    // Количество бит на идентификатор в сыром виде
    uint8_t bits_raw_meta = std::ceil(std::log2(maxValueMeta+1));
    assert(bits_raw_meta >= 1 && bits_raw_meta <= 8);
    // Количество бит на индекс
    uint8_t bits_indices_meta = std::ceil(std::log2(meta.size()));
    assert(bits_indices_meta >= 1 && bits_indices_meta <= 8);

    bool indices_meta = 3*8+bits_raw_meta*profiles.size()+bits_indices_meta*16*16*16 < bits_raw_meta*16*16*16;

    write(indices_meta, 1);
    write(bits_raw_meta, 3);
    write(bits_indices_meta, 3);
    
    // Таблицы
    if(indices_profile) {
        write(profiles.size(), 12);

        for(DefNodeId id : profiles) {
            write(id, bits_raw_profile);
        }
    }


    if(indices_meta) {
        write(meta.size(), 8);

        for(DefNodeId id : meta) {
            write(id, bits_raw_meta);
        }
    }


    // Данные
    for(size_t iter = 0; iter < 16*16*16; iter++) {
        const Node &node = nodes[iter];

        if(indices_profile) {
            size_t index = std::binary_search(profiles.begin(), profiles.end(), node.NodeId);
            write(index, bits_indices_profile);
        } else {
            write(node.NodeId, bits_raw_profile);
        }

        if(indices_meta) {
            size_t index = std::binary_search(meta.begin(), meta.end(), node.Meta);
            write(index, bits_indices_meta);
        } else {
            write(node.Meta, bits_raw_meta);
        }
    }
    
    return {compressLinear(compressed), profiles};
}

CompressedNodes compressNodes(const Node* nodes, bool fast) {
    std::u8string data(16*16*16*sizeof(Node), '\0');
    const char8_t *ptr = (const char8_t*) nodes;
    std::copy(ptr, ptr+16*16*16*4, data.data());

    std::vector<DefNodeId> node(16*16*16);
    for(int iter = 0; iter < 16*16*16; iter++) {
        node[iter] = nodes[iter].NodeId;
    }

    {
        std::sort(node.begin(), node.end());
        auto last = std::unique(node.begin(), node.end());
        node.erase(last, node.end());
        node.shrink_to_fit();
    }

    return {compressLinear(data), std::move(node)};

    // if(fast)
    //     return compressNodes_byte(nodes);
    // else
    //     return compressNodes_bit(nodes);
}

void unCompressNodes_byte(const std::u8string& compressed, Node* ptr) {
    size_t pos = 1;

    auto read = [&]() -> size_t {
        assert(pos < compressed.size());
        return compressed[pos++];
    };

    uint8_t value = read();

    uint8_t bytes_raw_profile = (value >> 1) & 0x3;
    uint8_t bytes_indices_profile = (value >> 3) & 0x3;
    bool indices_profile = value & 0x1;

    if(indices_profile) {
        std::vector<DefNodeId> profiles;
        profiles.resize(read() | (read() << 8) | (read() << 16));

        for(size_t iter = 0; iter < profiles.size(); iter++) {
            DefNodeId id = 0;
            
            if(bytes_raw_profile > 0)
                id = read();
            if(bytes_raw_profile > 1)
                id |= read() << 8;
            if(bytes_raw_profile > 2)
                id |= read() << 16;
        }

        for(size_t iter = 0; iter < 16*16*16; iter++) {
            Node &node = ptr[iter];

            DefNodeId index = read();
            if(bytes_indices_profile > 1)
                index |= read() << 8;

            node.NodeId = profiles[index];
            node.Meta = read();
        }
    } else {
        for(size_t iter = 0; iter < 16*16*16; iter++) {
            Node &node = ptr[iter];

            node.NodeId = 0;

            if(bytes_raw_profile > 0)
                node.NodeId = read();
            if(bytes_raw_profile > 1)
                node.NodeId |= read() << 8;
            if(bytes_raw_profile > 2)
                node.NodeId |= read() << 16;

            node.Meta = read();
        }
    }
}

void unCompressNodes_bit(const std::u8string& compressed, Node* ptr) {
    size_t pos = 1;

    auto read = [&](int bits) -> size_t {
        size_t out = 0;
        for(int iter = 0; iter < bits; iter++, pos++) {
            assert(pos < compressed.size()*8);

            out |= (compressed[pos / 8] >> (pos % 8)) << iter;
        }

        return out;
    };

    std::vector<DefNodeId> meta;


    bool indices_profile = read(1);
    uint8_t bits_raw_profile = read(5);
    uint8_t bits_indices_profile = read(4);

    bool indices_meta = read(1);
    uint8_t bits_raw_meta = read(3);
    uint8_t bits_indices_meta = read(3);

    std::vector<DefNodeId> profiles;
    
    // Таблицы
    if(indices_profile) {
        profiles.resize(read(12));

        for(size_t iter = 0; iter < profiles.size(); iter++) {
            profiles[iter] = read(bits_raw_profile);
        }
    }


    if(indices_meta) {
        meta.resize(read(8));

        for(size_t iter = 0; iter < meta.size(); iter++) {
            meta[iter] = read(bits_raw_meta);
        }
    }


    // Данные
    for(size_t iter = 0; iter < 16*16*16; iter++) {
        Node &node = ptr[iter];

        if(indices_profile) {
            node.NodeId = profiles[read(bits_indices_profile)];
        } else {
            node.NodeId = read(bits_raw_profile);
        }

        if(indices_meta) {
            node.Meta = meta[read(bits_indices_meta)];
        } else {
            node.Meta = read(bits_raw_meta);
        }
    }
}

void unCompressNodes(const std::u8string& compressed, Node* ptr) {
    const std::u8string& next = unCompressLinear(compressed);
    const Node *lPtr = (const Node*) next.data();
    std::copy(lPtr, lPtr+16*16*16, ptr);

    // if(next.front())
    //     return unCompressNodes_byte(next, ptr);
    // else
    //     return unCompressNodes_bit(next, ptr);
}

std::u8string compressLinear(const std::u8string& data) {
    std::stringstream in;
    in.write((const char*) data.data(), data.size());

    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::zlib_compressor());
    out.push(in);

    std::stringstream compressed;
    boost::iostreams::copy(out, compressed);
    std::string outString = compressed.str();

    return *(std::u8string*) &outString;
}

std::u8string unCompressLinear(const std::u8string& data) {
    std::stringstream in;
    in.write((const char*) data.data(), data.size());
    
    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::zlib_decompressor());
    out.push(in);

    std::stringstream compressed;
    boost::iostreams::copy(out, compressed);
    std::string outString = compressed.str();

    return *(std::u8string*) &outString;
}

PreparedNodeState::PreparedNodeState(const std::string_view modid, const js::object& profile) {
    for(auto& [condition, variability] : profile) {
        // Распарсить условие
        uint16_t node = parseCondition(condition);

        boost::container::small_vector<
            std::pair<float, std::variant<Model, VectorModel>>,
            1
        > models;

        if(variability.is_array()) {
            // Варианты условия
            for(const js::value& model : variability.as_array()) {
                models.push_back(parseModel(modid, model.as_object()));
            }

            HasVariability = true;
        } else if (variability.is_object()) {
            // Один список моделей на условие
            models.push_back(parseModel(modid, variability.as_object()));
        } else {
            MAKE_ERROR("Условию должен соответствовать список или объект");
        }

        Routes.emplace_back(node, std::move(models));
    }
}

PreparedNodeState::PreparedNodeState(const std::string_view modid, const sol::table& profile) {

}

PreparedNodeState::PreparedNodeState(const std::u8string& data) {
    Net::LinearReader lr(data);

    uint16_t size;
    lr >> size;

    ResourceToLocalId.reserve(size);
    for(int counter = 0; counter < size; counter++) {
        std::string domain, key;
        lr >> domain >> key;
        ResourceToLocalId.emplace_back(std::move(domain), std::move(key));
    }

    lr >> size;
    Nodes.reserve(size);
    for(int counter = 0; counter < size; counter++) {
        uint8_t index = lr.read<uint8_t>();
        Node node;

        switch(index) {
        case 0: node.v = Node::Num(lr.read<int>()); break;
        case 1: node.v = Node::Var(lr.read<std::string>()); break;
        case 2: {
            Node::Unary unary;
            unary.op = Op(lr.read<uint8_t>());
            unary.rhs = lr.read<uint16_t>();
            node.v = unary;
            break;
        }
        case 3: {
            Node::Binary binary;
            binary.op = Op(lr.read<uint8_t>());
            binary.lhs = lr.read<uint16_t>();
            binary.rhs = lr.read<uint16_t>();
            node.v = binary;
            break;
        }
        default:
            MAKE_ERROR("Ошибка формата данных");
        }

        Nodes.emplace_back(std::move(node));
    }

    lr >> size;
    Routes.reserve(size);
    for(int counter = 0; counter < size; counter++) {
        uint16_t nodeId, variantsSize;
        lr >> nodeId >> variantsSize;

        boost::container::small_vector<
            std::pair<float, std::variant<Model, VectorModel>>,
            1
        > variants;
        variants.reserve(variantsSize);

        for(int counter2 = 0; counter2 < variantsSize; counter2++) {
            float weight;
            lr >> weight;

            if(lr.read<uint8_t>()) {
                VectorModel mod;
                uint16_t modelsSize;
                lr >> modelsSize;
                mod.Models.reserve(modelsSize);

                for(int counter3 = 0; counter3 < modelsSize; counter3++) {
                    Model mod2;
                    lr >> mod2.Id;
                    mod2.UVLock = lr.read<uint8_t>();

                    uint16_t transformsSize;
                    lr >> transformsSize;

                    mod2.Transforms.reserve(transformsSize);
                    for(int counter4 = 0; counter4 < transformsSize; counter4++) {
                        Transformation tr;
                        tr.Op = Transformation::EnumTransform(lr.read<uint8_t>());
                        mod2.Transforms.push_back(tr);
                    }
                }

                mod.UVLock = lr.read<uint8_t>();

                uint16_t transformsSize;
                lr >> transformsSize;

                mod.Transforms.reserve(transformsSize);
                for(int counter3 = 0; counter3 < transformsSize; counter3++) {
                    Transformation tr;
                    tr.Op = Transformation::EnumTransform(lr.read<uint8_t>());
                    mod.Transforms.push_back(tr);
                }

                variants.emplace_back(weight, std::move(mod));
            } else {
                Model mod;
                lr >> mod.Id;
                mod.UVLock = lr.read<uint8_t>();
                uint16_t transformsSize;
                lr >> transformsSize;

                mod.Transforms.reserve(transformsSize);
                for(int counter3 = 0; counter3 < transformsSize; counter3++) {
                    Transformation tr;
                    tr.Op = Transformation::EnumTransform(lr.read<uint8_t>());
                    mod.Transforms.push_back(tr);
                }

                variants.emplace_back(weight, std::move(mod));
            }
        }
    }

}

std::u8string PreparedNodeState::dump() const {
    Net::Packet result;

    // ResourceToLocalId
    assert(ResourceToLocalId.size() < (1 << 16));
    result << uint16_t(ResourceToLocalId.size());

    for(const auto& [domain, key] : ResourceToLocalId) {
        assert(domain.size() < 32);
        result << domain;
        assert(key.size() < 32);
        result << key;
    }

    // Nodes
    assert(Nodes.size() < (1 << 16));
    result << uint16_t(Nodes.size());

    for(const Node& node : Nodes) {
        result << uint8_t(node.v.index());
        
        if(const Node::Num* val = std::get_if<Node::Num>(&node.v)) {
            result << val->v;
        } else if(const Node::Var* val = std::get_if<Node::Var>(&node.v)) {
            assert(val->name.size() < 32);
            result << val->name;
        } else if(const Node::Unary* val = std::get_if<Node::Unary>(&node.v)) {
            result << uint8_t(val->op) << val->rhs;
        } else if(const Node::Binary* val = std::get_if<Node::Binary>(&node.v)) {
            result << uint8_t(val->op) << val->lhs << val->rhs;
        } else {
            std::unreachable();
        }
    }

    // Routes
    assert(Routes.size() < (1 << 16));
    result << uint16_t(Routes.size());

    for(const auto& [nodeId, variants] : Routes) {
        result << nodeId;

        assert(variants.size() < (1 << 16));
        result << uint16_t(variants.size());

        for(const auto& [weight, model] : variants) {
            result << weight << uint8_t(model.index());
            
            if(const Model* val = std::get_if<Model>(&model)) {
                result << val->Id << uint8_t(val->UVLock);

                assert(val->Transforms.size() < (1 << 16));
                result << uint16_t(val->Transforms.size());

                for(const Transformation& val : val->Transforms) {
                    result << uint8_t(val.Op) << val.Value;
                }
            } else if(const VectorModel* val = std::get_if<VectorModel>(&model)) {
                assert(val->Models.size() < (1 << 16));
                result << uint16_t(val->Models.size());

                for(const Model& subModel : val->Models) {
                    result << subModel.Id << uint8_t(subModel.UVLock);

                    assert(subModel.Transforms.size() < (1 << 16));
                    result << uint16_t(subModel.Transforms.size());

                    for(const Transformation& val : subModel.Transforms)
                        result << uint8_t(val.Op) << val.Value;
                }

                result << uint8_t(val->UVLock);

                assert(val->Transforms.size() < (1 << 16));
                result << uint16_t(val->Transforms.size());

                for(const Transformation& val : val->Transforms)
                    result << uint8_t(val.Op) << val.Value;
            }
        }
    }

    return result.complite();
}

uint16_t PreparedNodeState::parseCondition(const std::string_view expression) {
    enum class EnumTokenKind {
        LParen, RParen,
        Plus, Minus, Star, Slash, Percent,
        Not, And, Or,
        LT, LE, GT, GE, EQ, NE
    };

    std::vector<std::variant<EnumTokenKind, std::string_view, int, uint16_t>> tokens;
    ssize_t pos = 0;
    auto skipWS = [&](){ while(pos<expression.size() && std::isspace((unsigned char) expression[pos])) ++pos; };

    for(; pos < expression.size(); pos++) {
        skipWS();

        char c = expression[pos];

        // Числа
        if(std::isdigit(c)) {
            ssize_t npos = pos;
            for(; npos < expression.size() && std::isdigit(expression[npos]); npos++);
            int value;
            std::string_view value_view = expression.substr(pos, npos-pos);
            auto [partial_ptr, partial_ec] = std::from_chars(value_view.data(), value_view.data() + value_view.size(), value);

            if(partial_ec == std::errc{} && partial_ptr != value_view.data() + value_view.size()) {
                MAKE_ERROR("Converted part of the string: " << value << " (remaining: " << std::string_view(partial_ptr, value_view.data() + value_view.size() - partial_ptr) << ")");
            } else if(partial_ec != std::errc{}) {
                MAKE_ERROR("Error converting partial string: " << value);
            }

            tokens.push_back(value);
            continue;
        }

        // Переменные
        if(std::isalpha(c) || c == ':') {
            ssize_t npos = pos;
            for(; npos < expression.size() && std::isalpha(expression[npos]); npos++);
            std::string_view value = expression.substr(pos, npos-pos);
            if(value == "true")
                tokens.push_back(1);
            else if(value == "false")
                tokens.push_back(0);
            else
                tokens.push_back(value);
            continue;
        }

        // Двойные операторы
        if(pos-1 < expression.size()) {
            char n = expression[pos+1];

            if(c == '<' && n == '=') {
                tokens.push_back(EnumTokenKind::LE);
                pos++;
                continue;
            } else if(c == '>' && n == '=') {
                tokens.push_back(EnumTokenKind::GE);
                pos++;
                continue;
            } else if(c == '=' && n == '=') {
                tokens.push_back(EnumTokenKind::EQ);
                pos++;
                continue;
            } else if(c == '!' && n == '=') {
                tokens.push_back(EnumTokenKind::NE);
                pos++;
                continue;
            }
        }

        // Операторы
        switch(c) {
            case '(': tokens.push_back(EnumTokenKind::LParen);
            case ')': tokens.push_back(EnumTokenKind::RParen);
            case '+': tokens.push_back(EnumTokenKind::Plus);
            case '-': tokens.push_back(EnumTokenKind::Minus);
            case '*': tokens.push_back(EnumTokenKind::Star);
            case '/': tokens.push_back(EnumTokenKind::Slash);
            case '%': tokens.push_back(EnumTokenKind::Percent);
            case '!': tokens.push_back(EnumTokenKind::Not);
            case '&': tokens.push_back(EnumTokenKind::And);
            case '|': tokens.push_back(EnumTokenKind::Or);
            case '<': tokens.push_back(EnumTokenKind::LT);
            case '>': tokens.push_back(EnumTokenKind::GT);
        }

        MAKE_ERROR("Недопустимый символ: " << c);
    }


    for(size_t index = 0; index < tokens.size(); index++) {
        auto &token = tokens[index];

        if(std::string_view* value = std::get_if<std::string_view>(&token)) {
            if(*value == "false") {
                token = 0;
            } else if(*value == "true") {
                token = 1;
            } else {
                Node node;
                node.v = Node::Var((std::string) *value);
                Nodes.emplace_back(std::move(node));
                assert(Nodes.size() < std::pow(2, 16)-64);
                token = uint16_t(Nodes.size()-1);
            }
        }
    }

    // Рекурсивный разбор выражений в скобках
    std::function<uint16_t(size_t pos)> lambdaParse = [&](size_t pos) -> uint16_t {
        size_t end = tokens.size();
        
        // Парсим выражения в скобках
        for(size_t index = pos; index < tokens.size(); index++) {
            if(EnumTokenKind* kind = std::get_if<EnumTokenKind>(&tokens[index])) {
                if(*kind == EnumTokenKind::LParen) {
                    uint16_t node = lambdaParse(index+1);
                    tokens.insert(tokens.begin()+index, node);
                    tokens.erase(tokens.begin()+index+1, tokens.begin()+index+3);
                    end = tokens.size();
                } else if(*kind == EnumTokenKind::RParen) {
                    end = index;
                    break;
                }
            }
        }

        // Обрабатываем унарные операции
        for(ssize_t index = end; index >= pos; index--) {
            if(EnumTokenKind *kind = std::get_if<EnumTokenKind>(&tokens[index])) {
                if(*kind != EnumTokenKind::Not && *kind != EnumTokenKind::Plus && *kind != EnumTokenKind::Minus)
                    continue;

                if(index+1 >= end)
                    MAKE_ERROR("Отсутствует операнд");

                auto rightToken = tokens[index+1];
                if(std::holds_alternative<EnumTokenKind>(rightToken))
                    MAKE_ERROR("Недопустимый операнд");

                if(int* value = std::get_if<int>(&rightToken)) {
                    if(*kind == EnumTokenKind::Not)
                        tokens[index] = *value ? 0 : 1;
                    else if(*kind == EnumTokenKind::Plus)
                        tokens[index] = +*value;
                    else if(*kind == EnumTokenKind::Minus)
                        tokens[index] = -*value;

                } else if(uint16_t* value = std::get_if<uint16_t>(&rightToken)) {
                    Node node;
                    Node::Unary un;
                    un.rhs = *value;

                    if(*kind == EnumTokenKind::Not)
                        un.op = Op::Not;
                    else if(*kind == EnumTokenKind::Plus)
                        un.op = Op::Pos;
                    else if(*kind == EnumTokenKind::Minus)
                        un.op = Op::Neg;

                    node.v = un;
                    Nodes.emplace_back(std::move(node));
                    assert(Nodes.size() < std::pow(2, 16)-64);
                    tokens[index] = uint16_t(Nodes.size()-1);
                }

                end--;
                tokens.erase(tokens.begin()+index+1);
            }
        }

        // Бинарные в порядке приоритета
        for(int priority = 0; priority < 6; priority++)
            for(size_t index = pos; index < end; index++) {
                EnumTokenKind *kind = std::get_if<EnumTokenKind>(&tokens[index]);

                if(!kind)
                    continue;

                if(priority == 0 && *kind != EnumTokenKind::Star && *kind != EnumTokenKind::Slash && *kind != EnumTokenKind::Percent)
                    continue;
                if(priority == 1 && *kind != EnumTokenKind::Plus && *kind != EnumTokenKind::Minus)
                    continue;
                if(priority == 2 && *kind != EnumTokenKind::LT && *kind != EnumTokenKind::GT && *kind != EnumTokenKind::LE && *kind != EnumTokenKind::GE)
                    continue;
                if(priority == 3 && *kind != EnumTokenKind::EQ && *kind != EnumTokenKind::NE)
                    continue;
                if(priority == 4 && *kind != EnumTokenKind::And)
                    continue;
                if(priority == 5 && *kind != EnumTokenKind::Or)
                    continue;

                if(index == pos)
                    MAKE_ERROR("Отсутствует операнд перед");
                else if(index == end-1)
                    MAKE_ERROR("Отсутствует операнд после");

                auto &leftToken = tokens[index-1];
                auto &rightToken = tokens[index+1];

                if(std::holds_alternative<EnumTokenKind>(leftToken))
                    MAKE_ERROR("Недопустимый операнд");

                if(std::holds_alternative<EnumTokenKind>(rightToken))
                    MAKE_ERROR("Недопустимый операнд");

                if(std::holds_alternative<int>(leftToken) && std::holds_alternative<int>(rightToken)) {
                    int value;

                    switch(*kind) {
                    case EnumTokenKind::Plus:       value = std::get<int>(leftToken) +  std::get<int>(rightToken); break;
                    case EnumTokenKind::Minus:      value = std::get<int>(leftToken) -  std::get<int>(rightToken); break;
                    case EnumTokenKind::Star:       value = std::get<int>(leftToken) *  std::get<int>(rightToken); break;
                    case EnumTokenKind::Slash:      value = std::get<int>(leftToken) /  std::get<int>(rightToken); break;
                    case EnumTokenKind::Percent:    value = std::get<int>(leftToken) %  std::get<int>(rightToken); break;
                    case EnumTokenKind::And:        value = std::get<int>(leftToken) && std::get<int>(rightToken); break;
                    case EnumTokenKind::Or:         value = std::get<int>(leftToken) || std::get<int>(rightToken); break;
                    case EnumTokenKind::LT:         value = std::get<int>(leftToken) <  std::get<int>(rightToken); break;
                    case EnumTokenKind::LE:         value = std::get<int>(leftToken) <= std::get<int>(rightToken); break;
                    case EnumTokenKind::GT:         value = std::get<int>(leftToken) >  std::get<int>(rightToken); break;
                    case EnumTokenKind::GE:         value = std::get<int>(leftToken) >= std::get<int>(rightToken); break;
                    case EnumTokenKind::EQ:         value = std::get<int>(leftToken) == std::get<int>(rightToken); break;
                    case EnumTokenKind::NE:         value = std::get<int>(leftToken) != std::get<int>(rightToken); break;

                    default: std::unreachable();
                    }


                    leftToken = value;
                } else {
                    Node node;
                    Node::Binary bin;

                    switch(*kind) {
                    case EnumTokenKind::Plus:       bin.op = Op::Add; break;
                    case EnumTokenKind::Minus:      bin.op = Op::Sub; break;
                    case EnumTokenKind::Star:       bin.op = Op::Mul; break;
                    case EnumTokenKind::Slash:      bin.op = Op::Div; break;
                    case EnumTokenKind::Percent:    bin.op = Op::Mod; break;
                    case EnumTokenKind::And:        bin.op = Op::And; break;
                    case EnumTokenKind::Or:         bin.op = Op::Or;  break;
                    case EnumTokenKind::LT:         bin.op = Op::LT;  break;
                    case EnumTokenKind::LE:         bin.op = Op::LE;  break;
                    case EnumTokenKind::GT:         bin.op = Op::GT;  break;
                    case EnumTokenKind::GE:         bin.op = Op::GE;  break;
                    case EnumTokenKind::EQ:         bin.op = Op::EQ;  break;
                    case EnumTokenKind::NE:         bin.op = Op::NE;  break;

                    default: std::unreachable();
                    }

                    if(int* value = std::get_if<int>(&leftToken)) {
                        Node valueNode;
                        valueNode.v = Node::Num(*value);
                        Nodes.emplace_back(std::move(valueNode));
                        assert(Nodes.size() < std::pow(2, 16)-64);
                        bin.lhs = uint16_t(Nodes.size()-1);
                    } else if(uint16_t* nodeId = std::get_if<uint16_t>(&leftToken)) {
                        bin.lhs = *nodeId;
                    }

                    if(int* value = std::get_if<int>(&rightToken)) {
                        Node valueNode;
                        valueNode.v = Node::Num(*value);
                        Nodes.emplace_back(std::move(valueNode));
                        assert(Nodes.size() < std::pow(2, 16)-64);
                        bin.rhs = uint16_t(Nodes.size()-1);
                    } else if(uint16_t* nodeId = std::get_if<uint16_t>(&rightToken)) {
                        bin.rhs = *nodeId;
                    }

                    Nodes.emplace_back(std::move(node));
                    assert(Nodes.size() < std::pow(2, 16)-64);
                    leftToken = uint16_t(Nodes.size()-1);
                }

                tokens.erase(tokens.begin()+index, tokens.begin()+index+2);
                end -= 2;
                index--;
            }

        if(tokens.size() != 1)
            MAKE_ERROR("Выражение не корректно");

        if(uint16_t* nodeId = std::get_if<uint16_t>(&tokens[0])) {
            return *nodeId;
        } else if(int* value = std::get_if<int>(&tokens[0])) {
            Node node;
            node.v = Node::Num(*value);
            Nodes.emplace_back(std::move(node));
            assert(Nodes.size() < std::pow(2, 16)-64);
            return uint16_t(Nodes.size()-1);
        } else {
            MAKE_ERROR("Выражение не корректно");
        }
    };

    uint16_t nodeId = lambdaParse(0);
    if(!tokens.empty())
        MAKE_ERROR("Выражение не действительно");

    return nodeId;

    // std::unordered_map<std::string, int> vars;
    // std::function<int(uint16_t)> lambdaCalcNode = [&](uint16_t nodeId) -> int {
    //     const Node& node = Nodes[nodeId];
    //     if(const Node::Num* value = std::get_if<Node::Num>(&node.v)) {
    //         return value->v;
    //     } else if(const Node::Var* value = std::get_if<Node::Var>(&node.v)) {
    //         auto iter = vars.find(value->name);
    //         if(iter == vars.end())
    //             MAKE_ERROR("Неопознанное состояние");

    //         return iter->second;
    //     } else if(const Node::Unary* value = std::get_if<Node::Unary>(&node.v)) {
    //         int rNodeValue = lambdaCalcNode(value->rhs);
    //         switch(value->op) {
    //         case Op::Not: return !rNodeValue;
    //         case Op::Pos: return +rNodeValue;
    //         case Op::Neg: return -rNodeValue;
    //         default:
    //             std::unreachable();
    //         }
    //     } else if(const Node::Binary* value = std::get_if<Node::Binary>(&node.v)) {
    //         int lNodeValue = lambdaCalcNode(value->lhs);
    //         int rNodeValue = lambdaCalcNode(value->rhs);

    //         switch(value->op) {
    //         case Op::Add: return lNodeValue+rNodeValue;
    //         case Op::Sub: return lNodeValue-rNodeValue;
    //         case Op::Mul: return lNodeValue*rNodeValue;
    //         case Op::Div: return lNodeValue/rNodeValue;
    //         case Op::Mod: return lNodeValue%rNodeValue;
    //         case Op::LT:  return lNodeValue<rNodeValue;
    //         case Op::LE:  return lNodeValue<=rNodeValue;
    //         case Op::GT:  return lNodeValue>rNodeValue;
    //         case Op::GE:  return lNodeValue>=rNodeValue;
    //         case Op::EQ:  return lNodeValue==rNodeValue;
    //         case Op::NE:  return lNodeValue!=rNodeValue;
    //         case Op::And: return lNodeValue&&rNodeValue;
    //         case Op::Or:  return lNodeValue||rNodeValue;
    //         default:
    //             std::unreachable();
    //         }
    //     } else {
    //         std::unreachable();
    //     }
    // };
}

std::pair<float, std::variant<PreparedNodeState::Model, PreparedNodeState::VectorModel>> PreparedNodeState::parseModel(const std::string_view modid, const js::object& obj) {
    // ResourceToLocalId

    bool uvlock;
    float weight = 1;
    std::vector<Transformation> transforms;

    if(const auto weight_val = obj.try_at("weight")) {
        weight = weight_val->to_number<float>();
    }

    if(const auto uvlock_val = obj.try_at("uvlock")) {
        uvlock = uvlock_val->as_bool();
    }

    if(const auto transformations_val = obj.try_at("transformations")) {
        transforms = parseTransormations(transformations_val->as_array());
    }

    const js::value& model = obj.at("model");
    if(const auto model_key = model.try_as_string()) {
        // Одна модель
        Model result;
        result.UVLock = uvlock;
        result.Transforms = std::move(transforms);

        auto [domain, key] = parseDomainKey((std::string) *model_key, modid);
        
        uint16_t resId = 0;
        for(auto& [lDomain, lKey] : ResourceToLocalId) {
            if(lDomain == domain && lKey == key)
                break;

            resId++;
        }

        if(resId == ResourceToLocalId.size()) {
            ResourceToLocalId.emplace_back(domain, key);
        }

        result.Id = resId;

        return {weight, result};
    } else if(model.is_array()) {
        // Множество моделей
        VectorModel result;
        result.UVLock = uvlock;
        result.Transforms = std::move(transforms);

        for(const js::value& js_value : model.as_array()) {
            const js::object& js_obj = js_value.as_object();

            Model subModel;
            if(const auto uvlock_val = js_obj.try_at("uvlock")) {
                subModel.UVLock = uvlock_val->as_bool();
            }

            if(const auto transformations_val = js_obj.try_at("transformations")) {
                subModel.Transforms = parseTransormations(transformations_val->as_array());
            }

            auto [domain, key] = parseDomainKey((std::string) js_obj.at("model").as_string(), modid);
        
            uint16_t resId = 0;
            for(auto& [lDomain, lKey] : ResourceToLocalId) {
                if(lDomain == domain && lKey == key)
                    break;

                resId++;
            }

            if(resId == ResourceToLocalId.size()) {
                ResourceToLocalId.emplace_back(domain, key);
            }

            subModel.Id = resId;
            result.Models.push_back(std::move(subModel));
        }

        return {weight, result};
    } else {
        MAKE_ERROR("");
    }
}

std::vector<PreparedNodeState::Transformation> PreparedNodeState::parseTransormations(const js::array& arr) {
    std::vector<Transformation> result;

    for(const js::value& js_value : arr) {
        const js::string_view transform = js_value.as_string();

        auto pos = transform.find('=');
        std::string_view key = transform.substr(0, pos);
        std::string_view value = transform.substr(pos+1);

        float f_value;
        auto [partial_ptr, partial_ec] = std::from_chars(value.data(), value.data() + value.size(), f_value);

        if(partial_ec == std::errc{} && partial_ptr != value.data() + value.size()) {
            MAKE_ERROR("Converted part of the string: " << value << " (remaining: " << std::string_view(partial_ptr, value.data() + value.size() - partial_ptr) << ")");
        } else if(partial_ec != std::errc{}) {
            MAKE_ERROR("Error converting partial string: " << value);
        }

        if(key == "x")
            result.emplace_back(Transformation::MoveX, f_value);
        else if(key == "y")
            result.emplace_back(Transformation::MoveY, f_value);
        else if(key == "z")
            result.emplace_back(Transformation::MoveZ, f_value);
        else if(key == "rx")
            result.emplace_back(Transformation::RotateX, f_value);
        else if(key == "ry")
            result.emplace_back(Transformation::RotateY, f_value);
        else if(key == "rz")
            result.emplace_back(Transformation::RotateZ, f_value);
        else
            MAKE_ERROR("Неизвестный ключ трансформации");
    }

    return result;
}


PreparedModel::PreparedModel(const std::string_view modid, const js::object& profile) {
    if(profile.contains("parent")) {
        auto [domain, key] = parseDomainKey((const std::string) profile.at("parent").as_string(), modid);
        Parent.emplace(std::move(domain), std::move(key));
    }

    if(profile.contains("gui_light")) {
        std::string_view gui_light = profile.at("gui_light").as_string();

        GuiLight = EnumGuiLight::Default;
    }

    if(profile.contains("AmbientOcclusion")) {
        AmbientOcclusion = profile.at("ambientocclusion").as_bool();
    }

    if(profile.contains("display")) {
        const js::object& list = profile.at("display").as_object();
        for(auto& [key, value] : list) {
            FullTransformation result;

            const js::object& display_value = value.as_object();

            if(boost::system::result<const js::value&> arr_val = display_value.try_at("rotation")) {
                const js::array& arr = arr_val->as_array();
                if(arr.size() != 3)
                    MAKE_ERROR("3");

                for(int iter = 0; iter < 3; iter++)
                    result.Rotation[iter] = arr[iter].to_number<float>();
            }

            if(boost::system::result<const js::value&> arr_val = display_value.try_at("translation")) {
                const js::array& arr = arr_val->as_array();
                if(arr.size() != 3)
                    MAKE_ERROR("3");

                for(int iter = 0; iter < 3; iter++)
                    result.Translation[iter] = arr[iter].to_number<float>();
            }

            if(boost::system::result<const js::value&> arr_val = display_value.try_at("scale")) {
                const js::array& arr = arr_val->as_array();
                if(arr.size() != 3)
                    MAKE_ERROR("3");

                for(int iter = 0; iter < 3; iter++)
                    result.Scale[iter] = arr[iter].to_number<float>();
            }

            Display[key] = result;
        }

    }

    if(boost::system::result<const js::value&> textures_val = profile.try_at("textures")) {
        const js::object& textures = textures_val->as_object();

        for(const auto& [key, value] : textures) {
            auto [domain, key2] = parseDomainKey((const std::string) value.as_string(), modid);
            Textures[key] = {domain, key2};
        }
    }

    if(boost::system::result<const js::value&> cuboids_val = profile.try_at("cuboids")) {
        const js::array& cuboids = cuboids_val->as_array();

        for(const auto& value : cuboids) {
            const js::object& cuboid = value.as_object();

            Cuboid result;

            if(boost::system::result<const js::value&> shade_val = cuboid.try_at("shade")) {
                result.Shade = shade_val->as_bool();
            } else {
                result.Shade = true;
            }

            {
                const js::array& from = cuboid.at("from").as_array();
                if(from.size() != 3)
                    MAKE_ERROR("3");

                for(int iter = 0; iter < 3; iter++)
                    result.From[iter] = from[iter].to_number<float>();
            }

            {
                const js::array& to = cuboid.at("to").as_array();
                if(to.size() != 3)
                    MAKE_ERROR("3");

                for(int iter = 0; iter < 3; iter++)
                    result.To[iter] = to[iter].to_number<float>();
            }

            {
                const js::object& faces = cuboid.at("faces").as_object();

                for(const auto& [key, value] : faces) {
                    Cuboid::EnumFace type;

                    if(key == "down")
                        type = Cuboid::EnumFace::Down;
                    else if(key == "up")
                        type = Cuboid::EnumFace::Up;
                    else if(key == "north")
                        type = Cuboid::EnumFace::North;
                    else if(key == "south")
                        type = Cuboid::EnumFace::South;
                    else if(key == "west")
                        type = Cuboid::EnumFace::West;
                    else if(key == "east")
                        type = Cuboid::EnumFace::East;
                    else
                        MAKE_ERROR("Unknown face");

                    const js::object& js_face = value.as_object();
                    Cuboid::Face face;

                    if(boost::system::result<const js::value&> uv_val = js_face.try_at("uv")) {
                        const js::array& arr = uv_val->as_array();
                        if(arr.size() != 4)
                            MAKE_ERROR(4);

                        for(int iter = 0; iter < 4; iter++)
                            face.UV[iter] = arr[iter].to_number<float>();
                    }

                    if(boost::system::result<const js::value&> texture_val = js_face.try_at("texture")) {
                        face.Texture = texture_val->as_string();
                    }

                    if(boost::system::result<const js::value&> cullface_val = js_face.try_at("cullface")) {
                        const std::string_view cullface = cullface_val->as_string();

                        if(cullface == "down")
                            face.Cullface = Cuboid::EnumFace::Down;
                        else if(cullface == "up")
                            face.Cullface = Cuboid::EnumFace::Up;
                        else if(cullface == "north")
                            face.Cullface = Cuboid::EnumFace::North;
                        else if(cullface == "south")
                            face.Cullface = Cuboid::EnumFace::South;
                        else if(cullface == "west")
                            face.Cullface = Cuboid::EnumFace::West;
                        else if(cullface == "east")
                            face.Cullface = Cuboid::EnumFace::East;
                        else
                            MAKE_ERROR("Unknown face");
                    }

                    if(boost::system::result<const js::value&> tintindex_val = js_face.try_at("tintindex")) {
                        face.TintIndex = tintindex_val->to_number<int>();
                    } else
                        face.TintIndex = -1;

                    if(boost::system::result<const js::value&> rotation_val = js_face.try_at("rotation")) {
                        face.Rotation = rotation_val->to_number<int16_t>();
                    } else
                        face.Rotation = 0;
                    

                    result.Faces[type] = face;
                }
            }

            if(boost::system::result<const js::value&> transformations_val = cuboid.try_at("transformations")) {
                const js::array& transformations = transformations_val->as_array();

                for(const js::value& tobj : transformations) {
                    const js::string_view transform = tobj.as_string();

                    auto pos = transform.find('=');
                    std::string_view key = transform.substr(0, pos);
                    std::string_view value = transform.substr(pos+1);

                    float f_value;
                    auto [partial_ptr, partial_ec] = std::from_chars(value.data(), value.data() + value.size(), f_value);

                    if(partial_ec == std::errc{} && partial_ptr != value.data() + value.size()) {
                        MAKE_ERROR("Converted part of the string: " << value << " (remaining: " << std::string_view(partial_ptr, value.data() + value.size() - partial_ptr) << ")");
                    } else if(partial_ec != std::errc{}) {
                        MAKE_ERROR("Error converting partial string: " << value);
                    }

                    if(key == "x")
                        result.Transformations.emplace_back(Cuboid::Transformation::MoveX, f_value);
                    else if(key == "y")
                        result.Transformations.emplace_back(Cuboid::Transformation::MoveY, f_value);
                    else if(key == "z")
                        result.Transformations.emplace_back(Cuboid::Transformation::MoveZ, f_value);
                    else if(key == "rx")
                        result.Transformations.emplace_back(Cuboid::Transformation::RotateX, f_value);
                    else if(key == "ry")
                        result.Transformations.emplace_back(Cuboid::Transformation::RotateY, f_value);
                    else if(key == "rz")
                        result.Transformations.emplace_back(Cuboid::Transformation::RotateZ, f_value);
                    else
                        MAKE_ERROR("Неизвестный ключ трансформации");
                }
            }
        }
    }

    if(boost::system::result<const js::value&> gltf_val = profile.try_at("gltf")) {
        const js::array& gltf = gltf_val->as_array();

        for(const js::value& sub_val : gltf) {
            SubGLTF result;
            const js::object& sub = sub_val.as_object();
            auto [domain, key] = parseDomainKey((std::string) sub.at("path").as_string(), modid);
            result.Domain = std::move(domain);
            result.Key = std::move(key);

            if(boost::system::result<const js::value&> scene_val = profile.try_at("scene"))
                result.Scene = scene_val->to_number<uint16_t>();
        }
    }
}

PreparedModel::PreparedModel(const std::string_view modid, const sol::table& profile) {
    std::unreachable();
}

PreparedModel::PreparedModel(const std::u8string& data) {
    Net::LinearReader lr(data);

    if(lr.read<uint8_t>()) {
        std::string domain, key;
        lr >> domain >> key;
        Parent.emplace(std::move(domain), std::move(key));
    }

    if(lr.read<uint8_t>()) {
        GuiLight = (EnumGuiLight) lr.read<uint8_t>();
    }

    {
        uint8_t val;
        lr >> val;

        if(val == 2)
            AmbientOcclusion = true;
        else if(val == 1)
            AmbientOcclusion = false;
        else if(val != 0)
            MAKE_ERROR("Ошибка формата данных");
    }

    uint16_t size;
    lr >> size;
    Display.reserve(size);

    for(int counter = 0; counter < size; counter++) {
        std::string key;
        lr >> key;

        FullTransformation tsf;
        for(int iter = 0; iter < 3; iter++)
            lr >> tsf.Rotation[iter];
        
        for(int iter = 0; iter < 3; iter++)
            lr >> tsf.Translation[iter];
        
        for(int iter = 0; iter < 3; iter++)
            lr >> tsf.Scale[iter];

        Display.insert({key, tsf});
    }

    lr >> size;
    Textures.reserve(size);
    for(int counter = 0; counter < size; counter++) {
        std::string tkey, domain, key;
        lr >> tkey >> domain >> key;
        Textures.insert({tkey, {std::move(domain), std::move(key)}});
    }

    lr >> size;
    Cuboids.reserve(size);
    for(int counter = 0; counter < size; counter++) {
        Cuboid cuboid;
        cuboid.Shade = lr.read<uint8_t>();

        for(int iter = 0; iter < 3; iter++)
            lr >> cuboid.From[iter];
        
        for(int iter = 0; iter < 3; iter++)
            lr >> cuboid.To[iter];

        uint16_t facesSize;
        lr >> facesSize;
        cuboid.Faces.reserve(facesSize);

        for(int counter2 = 0; counter2 < facesSize; counter2++) {
            Cuboid::Face face;
            uint8_t type;
            lr >> type;

            for(int iter = 0; iter < 4; iter++)
                lr >> face.UV[iter];

            lr >> face.Texture;
            uint8_t val = lr.read<uint8_t>();
            if(val != uint8_t(-1)) {
                face.Cullface = Cuboid::EnumFace(val);
            }

            lr >> face.TintIndex >> face.Rotation;

            cuboid.Faces.insert({(Cuboid::EnumFace) type, face});
        }

        uint16_t transformationsSize;
        lr >> transformationsSize;
        cuboid.Transformations.reserve(transformationsSize);

        for(int counter2 = 0; counter2 < transformationsSize; counter2++) {
            Cuboid::Transformation tsf;
            tsf.Op = (Cuboid::Transformation::EnumTransform) lr.read<uint8_t>();
            lr >> tsf.Value;
            cuboid.Transformations.emplace_back(tsf);
        }

        Cuboids.emplace_back(std::move(cuboid));
    }

    lr >> size;
    GLTF.reserve(size);
    for(int counter = 0; counter < size; counter++) {
        SubGLTF sub;
        lr >> sub.Domain >> sub.Key;
        uint16_t val = lr.read<uint16_t>();
        if(val != uint16_t(-1)) {
            sub.Scene = val;
        }

        GLTF.push_back(std::move(sub));
    }
}

std::u8string PreparedModel::dump() const {
    Net::Packet result;

    if(Parent.has_value()) {
        result << uint8_t(1);

        assert(Parent->first.size() < 32);
        result << Parent->first;
        
        assert(Parent->second.size() < 32);
        result << Parent->second;
    } else {
        result << uint8_t(0);
    }

    if(GuiLight.has_value()) {
        result << uint8_t(1);
        result << uint8_t(GuiLight.value());
    } else
        result << uint8_t(0);

    if(AmbientOcclusion.has_value()) {
        if(*AmbientOcclusion)
            result << uint8_t(2);
        else
            result << uint8_t(1);
    } else
        result << uint8_t(0);

    assert(Display.size() < (1 << 16));
    result << uint16_t(Display.size());

    for(const auto& [key, tsf] : Display) {
        assert(key.size() < (1 << 16));
        result << key;

        for(int iter = 0; iter < 3; iter++)
            result << tsf.Rotation[iter];

        for(int iter = 0; iter < 3; iter++)
            result << tsf.Translation[iter];

        for(int iter = 0; iter < 3; iter++)
            result << tsf.Scale[iter];
    }

    assert(Textures.size() < (1 << 16));
    result << uint16_t(Textures.size());

    for(const auto& [tkey, dk] : Textures) {
        assert(tkey.size() < 32);
        result << tkey;

        assert(dk.first.size() < 32);
        result << dk.first;

        assert(dk.second.size() < 32);
        result << dk.second;
    }

    assert(Cuboids.size() < (1 << 16));
    result << uint16_t(Cuboids.size());

    for(const Cuboid& cuboid : Cuboids) {
        result << uint8_t(cuboid.Shade);

        for(int iter = 0; iter < 3; iter++)
            result << cuboid.From[iter];

        for(int iter = 0; iter < 3; iter++)
            result << cuboid.To[iter];

        assert(cuboid.Faces.size() < 7);
        result << uint8_t(cuboid.Faces.size());
        for(const auto& [type, face] : cuboid.Faces) {
            result << uint8_t(type);

            for(int iter = 0; iter < 4; iter++)
                result << face.UV[iter];

            result << face.Texture;
            if(face.Cullface)
                result << uint8_t(*face.Cullface);
            else
                result << uint8_t(-1);

            result << face.TintIndex << face.Rotation;
        }

        assert(cuboid.Transformations.size() < 256);
        result << uint8_t(cuboid.Transformations.size());
        for(const auto& [op, value] : cuboid.Transformations) {
            result << uint8_t(op) << value;
        }
    }

    assert(GLTF.size() < 256);
    result << uint8_t(GLTF.size());
    for(const SubGLTF& gltf : GLTF) {
        assert(gltf.Domain.size() < 32);
        assert(gltf.Key.size() < 32);

        result << gltf.Domain << gltf.Key;
        if(gltf.Scene)
            result << uint16_t(*gltf.Scene);
        else
            result << uint16_t(-1);
    }

    return result.complite();
}

}