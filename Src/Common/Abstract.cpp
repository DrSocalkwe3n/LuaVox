#include "Abstract.hpp"
#include <algorithm>



namespace LV {


CompressedVoxels compressVoxels_byte(const std::vector<VoxelCube>& voxels) {
    std::u8string compressed;
    std::vector<DefVoxelId_t> defines;
    DefVoxelId_t maxValue = 0;
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
        for(DefVoxelId_t id : defines) {
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

    return {compressed, defines};
}

CompressedVoxels compressVoxels_bit(const std::vector<VoxelCube>& voxels) {
    std::vector<DefVoxelId_t> profile;
    std::vector<DefVoxelId_t> one_byte[7];

    DefVoxelId_t maxValueProfile = 0;
    DefVoxelId_t maxValues[7] = {0};

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

        for(DefNodeId_t id : profile)
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

    return {compressed, profile};
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

        std::vector<DefVoxelId_t> defines;
        defines.resize(read() | (read() << 8));

        for(size_t iter = 0; iter < defines.size(); iter++) {
            DefVoxelId_t id = read();
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

    std::vector<DefVoxelId_t> profile;
    std::vector<DefVoxelId_t> one_byte[7];
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
    if(compressed.front())
        return unCompressVoxels_byte(compressed);
    else
        return unCompressVoxels_bit(compressed);
}



CompressedNodes compressNodes_byte(const Node* nodes) {
    std::u8string compressed;

    std::vector<DefNodeId_t> profiles;

    profiles.reserve(16*16*16);

    compressed.push_back(1);

    DefNodeId_t maxValueProfile = 0;

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
    uint8_t bytes_raw_profile = std::ceil(std::log2(maxValueProfile)/8);
    assert(bytes_raw_profile >= 1 && bytes_raw_profile <= 3);
    // Количество байт на индекс
    uint8_t bytes_indices_profile = std::ceil(std::log2(profiles.size())/8);
    assert(bytes_indices_profile >= 1 && bytes_indices_profile <= 2);

    bool indices_profile = 3+bytes_raw_profile*profiles.size()+bytes_indices_profile*16*16*16 < bytes_raw_profile*16*16*16;

    compressed.push_back(indices_profile | (bytes_raw_profile << 1) | (bytes_indices_profile << 3));
    
    if(indices_profile) {
        // Таблица
        compressed.push_back(profiles.size() & 0xff);
        compressed.push_back((profiles.size() >> 8) & 0xff);
        compressed.push_back((profiles.size() >> 16) & 0xff);

        for(DefNodeId_t id : profiles) {
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

            compressed.push_back(node.NodeId & 0xff);
            if(bytes_raw_profile > 1)
                compressed.push_back((node.NodeId >> 8) & 0xff);
            if(bytes_raw_profile > 2)
                compressed.push_back((node.NodeId >> 8) & 0xff);

            compressed.push_back(node.Meta);
        }
    }

    profiles.shrink_to_fit();
    
    return {compressed, profiles};
}

CompressedNodes compressNodes_bit(const Node* nodes) {
    std::u8string compressed;

    std::vector<DefNodeId_t> profiles;
    std::vector<DefNodeId_t> meta;

    profiles.reserve(16*16*16);
    meta.reserve(16*16*16);

    compressed.push_back(1);

    DefNodeId_t maxValueProfile = 0,
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
    uint8_t bits_raw_profile = std::ceil(std::log2(maxValueProfile));
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
    uint8_t bits_raw_meta = std::ceil(std::log2(maxValueMeta));
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

        for(DefNodeId_t id : profiles) {
            write(id, bits_raw_profile);
        }
    }


    if(indices_meta) {
        write(meta.size(), 8);

        for(DefNodeId_t id : meta) {
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
    
    return {compressed, profiles};
}


CompressedNodes compressNodes(const Node* nodes, bool fast) {
    if(fast)
        return compressNodes_byte(nodes);
    else
        return compressNodes_bit(nodes);
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
        std::vector<DefNodeId_t> profiles;
        profiles.resize(read() | (read() << 8) | (read() << 16));

        for(size_t iter = 0; iter < profiles.size(); iter++) {
            DefNodeId_t id = read();

            if(bytes_raw_profile > 1)
                id |= read() << 8;
            if(bytes_raw_profile > 2)
                id |= read() << 16;
        }

        for(size_t iter = 0; iter < 16*16*16; iter++) {
            Node &node = ptr[iter];

            DefNodeId_t index = read();
            if(bytes_indices_profile > 1)
                index |= read() << 8;

            node.NodeId = profiles[index];
            node.Meta = read();
        }
    } else {
        for(size_t iter = 0; iter < 16*16*16; iter++) {
            Node &node = ptr[iter];

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

    std::vector<DefNodeId_t> meta;


    bool indices_profile = read(1);
    uint8_t bits_raw_profile = read(5);
    uint8_t bits_indices_profile = read(4);

    bool indices_meta = read(1);
    uint8_t bits_raw_meta = read(3);
    uint8_t bits_indices_meta = read(3);

    std::vector<DefNodeId_t> profiles;
    
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
    if(compressed.front())
        return unCompressNodes_byte(compressed, ptr);
    else
        return unCompressNodes_bit(compressed, ptr);
}

}