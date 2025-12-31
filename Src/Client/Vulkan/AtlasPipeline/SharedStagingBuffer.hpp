#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

/*
  Межкадровый промежуточный буфер.
  Для модели рендера Один за одним.
  После окончания рендера кадра считается синхронизированным
  и может заполняться по новой.
*/

class SharedStagingBuffer {
public:
  static constexpr VkDeviceSize kDefaultSize = 64ull * 1024ull * 1024ull;

  SharedStagingBuffer(VkDevice device,
                      VkPhysicalDevice physicalDevice,
                      VkDeviceSize sizeBytes = kDefaultSize)
      : device_(device),
        physicalDevice_(physicalDevice),
        size_(sizeBytes) {
    if (!device_ || !physicalDevice_) {
      throw std::runtime_error("SharedStagingBuffer: null device/physicalDevice");
    }
    if (size_ == 0) {
      throw std::runtime_error("SharedStagingBuffer: size must be > 0");
    }

    VkBufferCreateInfo bi{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size_,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr
    };

    if (vkCreateBuffer(device_, &bi, nullptr, &buffer_) != VK_SUCCESS) {
      throw std::runtime_error("SharedStagingBuffer: vkCreateBuffer failed");
    }

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(device_, buffer_, &mr);

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = FindMemoryType_(mr.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device_, &ai, nullptr, &memory_) != VK_SUCCESS) {
      vkDestroyBuffer(device_, buffer_, nullptr);
      buffer_ = VK_NULL_HANDLE;
      throw std::runtime_error("SharedStagingBuffer: vkAllocateMemory failed");
    }

    vkBindBufferMemory(device_, buffer_, memory_, 0);

    if (vkMapMemory(device_, memory_, 0, VK_WHOLE_SIZE, 0, &mapped_) != VK_SUCCESS) {
      vkFreeMemory(device_, memory_, nullptr);
      vkDestroyBuffer(device_, buffer_, nullptr);
      buffer_ = VK_NULL_HANDLE;
      memory_ = VK_NULL_HANDLE;
      throw std::runtime_error("SharedStagingBuffer: vkMapMemory failed");
    }
  }

  ~SharedStagingBuffer() { Destroy_(); }

  SharedStagingBuffer(const SharedStagingBuffer&) = delete;
  SharedStagingBuffer& operator=(const SharedStagingBuffer&) = delete;

  SharedStagingBuffer(SharedStagingBuffer&& other) noexcept {
    *this = std::move(other);
  }

  SharedStagingBuffer& operator=(SharedStagingBuffer&& other) noexcept {
    if (this != &other) {
      Destroy_();
      device_ = other.device_;
      physicalDevice_ = other.physicalDevice_;
      buffer_ = other.buffer_;
      memory_ = other.memory_;
      mapped_ = other.mapped_;
      size_ = other.size_;
      offset_ = other.offset_;

      other.device_ = VK_NULL_HANDLE;
      other.physicalDevice_ = VK_NULL_HANDLE;
      other.buffer_ = VK_NULL_HANDLE;
      other.memory_ = VK_NULL_HANDLE;
      other.mapped_ = nullptr;
      other.size_ = 0;
      other.offset_ = 0;
    }
    return *this;
  }

  VkBuffer Buffer() const { return buffer_; }
  void* Mapped() const { return mapped_; }
  VkDeviceSize Size() const { return size_; }

  std::optional<VkDeviceSize> Allocate(VkDeviceSize bytes, VkDeviceSize alignment) {
    VkDeviceSize off = Align_(offset_, alignment);
    if (off + bytes > size_) {
      return std::nullopt;
    }
    offset_ = off + bytes;
    return off;
  }

  void Reset() { offset_ = 0; }

private:
  uint32_t FindMemoryType_(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
      if ((typeBits & (1u << i)) &&
          (mp.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }
    throw std::runtime_error("SharedStagingBuffer: no suitable memory type");
  }

  static VkDeviceSize Align_(VkDeviceSize value, VkDeviceSize alignment) {
    if (alignment == 0) return value;
    return (value + alignment - 1) & ~(alignment - 1);
  }

  void Destroy_() {
    if (device_ == VK_NULL_HANDLE) {
      return;
    }
    if (mapped_) {
      vkUnmapMemory(device_, memory_);
      mapped_ = nullptr;
    }
    if (buffer_) {
      vkDestroyBuffer(device_, buffer_, nullptr);
      buffer_ = VK_NULL_HANDLE;
    }
    if (memory_) {
      vkFreeMemory(device_, memory_, nullptr);
      memory_ = VK_NULL_HANDLE;
    }
    size_ = 0;
    offset_ = 0;
    device_ = VK_NULL_HANDLE;
    physicalDevice_ = VK_NULL_HANDLE;
  }

  VkDevice device_ = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  void* mapped_ = nullptr;
  VkDeviceSize size_ = 0;
  VkDeviceSize offset_ = 0;
};
