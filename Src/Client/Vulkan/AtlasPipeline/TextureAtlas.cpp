#include "TextureAtlas.hpp"

TextureAtlas::TextureAtlas(VkDevice device,
                               VkPhysicalDevice physicalDevice,
                               const Config& cfg,
                               EventCallback cb,
                               std::shared_ptr<SharedStagingBuffer> staging)
    : Device_(device),
      Phys_(physicalDevice),
      Cfg_(cfg),
      OnEvent_(std::move(cb)),
      Staging_(std::move(staging)) {
  if(!Device_ || !Phys_) {
    throw std::runtime_error("TextureAtlas: device/physicalDevice == null");
  }
  _validateConfigOrThrow();

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(Phys_, &props);
  CopyOffsetAlignment_ = std::max<VkDeviceSize>(4, props.limits.optimalBufferCopyOffsetAlignment);

  if(!Staging_) {
    Staging_ = std::make_shared<SharedStagingBuffer>(Device_, Phys_, kStagingSizeBytes);
  }
  _validateStagingCapacityOrThrow();

  _createEntriesBufferOrThrow();
  _createAtlasOrThrow(Cfg_.InitialSide, 1);

  EntriesCpu_.resize(Cfg_.MaxTextureId);
  std::memset(EntriesCpu_.data(), 0, EntriesCpu_.size() * sizeof(Entry));
  _initReservedEntries();
  EntriesDirty_ = true;

  Slots_.resize(Cfg_.MaxTextureId);
  FreeIds_.reserve(Cfg_.MaxTextureId);
  PendingInQueue_.assign(Cfg_.MaxTextureId, false);
  NextId_ = _allocatableStart();

  if(Cfg_.ExternalSampler != VK_NULL_HANDLE) {
    Sampler_ = Cfg_.ExternalSampler;
    OwnsSampler_ = false;
  } else {
    _createSamplerOrThrow();
    OwnsSampler_ = true;
  }

  _rebuildPackersFromPlacements();
  Alive_ = true;
}

TextureAtlas::~TextureAtlas() { _shutdownNoThrow(); }

TextureAtlas::TextureAtlas(TextureAtlas&& other) noexcept {
  _moveFrom(std::move(other));
}

TextureAtlas& TextureAtlas::operator=(TextureAtlas&& other) noexcept {
  if(this != &other) {
    _shutdownNoThrow();
    _moveFrom(std::move(other));
  }
  return *this;
}

void TextureAtlas::shutdown() {
  _ensureAliveOrThrow();
  _shutdownNoThrow();
}

TextureAtlas::TextureId TextureAtlas::registerTexture() {
  _ensureAliveOrThrow();

  TextureId id = kOverflowId;
  if(NextId_ < _allocatableStart()) {
    NextId_ = _allocatableStart();
  }
  while(!FreeIds_.empty() && isReservedId(FreeIds_.back())) {
    FreeIds_.pop_back();
  }
  if(!FreeIds_.empty()) {
    id = FreeIds_.back();
    FreeIds_.pop_back();
  } else if(NextId_ < _allocatableLimit()) {
    id = NextId_++;
  } else {
    return reservedOverflowId();
  }

  Slot& s = Slots_[id];
  s = Slot{};
  s.InUse = true;
  s.StateValue = State::REGISTERED;
  s.Generation = 1;

  _setEntryInvalid(id, /*diagPending*/false, /*diagTooLarge*/false);
  EntriesDirty_ = true;
  return id;
}

