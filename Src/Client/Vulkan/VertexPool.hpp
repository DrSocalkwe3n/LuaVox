#pragma once

#include "Vulkan.hpp"
#include <bitset>
#include <vulkan/vulkan_core.h>


namespace LV::Client::VK {

/*
    Память на устройстве выделяется пулами
    Для массивов вершин память выделяется блоками по PerBlock вершин в каждом
    Размер пулла sizeof(Vertex)*PerBlock*PerPool

    Получаемые вершины сначала пишутся в общий буфер, потом передаются на устройство
*/
template<typename Vertex, uint16_t PerBlock = 1 << 10, uint16_t PerPool = 1 << 12>
class VertexPool {
    static constexpr size_t HC_Buffer_Size = size_t(PerBlock)*size_t(PerPool);

    Vulkan *Inst;

    // Память, доступная для обмена с устройством
    Buffer HostCoherent;
    Vertex *HCPtr = nullptr;
    size_t WritePos = 0;

    struct Pool {
        // Память на устройстве
        Buffer DeviceBuff;
        // Свободные блоки
        std::bitset<PerPool> Allocation;

        Pool(Vulkan* inst)
            : DeviceBuff(inst, 
                sizeof(Vertex)*size_t(PerBlock)*size_t(PerPool)+4 /* Для vkCmdFillBuffer */, 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            Allocation.set();
        }
    };

    std::vector<Pool> Pools;

    struct Task {
        std::vector<Vertex> Data;
        size_t Pos = -1; // Если данные уже записаны, то будет указана позиция в буфере общения
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
        if(HC_Buffer_Size-WritePos >= data.size()) {
            // Пишем в общий буфер, TasksWait
            Vertex *ptr = HCPtr+WritePos;
            std::copy(data.begin(), data.end(), ptr);
            TasksWait.push({std::move(data), WritePos, poolId, blockId});
            WritePos += data.size();
        } else {
            // Отложим запись на следующий такт
            TasksPostponed.push(Task(std::move(data), -1, poolId, blockId));
        }
    }

public:
    VertexPool(Vulkan* inst)
        : Inst(inst), 
        HostCoherent(inst, 
                sizeof(Vertex)*HC_Buffer_Size+4 /* Для vkCmdFillBuffer */, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    {
        Pools.reserve(16);
        HCPtr = (Vertex*) HostCoherent.mapMemory();
    }

    ~VertexPool() {
        if(HCPtr)
            HostCoherent.unMapMemory();
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
        Должно вызываться после приёма всех данных и перед рендером
    */
    void update(VkCommandPool commandPool) {
        if(TasksWait.empty())
            return;
        
        VkCommandBufferAllocateInfo allocInfo {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            nullptr,
            commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1
        };

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(Inst->Graphics.Device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            nullptr
        };

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        while(!TasksWait.empty()) {
            Task& task = TasksWait.front();

            VkBufferCopy copyRegion {
                task.Pos*sizeof(Vertex),
                task.BlockId*sizeof(Vertex)*size_t(PerBlock),
                task.Data.size()*sizeof(Vertex)
            };

            assert(copyRegion.dstOffset+copyRegion.size < sizeof(Vertex)*PerBlock*PerPool);

            vkCmdCopyBuffer(commandBuffer, HostCoherent.getBuffer(), Pools[task.PoolId].DeviceBuff.getBuffer(),
                 1, &copyRegion);

            TasksWait.pop();
        }

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            0, nullptr,
            nullptr,
            1,
            &commandBuffer,
            0,
            nullptr
        };

        vkQueueSubmit(Inst->Graphics.DeviceQueueGraphic, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(Inst->Graphics.DeviceQueueGraphic);
        vkFreeCommandBuffers(Inst->Graphics.Device, commandPool, 1, &commandBuffer);

        std::queue<Task> postponed = std::move(TasksPostponed);
        WritePos = 0;
            
        while(!postponed.empty()) {
            Task& task = TasksWait.front();
            pushData(std::move(task.Data), task.PoolId, task.BlockId);
            TasksWait.pop();
        }
    }
};


}