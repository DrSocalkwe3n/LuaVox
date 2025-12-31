#pragma once

#include "Vulkan.hpp"
#include "Client/Vulkan/AtlasPipeline/SharedStagingBuffer.hpp"
#include <algorithm>
#include <bitset>
#include <cstring>
#include <memory>
#include <optional>
#include <queue>
#include <vector>
#include <vulkan/vulkan_core.h>


namespace LV::Client::VK {

inline std::weak_ptr<SharedStagingBuffer>& globalVertexStaging() {
    static std::weak_ptr<SharedStagingBuffer> staging;
    return staging;
}

inline std::shared_ptr<SharedStagingBuffer> getOrCreateVertexStaging(Vulkan* inst) {
    auto& staging = globalVertexStaging();
    std::shared_ptr<SharedStagingBuffer> shared = staging.lock();
    if(!shared) {
        shared = std::make_shared<SharedStagingBuffer>(
            inst->Graphics.Device,
            inst->Graphics.PhysicalDevice
        );
        staging = shared;
    }
    return shared;
}

inline void resetVertexStaging() {
    auto& staging = globalVertexStaging();
    if(auto shared = staging.lock())
        shared->Reset();
}

/*
    Память на устройстве выделяется пулами
    Для массивов вершин память выделяется блоками по PerBlock вершин в каждом
    Размер пулла sizeof(Vertex)*PerBlock*PerPool

    Получаемые вершины сначала пишутся в общий буфер, потом передаются на устройство
*/
// Нужна реализация индексного буфера
template<typename Vertex, uint16_t PerBlock = 1 << 10, uint16_t PerPool = 1 << 12, bool IsIndex = false>
class VertexPool {
    static constexpr size_t HC_Buffer_Size = size_t(PerBlock)*size_t(PerPool);

    Vulkan *Inst;

    // Память, доступная для обмена с устройством
    std::shared_ptr<SharedStagingBuffer> Staging;
    VkDeviceSize CopyOffsetAlignment = 4;

    struct Pool {
        // Память на устройстве
        Buffer DeviceBuff;
        // Свободные блоки
        std::bitset<PerPool> Allocation;

        Pool(Vulkan* inst)
            : DeviceBuff(inst, 
                sizeof(Vertex)*size_t(PerBlock)*size_t(PerPool)+4 /* Для vkCmdFillBuffer */, 
                (IsIndex ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            Allocation.set();
        }
    };

    std::vector<Pool> Pools;

    struct Task {
        std::vector<Vertex> Data;
        uint8_t PoolId; // Куда потом направить
        uint16_t BlockId; // И в какой блок
    };

    /*
        Перед следующим обновлением буфер общения заполняется с начала и до конца
        Если место закончится, будет дослано в следующем обновлении
    */
    std::queue<Task> TasksWait, TasksPostponed;


private:
    void pushData(std::vector<Vertex>&& data, uint8_t poolId, uint16_t blockId) {
        TasksWait.push({std::move(data), poolId, blockId});
    }

public:
    VertexPool(Vulkan* inst)
        : Inst(inst)
    {
        Pools.reserve(16);
        Staging = getOrCreateVertexStaging(inst);
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(inst->Graphics.PhysicalDevice, &props);
        CopyOffsetAlignment = std::max<VkDeviceSize>(4, props.limits.optimalBufferCopyOffsetAlignment);
    }

    ~VertexPool() {
    }


    struct Pointer {
        uint32_t PoolId : 8, BlockId : 16, VertexCount = 0;

        operator bool() const { return VertexCount; }
    };

    /*
        Переносит вершины на устройство, заранее передаёт указатель на область в памяти
        Надеемся что к следующему кадру данные будут переданы 
    */
    Pointer pushVertexs(std::vector<Vertex>&& data) {
        if(data.empty())
            return {0, 0, 0};

        // Необходимое количество блоков
        uint16_t blocks = (data.size()+PerBlock-1) / PerBlock;
        assert(blocks <= PerPool);

        // Нужно найти пулл в котором будет свободно blocks количество блоков или создать новый
        for(size_t iterPool = 0; iterPool < Pools.size(); iterPool++) {
            Pool &pool = Pools[iterPool];
            size_t pos = pool.Allocation._Find_first();

            if(pos == PerPool)
                continue;

            while(true) {
                int countEmpty = 1;
                for(size_t pos2 = pos+1; pos2 < PerPool && pool.Allocation.test(pos2) && countEmpty < blocks; pos2++, countEmpty++);

                if(countEmpty == blocks) {
                    for(int block = 0; block < blocks; block++) {
                        pool.Allocation.reset(pos+block);
                    }

                    size_t count = data.size();
                    pushData(std::move(data), iterPool, pos);

                    return Pointer(iterPool, pos, count);
                }

                pos += countEmpty;
                
                if(pos >= PerPool)
                    break;
                
                pos = pool.Allocation._Find_next(pos+countEmpty);
            }
        }

        // Не нашлось подходящего пула, создаём новый
        assert(Pools.size() < 256);

        Pools.emplace_back(Inst);
        Pool &last = Pools.back();
        // vkCmdFillBuffer(nullptr, last.DeviceBuff, 0, last.DeviceBuff.getSize() & ~0x3, 0);
        for(int block = 0; block < blocks; block++)
            last.Allocation.reset(block);

        size_t count = data.size();
        pushData(std::move(data), Pools.size()-1, 0);

        return Pointer(Pools.size()-1, 0, count);
    }