void TextureAtlas::setTextureData(TextureId id,
                                    uint32_t w,
                                    uint32_t h,
                                    const void* pixelsRGBA8,
                                    uint32_t rowPitchBytes) {
  _ensureAliveOrThrow();
  if(isInvalidId(id)) return;
  _ensureRegisteredIdOrThrow(id);

  if(w == 0 || h == 0) {
    throw _inputError("setTextureData: w/h must be > 0");
  }
  if(w > Cfg_.MaxTextureSize || h > Cfg_.MaxTextureSize) {
    _handleTooLarge(id);
    throw _inputError("setTextureData: texture is TOO_LARGE (>2048)");
  }
  if(!pixelsRGBA8) {
    throw _inputError("setTextureData: pixelsRGBA8 == null");
  }

  if(rowPitchBytes == 0) {
    rowPitchBytes = w * 4;
  }
  if(rowPitchBytes < w * 4) {
    throw _inputError("setTextureData: rowPitchBytes < w*4");
  }

  Slot& s = Slots_[id];

  const bool sizeChanged = (s.HasCpuData && (s.W != w || s.H != h));
  if(sizeChanged) {
    _freePlacement(id);
    _setEntryInvalid(id, /*diagPending*/true, /*diagTooLarge*/false);
    EntriesDirty_ = true;
  }

  s.W = w;
  s.H = h;

  s.CpuPixels = static_cast<const uint8_t*>(pixelsRGBA8);
  s.CpuRowPitchBytes = rowPitchBytes;
  s.HasCpuData = true;
  s.StateValue = State::PENDING_UPLOAD;
  s.Generation++;

  if(!sizeChanged && s.HasPlacement && s.StateWasValid) {
    // keep entry valid
  } else if(!s.HasPlacement) {
    _setEntryInvalid(id, /*diagPending*/true, /*diagTooLarge*/false);
    EntriesDirty_ = true;
  }

  _enqueuePending(id);

  if(Repack_.Active && Repack_.Plan.count(id) != 0) {
    _enqueueRepackPending(id);
  }
}

void TextureAtlas::clearTextureData(TextureId id) {
  _ensureAliveOrThrow();
  if(isInvalidId(id)) return;
  _ensureRegisteredIdOrThrow(id);

  Slot& s = Slots_[id];
  s.CpuPixels = nullptr;
  s.CpuRowPitchBytes = 0;
  s.HasCpuData = false;

  _freePlacement(id);
  s.StateValue = State::REGISTERED;
  s.StateWasValid = false;

  _removeFromPending(id);
  _removeFromRepackPending(id);

  _setEntryInvalid(id, /*diagPending*/false, /*diagTooLarge*/false);
  EntriesDirty_ = true;
}

void TextureAtlas::removeTexture(TextureId id) {
  _ensureAliveOrThrow();
  if(isInvalidId(id)) return;
  _ensureRegisteredIdOrThrow(id);

  Slot& s = Slots_[id];

  clearTextureData(id);

  s.InUse = false;
  s.StateValue = State::REMOVED;

  FreeIds_.push_back(id);

  _setEntryInvalid(id, /*diagPending*/false, /*diagTooLarge*/false);
  EntriesDirty_ = true;
}

void TextureAtlas::requestFullRepack(RepackMode mode) {
  _ensureAliveOrThrow();
  Repack_.Requested = true;
  Repack_.Mode = mode;
}

