#pragma once
#include "Client/Abstract.hpp"
#include <Client/Vulkan/Vulkan.hpp>


namespace LV::Client::VK {

class VulkanRenderSession : public IRenderSession {
    VK::Vulkan *VkInst;
    IServerSession *ServerSession = nullptr;

    WorldId_c WorldId;
    Pos::Object Pos;
    glm::quat Quat;

    VK::AtlasImage MainAtlas;
    std::map<TextureId_c, uint16_t> ServerToAtlas;

public:
    VulkanRenderSession(VK::Vulkan *vkInst);
    virtual ~VulkanRenderSession();

    void setServerSession(IServerSession *serverSession) {
        ServerSession = serverSession;
        assert(serverSession);
    }

    virtual void onDefTexture(TextureId_c id, std::vector<std::byte> &&info) override;
    virtual void onDefTextureLost(const std::vector<TextureId_c> &&lost) override;
    virtual void onDefModel(ModelId_c id, std::vector<std::byte> &&info) override;
    virtual void onDefModelLost(const std::vector<ModelId_c> &&lost) override;

    virtual void onDefWorldUpdates(const std::vector<DefWorldId_c> &updates) override;
    virtual void onDefVoxelUpdates(const std::vector<DefVoxelId_c> &updates) override;
    virtual void onDefNodeUpdates(const std::vector<DefNodeId_c> &updates) override;
    virtual void onDefPortalUpdates(const std::vector<DefPortalId_c> &updates) override;
    virtual void onDefEntityUpdates(const std::vector<DefEntityId_c> &updates) override;

    virtual void onChunksChange(WorldId_c worldId, const std::vector<Pos::GlobalChunk> &changeOrAddList, const std::vector<Pos::GlobalChunk> &remove) override;
    virtual void setCameraPos(WorldId_c worldId, Pos::Object pos, glm::quat quat) override;
};

}