    /*
        Освобождает указатель
    */
    void dropVertexs(const Pointer &pointer) {
        if(!pointer)
            return;

        assert(pointer.PoolId < Pools.size());
        assert(pointer.BlockId < PerPool);

        Pool &pool = Pools[pointer.PoolId];
        int blocks = (pointer.VertexCount+PerBlock-1) / PerBlock;
        for(int indexBlock = 0; indexBlock < blocks; indexBlock++) {
            assert(!pool.Allocation.test(pointer.BlockId+indexBlock));
            pool.Allocation.set(pointer.BlockId+indexBlock);
        }
    }

    void dropVertexs(Pointer &pointer) {
        dropVertexs(const_cast<const Pointer&>(pointer));
        pointer.VertexCount = 0;
    }

    /*
        Перевыделяет память под новые данные
    */
    void relocate(Pointer& pointer, std::vector<Vertex>&& data) {
        if(data.empty()) {
            if(!pointer)
                return;
            else {
                dropVertexs(pointer);
                pointer.VertexCount = 0;
            }
        } else if(!pointer) {
            pointer = pushVertexs(std::move(data));
        } else {
            int needBlocks = (data.size()+PerBlock-1) / PerBlock;

            if((pointer.VertexCount+PerBlock-1) / PerBlock == needBlocks) {
                pointer.VertexCount = data.size();
                pushData(std::move(data), pointer.PoolId, pointer.BlockId);
            } else {
                dropVertexs(pointer);
                pointer = pushVertexs(std::move(data));
            }
        }
    }

    /*
        Транслирует локальный указатель в буффер и позицию вершины в нём
    */
    std::pair<VkBuffer, int> map(const Pointer pointer) {
        assert(pointer.PoolId < Pools.size());
        assert(pointer.BlockId < PerPool);

        return {Pools[pointer.PoolId].DeviceBuff.getBuffer(), pointer.BlockId*PerBlock};
    }

    /*
        Должно вызываться после приёма всех данных, до начала рендера в командном буфере
    */
    void flushUploadsAndBarriers(VkCommandBuffer commandBuffer) {
        if(TasksWait.empty())
            return;

        struct CopyTask {
            VkBuffer DstBuffer;
            VkDeviceSize SrcOffset;
            VkDeviceSize DstOffset;
            VkDeviceSize Size;
            uint8_t PoolId;
        };

        std::vector<CopyTask> copies;
        copies.reserve(TasksWait.size());
        std::vector<uint8_t> touchedPools(Pools.size(), 0);

        while(!TasksWait.empty()) {
            Task task = std::move(TasksWait.front());
            TasksWait.pop();

            VkDeviceSize bytes = task.Data.size()*sizeof(Vertex);
            std::optional<VkDeviceSize> stagingOffset = Staging->Allocate(bytes, CopyOffsetAlignment);
            if(!stagingOffset) {
                TasksPostponed.push(std::move(task));
                while(!TasksWait.empty()) {
                    TasksPostponed.push(std::move(TasksWait.front()));
                    TasksWait.pop();
                }
                break;
            }

            std::memcpy(static_cast<uint8_t*>(Staging->Mapped()) + *stagingOffset,
                task.Data.data(), bytes);

            copies.push_back({
                Pools[task.PoolId].DeviceBuff.getBuffer(),
                *stagingOffset,
                task.BlockId*sizeof(Vertex)*size_t(PerBlock),
                bytes,
                task.PoolId
            });
            touchedPools[task.PoolId] = 1;
        }

        if(copies.empty())
            return;

        VkBufferMemoryBarrier stagingBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_HOST_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            Staging->Buffer(),
            0,
            Staging->Size()
        };

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            1, &stagingBarrier,
            0, nullptr
        );

        for(const CopyTask& copy : copies) {
            VkBufferCopy copyRegion {
                copy.SrcOffset,
                copy.DstOffset,
                copy.Size
            };

            assert(copyRegion.dstOffset+copyRegion.size <= Pools[copy.PoolId].DeviceBuff.getSize());

            vkCmdCopyBuffer(commandBuffer, Staging->Buffer(), copy.DstBuffer, 1, &copyRegion);
        }

        std::vector<VkBufferMemoryBarrier> dstBarriers;
        dstBarriers.reserve(Pools.size());
        for(size_t poolId = 0; poolId < Pools.size(); poolId++) {
            if(!touchedPools[poolId])
                continue;

            VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                IsIndex ? VK_ACCESS_INDEX_READ_BIT : VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                Pools[poolId].DeviceBuff.getBuffer(),
                0,
                Pools[poolId].DeviceBuff.getSize()
            };
            dstBarriers.push_back(barrier);
        }

        if(!dstBarriers.empty()) {
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0,
                0, nullptr,
                static_cast<uint32_t>(dstBarriers.size()),
                dstBarriers.data(),
                0, nullptr
            );
        }
    }

    void notifyGpuFinished() {
        std::queue<Task> postponed = std::move(TasksPostponed);
        while(!postponed.empty()) {
            TasksWait.push(std::move(postponed.front()));
            postponed.pop();
        }
    }
};

template<typename Type, uint16_t PerBlock = 1 << 10, uint16_t PerPool = 1 << 12>
using IndexPool = VertexPool<Type, PerBlock, PerPool, true>;

}
