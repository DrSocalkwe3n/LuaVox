#include "VulkanRenderSession.hpp"

namespace LV::Client::VK {


VulkanRenderSession::VulkanRenderSession(VK::Vulkan *vkInst)
    : VkInst(vkInst),
        MainAtlas(vkInst)
{
    assert(VkInst);
}

VulkanRenderSession::~VulkanRenderSession() {

}

void VulkanRenderSession::onDefTexture(TextureId_c id, std::vector<std::byte> &&info) {

}

void VulkanRenderSession::onDefTextureLost(const std::vector<TextureId_c> &&lost) {

}

void VulkanRenderSession::onDefModel(ModelId_c id, std::vector<std::byte> &&info) {

}

void VulkanRenderSession::onDefModelLost(const std::vector<ModelId_c> &&lost) {

}

void VulkanRenderSession::onDefWorldUpdates(const std::vector<DefWorldId_c> &updates) {

}

void VulkanRenderSession::onDefVoxelUpdates(const std::vector<DefVoxelId_c> &updates) {

}

void VulkanRenderSession::onDefNodeUpdates(const std::vector<DefNodeId_c> &updates) {

}

void VulkanRenderSession::onDefPortalUpdates(const std::vector<DefPortalId_c> &updates) {

}

void VulkanRenderSession::onDefEntityUpdates(const std::vector<DefEntityId_c> &updates) {

}

void VulkanRenderSession::onChunksChange(WorldId_c worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove) {

}

void VulkanRenderSession::setCameraPos(WorldId_c worldId, Pos::Object pos, glm::quat quat) {
    WorldId = worldId;
    Pos = pos;
    Quat = quat;
}

}