TextureAtlas::DescriptorOut TextureAtlas::flushUploadsAndBarriers(VkCommandBuffer cmdBuffer) {
  _ensureAliveOrThrow();
  if(cmdBuffer == VK_NULL_HANDLE) {
    throw _inputError("flushUploadsAndBarriers: cmdBuffer == null");
  }

  if(Repack_.SwapReady) {
    _swapToRepackedAtlas();
  }
  if(Repack_.Requested && !Repack_.Active) {
    _startRepackIfPossible();
  }

  _processPendingLayerGrow(cmdBuffer);

  bool willTouchEntries = EntriesDirty_;

  auto collectQueue = [this](std::deque<TextureId>& queue,
                             std::vector<bool>& inQueue,
                             std::vector<TextureId>& out) {
    while (!queue.empty()) {
      TextureId id = queue.front();
      queue.pop_front();
      if(isInvalidId(id) || id >= inQueue.size()) {
        continue;
      }
      if(!inQueue[id]) {
        continue;
      }
      inQueue[id] = false;
      out.push_back(id);
    }
  };

  std::vector<TextureId> pendingNow;
  pendingNow.reserve(Pending_.size());
  collectQueue(Pending_, PendingInQueue_, pendingNow);

  std::vector<TextureId> repackPending;
  if(Repack_.Active) {
    if(Repack_.InPending.empty()) {
      Repack_.InPending.assign(Cfg_.MaxTextureId, false);
    }
    collectQueue(Repack_.Pending, Repack_.InPending, repackPending);
  }

  auto processPlacement = [&](TextureId id, Slot& s) -> bool {
    if(s.HasPlacement) return true;
    const uint32_t wP = s.W + 2u * Cfg_.PaddingPx;
    const uint32_t hP = s.H + 2u * Cfg_.PaddingPx;
    if(!_tryPlaceWithGrow(id, wP, hP, cmdBuffer)) {
      return false;
    }
    willTouchEntries = true;
    return true;
  };

  bool outOfSpace = false;
  for(TextureId id : pendingNow) {
    if(isInvalidId(id)) continue;
    if(id >= Slots_.size()) continue;
    Slot& s = Slots_[id];
    if(!s.InUse || !s.HasCpuData) continue;
    if(!processPlacement(id, s)) {
      outOfSpace = true;
      _enqueuePending(id);
    }
  }
  if(outOfSpace) {
    _emitEventOncePerFlush(AtlasEvent::AtlasOutOfSpace);
  }

  bool anyAtlasWrites = false;
  bool anyRepackWrites = false;

  auto uploadTextureIntoAtlas = [&](Slot& s,
                                   const Placement& pp,
                                   ImageRes& targetAtlas,
                                   bool isRepackTarget) {
    const uint32_t wP = pp.WP;
    const uint32_t hP = pp.HP;
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(wP) * hP * 4u;
    auto stagingOff = Staging_->Allocate(bytes, CopyOffsetAlignment_);
    if(!stagingOff) {
      _emitEventOncePerFlush(AtlasEvent::StagingOverflow);
      return false;
    }

    uint8_t* dst = static_cast<uint8_t*>(Staging_->Mapped()) + *stagingOff;
    if(!s.CpuPixels) {
      return false;
    }
    _writePaddedRGBA8(dst, wP * 4u, s.W, s.H, Cfg_.PaddingPx,
                      s.CpuPixels, s.CpuRowPitchBytes);

    _ensureImageLayoutForTransferDst(cmdBuffer, targetAtlas,
                                     isRepackTarget ? anyRepackWrites : anyAtlasWrites);

    VkBufferImageCopy region{};
    region.bufferOffset = *stagingOff;
    region.bufferRowLength = wP;
    region.bufferImageHeight = hP;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = pp.Layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { static_cast<int32_t>(pp.X),
                           static_cast<int32_t>(pp.Y), 0 };
    region.imageExtent = { wP, hP, 1 };

    vkCmdCopyBufferToImage(cmdBuffer, Staging_->Buffer(), targetAtlas.Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return true;
  };

  for(TextureId id : pendingNow) {
    if(isInvalidId(id)) continue;
    Slot& s = Slots_[id];
    if(!s.InUse || !s.HasCpuData || !s.HasPlacement) continue;
    if(!uploadTextureIntoAtlas(s, s.Place, Atlas_, false)) {
      _enqueuePending(id);
      continue;
    }
    s.StateValue = State::VALID;
    s.StateWasValid = true;
    _setEntryValid(id);
    EntriesDirty_ = true;
  }

  if(Repack_.Active) {
    for(TextureId id : repackPending) {
      if(Repack_.Plan.count(id) == 0) continue;
      Slot& s = Slots_[id];
      if(!s.InUse || !s.HasCpuData) continue;
      const PlannedPlacement& pp = Repack_.Plan[id];
      Placement place{pp.X, pp.Y, pp.WP, pp.HP, pp.Layer};
      if(!uploadTextureIntoAtlas(s, place, Repack_.Atlas, true)) {
        _enqueueRepackPending(id);
        continue;
      }
      Repack_.WroteSomethingThisFlush = true;
    }
  }

  if(willTouchEntries || EntriesDirty_) {
    const VkDeviceSize entriesBytes = static_cast<VkDeviceSize>(EntriesCpu_.size()) * sizeof(Entry);
    auto off = Staging_->Allocate(entriesBytes, CopyOffsetAlignment_);
    if(!off) {
      _emitEventOncePerFlush(AtlasEvent::StagingOverflow);
    } else {
      std::memcpy(static_cast<uint8_t*>(Staging_->Mapped()) + *off,
                  EntriesCpu_.data(),
                  static_cast<size_t>(entriesBytes));

      VkBufferCopy c{};
      c.srcOffset = *off;
      c.dstOffset = 0;
      c.size = entriesBytes;
      vkCmdCopyBuffer(cmdBuffer, Staging_->Buffer(), Entries_.Buffer, 1, &c);

      VkBufferMemoryBarrier b{};
      b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      b.buffer = Entries_.Buffer;
      b.offset = 0;
      b.size = VK_WHOLE_SIZE;

      vkCmdPipelineBarrier(cmdBuffer,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           0, 0, nullptr, 1, &b, 0, nullptr);
      EntriesDirty_ = false;
    }
  }

  if(anyAtlasWrites) {
    _transitionImage(cmdBuffer, Atlas_,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  } else if(Atlas_.Layout == VK_IMAGE_LAYOUT_UNDEFINED) {
    _transitionImage(cmdBuffer, Atlas_,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     0, VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }

  if(anyRepackWrites) {
    _transitionImage(cmdBuffer, Repack_.Atlas,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }

  if(Repack_.Active) {
    if(Repack_.Pending.empty()) {
      Repack_.WaitingGpuForReady = true;
    }
    Repack_.WroteSomethingThisFlush = false;
  }

  return _buildDescriptorOut();
}

void TextureAtlas::notifyGpuFinished() {
  _ensureAliveOrThrow();

  for(auto& img : DeferredImages_) {
    _destroyImage(img);
  }
  DeferredImages_.clear();

  if(Staging_) {
    Staging_->Reset();
  }
  FlushEventMask_ = 0;

  if(Repack_.Active && Repack_.WaitingGpuForReady && Repack_.Pending.empty()) {
    Repack_.SwapReady = true;
    Repack_.WaitingGpuForReady = false;
  }
}

void TextureAtlas::_moveFrom(TextureAtlas&& other) noexcept {
  Device_ = other.Device_;
  Phys_ = other.Phys_;
  Cfg_ = other.Cfg_;
  OnEvent_ = std::move(other.OnEvent_);
  Alive_ = other.Alive_;
  CopyOffsetAlignment_ = other.CopyOffsetAlignment_;
  Staging_ = std::move(other.Staging_);
  Entries_ = other.Entries_;
  Atlas_ = other.Atlas_;
  Sampler_ = other.Sampler_;
  OwnsSampler_ = other.OwnsSampler_;
  EntriesCpu_ = std::move(other.EntriesCpu_);
  EntriesDirty_ = other.EntriesDirty_;
  Slots_ = std::move(other.Slots_);
  FreeIds_ = std::move(other.FreeIds_);
  NextId_ = other.NextId_;
  Pending_ = std::move(other.Pending_);
  PendingInQueue_ = std::move(other.PendingInQueue_);
  Packers_ = std::move(other.Packers_);
  DeferredImages_ = std::move(other.DeferredImages_);
  FlushEventMask_ = other.FlushEventMask_;
  GrewThisFlush_ = other.GrewThisFlush_;
  Repack_ = std::move(other.Repack_);

  other.Device_ = VK_NULL_HANDLE;
  other.Phys_ = VK_NULL_HANDLE;
  other.OnEvent_ = {};
  other.Alive_ = false;
  other.CopyOffsetAlignment_ = 0;
  other.Staging_.reset();
  other.Entries_ = {};
  other.Atlas_ = {};
  other.Sampler_ = VK_NULL_HANDLE;
  other.OwnsSampler_ = false;
  other.EntriesCpu_.clear();
  other.EntriesDirty_ = false;
  other.Slots_.clear();
  other.FreeIds_.clear();
  other.NextId_ = 0;
  other.Pending_.clear();
  other.PendingInQueue_.clear();
  other.Packers_.clear();
  other.DeferredImages_.clear();
  other.FlushEventMask_ = 0;
  other.GrewThisFlush_ = false;
  other.Repack_ = RepackState{};
}
