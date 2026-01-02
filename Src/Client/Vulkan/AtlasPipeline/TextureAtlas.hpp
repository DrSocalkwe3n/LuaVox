// TextureAtlas.hpp
#pragma once

/*
================================================================================
TextureAtlas — как пользоваться (кратко)

1) Создайте атлас (один раз):
   TextureAtlas atlas(device, physicalDevice, cfg, callback);

2) Зарегистрируйте текстуру и получите стабильный ID:
   TextureId id = atlas.registerTexture();
   if(id == TextureAtlas::kOverflowId || id == atlas.reservedOverflowId()) { ... } // нет свободных ID

3) Задайте данные (RGBA8), можно много раз — ID не меняется:
   atlas.setTextureData(id, w, h, pixels, rowPitchBytes);

4) Каждый кадр (или когда нужно) запишите команды в cmdBuffer:
   auto desc = atlas.flushUploadsAndBarriers(cmdBuffer);

   - desc.ImageInfo (sampler+view) используйте как sampled image (2D array).
   - desc.EntriesInfo (SSBO) используйте как storage buffer с Entry[].
   - В шейдере ОБЯЗАТЕЛЬНО делайте:
       uv = clamp(uv, entry.uvMinMax.xy, entry.uvMinMax.zw);

5) Затем пользователь САМ делает submit и ждёт завершения GPU (fence вне ТЗ).
   После того как GPU точно закончил команды Flush — вызовите:
   atlas.notifyGpuFinished();

6) Удаление:
   atlas.clearTextureData(id);  // убрать данные + освободить место + REGISTERED
   atlas.removeTexture(id);     // освободить ID (после этого использовать нельзя)

Примечания:
- Вызовы API с kOverflowId и зарезервированными ID игнорируются (no-op).
- ID из начала диапазона зарезервированы под служебные нужды:
    reservedOverflowId() == 0,
    reservedLayerId(0) == 1 (первый слой),
    reservedLayerId(1) == 2 (второй слой) и т.д.
- Ошибки ресурсов (нет места/стейджинга/oom) НЕ бросают исключения — дают события.
- Исключения только за неверный ввод/неверное использование (см. ТЗ).
- Класс не thread-safe: синхронизацию обеспечивает пользователь.
================================================================================
*/

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <MaxRectsBinPack.h>

#include "SharedStagingBuffer.hpp"

class TextureAtlas final {
public:
  using TextureId = uint32_t;
  static constexpr TextureId kOverflowId = 0xFFFFFFFFu;

  // ----------------------------- Конфигурация -----------------------------

  struct Config {
    uint32_t MaxTextureId = 4096;        // Размер SSBO: MaxTextureId * sizeof(Entry)
    uint32_t InitialSide = 1024;         // {1024, 2048, 4096}
    uint32_t MaxLayers = 16;             // <= 16
    uint32_t PaddingPx = 2;              // фиксированный padding (edge-extend)
    uint32_t MaxTextureSize = 2048;      // w,h <= 2048
    VkFilter SamplerFilter = VK_FILTER_LINEAR;
    bool SamplerAnisotropyEnable = false;

    // Если хотите — можно задать внешний sampler (тогда класс его НЕ уничтожает)
    VkSampler ExternalSampler = VK_NULL_HANDLE;
  };

  // ----------------------------- События -----------------------------

  enum class AtlasEvent {
    StagingOverflow,
    AtlasOutOfSpace,
    GpuOutOfMemory,
    RepackStarted,
    RepackFinished
  };

  using EventCallback = std::function<void(AtlasEvent)>;

  // ----------------------------- Shader entry -----------------------------
  // Важно: layout должен соответствовать std430 на стороне шейдера.
  struct alignas(16) Entry {
    // (uMin, vMin, uMax, vMax)
    float UVMinMax[4];
    uint32_t Layer;
    uint32_t Flags;
    uint32_t _Pad0;
    uint32_t _Pad1;
  };
  static_assert(sizeof(Entry) % 16 == 0, "Entry должен быть кратен 16 байтам");

  enum EntryFlags : uint32_t {
    ENTRY_VALID = 1u << 0,
    // опционально под диагностику:
    ENTRY_DIAG_PENDING = 1u << 1,
    ENTRY_DIAG_TOO_LARGE = 1u << 2
  };

  // ----------------------------- Выход для дескрипторов -----------------------------

  struct DescriptorOut {
    VkImage AtlasImage = VK_NULL_HANDLE;
    VkImageView AtlasView = VK_NULL_HANDLE;
    VkSampler Sampler = VK_NULL_HANDLE;

    VkBuffer EntriesBuffer = VK_NULL_HANDLE;

    VkDescriptorImageInfo ImageInfo{};
    VkDescriptorBufferInfo EntriesInfo{};

    uint32_t AtlasSide = 0;
    uint32_t AtlasLayers = 0;
  };

  // ----------------------------- Full repack -----------------------------

  enum class RepackMode {
    Tightest,
    KeepCurrentCapacity,
    AllowGrow
  };

  // ----------------------------- Жизненный цикл -----------------------------

  /// Создаёт GPU-ресурсы: atlas image, entries SSBO, staging ring (64MB, если внешняя staging не передана), sampler.
  TextureAtlas(VkDevice device,
                 VkPhysicalDevice physicalDevice,
                 const Config& cfg,
                 EventCallback cb = {},
                 std::shared_ptr<SharedStagingBuffer> staging = {});
  TextureAtlas(const TextureAtlas&) = delete;
  TextureAtlas& operator=(const TextureAtlas&) = delete;
  TextureAtlas(TextureAtlas&& other) noexcept;
  TextureAtlas& operator=(TextureAtlas&& other) noexcept;

  /// Уничтожает ресурсы (если не уничтожены раньше).
  ~TextureAtlas();

  /// Явно освобождает все ресурсы. После этого любые вызовы (кроме деструктора) — ошибка.
  void shutdown();

  // ----------------------------- API из ТЗ -----------------------------

  /// Регистрирует новую текстуру и возвращает стабильный TextureId (или kOverflowId).
  TextureId registerTexture();

  /// Устанавливает пиксели (RGBA8), переводит в PENDING_UPLOAD, при смене размера освобождает размещение.
  /// Внимание: pixelsRGBA8 должен оставаться валидным, пока текстура не будет очищена или заменена.
  void setTextureData(TextureId id,
                      uint32_t w,
                      uint32_t h,
                      const void* pixelsRGBA8,
                      uint32_t rowPitchBytes);

  /// Сбрасывает данные, освобождает размещение, переводит в REGISTERED.
  void clearTextureData(TextureId id);

  /// Удаляет текстуру: освобождает размещение, удаляет данные, удаляет pending, возвращает ID в пул.
  void removeTexture(TextureId id);

  /// Запрашивает полный репак (выполняется по частям через Flush, с событиями).
  void requestFullRepack(RepackMode mode);

  // ----------------------------- Flush/Notify из ТЗ -----------------------------

  /// Записывает команды Vulkan: рост/репак/копии pending/обновление entries/барьеры. Submit делает пользователь.
  DescriptorOut flushUploadsAndBarriers(VkCommandBuffer cmdBuffer);

  /// Пользователь вызывает ПОСЛЕ завершения GPU-команд Flush: освобождает deferred ресурсы, сбрасывает staging и т.п.
  void notifyGpuFinished();

// ----------------------------- Дополнительно -----------------------------

  /// Текущая сторона атласа.
  uint32_t atlasSide() const { return Atlas_.Side; }

  /// Текущее число слоёв атласа.
  uint32_t atlasLayers() const { return Atlas_.Layers; }

  uint32_t maxLayers() const { return Cfg_.MaxLayers; }
  uint32_t maxTextureId() const { return Cfg_.MaxTextureId; }

  TextureId reservedOverflowId() const { return 0; }
  TextureId reservedLayerId(uint32_t layer) const { return 1u + layer; }

  bool isReservedId(TextureId id) const {
    if(id >= Cfg_.MaxTextureId) return false;
    return id < _reservedCount();
  }

  bool isReservedLayerId(TextureId id) const {
    if(id >= Cfg_.MaxTextureId) return false;
    return id != reservedOverflowId() && id < _reservedCount();
  }

  bool isInvalidId(TextureId id) const {
    return id == kOverflowId || isReservedId(id);
  }

  void requestLayerCount(uint32_t layers) {
    _ensureAliveOrThrow();
    _scheduleLayerGrow(layers);
  }

  /// Общий staging-буфер (может быть задан извне).
  std::shared_ptr<SharedStagingBuffer> getStagingBuffer() const { return Staging_; }

private:
  void _moveFrom(TextureAtlas&& other) noexcept;
  uint32_t _reservedCount() const { return Cfg_.MaxLayers + 1; }
  uint32_t _reservedStart() const { return 0; }
  uint32_t _allocatableStart() const { return _reservedCount(); }
  uint32_t _allocatableLimit() const { return Cfg_.MaxTextureId; }

  void _initReservedEntries() {
    if(Cfg_.MaxTextureId <= _reservedCount()) {
      return;
    }

    _setEntryInvalid(reservedOverflowId(), /*diagPending*/false, /*diagTooLarge*/false);
    for(uint32_t layer = 0; layer < Cfg_.MaxLayers; ++layer) {
      TextureId id = reservedLayerId(layer);
      Entry& e = EntriesCpu_[id];
      e.UVMinMax[0] = 0.0f;
      e.UVMinMax[1] = 0.0f;
      e.UVMinMax[2] = 1.0f;
      e.UVMinMax[3] = 1.0f;
      e.Layer = layer;
      e.Flags = ENTRY_VALID;
    }
    EntriesDirty_ = true;
  }

  // ============================= Ошибки/валидация =============================

  struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  static InputError _inputError(const std::string& msg) { return InputError(msg); }

  void _validateConfigOrThrow() {
    auto isPow2Allowed = [](uint32_t s) {
      return s == 1024u || s == 2048u || s == 4096u;
    };

    if(!isPow2Allowed(Cfg_.InitialSide)) {
      throw _inputError("Config.InitialSide must be 1024/2048/4096");
    }

    if(Cfg_.MaxLayers == 0 || Cfg_.MaxLayers > 16) {
      throw _inputError("Config.MaxLayers must be 1..16");
    }

    if(Cfg_.PaddingPx > 64) {
      throw _inputError("Config.PaddingPx looks too large");
    }

    if(Cfg_.MaxTextureId == 0) {
      throw _inputError("Config.MaxTextureId must be > 0");
    }
    if(Cfg_.MaxTextureId <= (Cfg_.MaxLayers + 1)) {
      throw _inputError("Config.MaxTextureId must be > MaxLayers + 1 (reserved ids)");
    }

    if(Cfg_.MaxTextureSize != 2048) {
      /// TODO: 
    }
  }

  void _validateStagingCapacityOrThrow() const {
    if(!Staging_) {
      throw std::runtime_error("TextureAtlas: staging buffer not initialized");
    }
    const VkDeviceSize entriesBytes = static_cast<VkDeviceSize>(Cfg_.MaxTextureId) * sizeof(Entry);
    if(entriesBytes > Staging_->Size()) {
      throw _inputError("Config.MaxTextureId слишком большой: entries не влезают в staging buffer");
    }
  }

  void _ensureAliveOrThrow() const {
    if(!Alive_) throw _inputError("TextureAtlas: used after shutdown");
  }

  void _ensureRegisteredIdOrThrow(TextureId id) const {
    if(id >= Cfg_.MaxTextureId || isReservedId(id)) {
      throw _inputError("TextureId out of range");
    }
    if(!Slots_[id].InUse || Slots_[id].StateValue == State::REMOVED) {
      throw _inputError("Using unregistered or removed TextureId");
    }
  }

  // ============================= Состояния/слоты =============================

  enum class State {
    REGISTERED,
    PENDING_UPLOAD,
    VALID,
    NOT_LOADED_TOO_LARGE,
    REMOVED
  };

  struct Placement {
    uint32_t X = 0, Y = 0;
    uint32_t WP = 0, HP = 0;
    uint32_t Layer = 0;
  };

  struct Slot {
    bool InUse = false;

    State StateValue = State::REMOVED;
    bool HasCpuData = false;
    bool TooLarge = false;

    uint32_t W = 0, H = 0;
    const uint8_t* CpuPixels = nullptr;
    uint32_t CpuRowPitchBytes = 0;

    bool HasPlacement = false;
    Placement Place{};

    bool StateWasValid = false;
    uint64_t Generation = 0;
  };

  // ============================= Vulkan ресурсы =============================

  static constexpr VkDeviceSize kStagingSizeBytes = 64ull * 1024ull * 1024ull;

  struct BufferRes {
    VkBuffer Buffer = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkDeviceSize Size = 0;
  };

  struct ImageRes {
    VkImage Image = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkImageView View = VK_NULL_HANDLE;
    VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t Side = 0;
    uint32_t Layers = 0;
  };

  // ============================= MaxRects packer =============================

  struct Rect {
    uint32_t X = 0, Y = 0, W = 0, H = 0;
  };

  struct MaxRectsBin {
    uint32_t Width = 0;
    uint32_t Height = 0;
    std::vector<Rect> FreeRects;

    void reset(uint32_t w, uint32_t h) {
      Width = w; Height = h;
      FreeRects.clear();
      FreeRects.push_back(Rect{0,0,w,h});
    }

    static bool _contains(const Rect& a, const Rect& b) {
      return b.X >= a.X && b.Y >= a.Y &&
             (b.X + b.W) <= (a.X + a.W) &&
             (b.Y + b.H) <= (a.Y + a.H);
    }

    static bool _intersects(const Rect& a, const Rect& b) {
      return !(b.X >= a.X + a.W || b.X + b.W <= a.X ||
               b.Y >= a.Y + a.H || b.Y + b.H <= a.Y);
    }

    void _prune() {
      // удаляем прямоугольники, которые содержатся в других
      for(size_t i = 0; i < FreeRects.size(); ++i) {
        for(size_t j = i + 1; j < FreeRects.size();) {
          if(_contains(FreeRects[i], FreeRects[j])) {
            FreeRects.erase(FreeRects.begin() + j);
          } else if(_contains(FreeRects[j], FreeRects[i])) {
            FreeRects.erase(FreeRects.begin() + i);
            --i;
            break;
          } else {
            ++j;
          }
        }
      }
    }

    std::optional<Rect> insert(uint32_t w, uint32_t h) {
      // Best Area Fit
      size_t bestIdx = std::numeric_limits<size_t>::max();
      uint64_t bestAreaWaste = std::numeric_limits<uint64_t>::max();
      uint32_t bestShortSide = std::numeric_limits<uint32_t>::max();

      for(size_t i = 0; i < FreeRects.size(); ++i) {
        const Rect& r = FreeRects[i];
        if(w <= r.W && h <= r.H) {
          const uint64_t waste = static_cast<uint64_t>(r.W) * r.H - static_cast<uint64_t>(w) * h;
          const uint32_t shortSide = std::min(r.W - w, r.H - h);
          if(waste < bestAreaWaste || (waste == bestAreaWaste && shortSide < bestShortSide)) {
            bestAreaWaste = waste;
            bestShortSide = shortSide;
            bestIdx = i;
          }
        }
      }

      if(bestIdx == std::numeric_limits<size_t>::max()) {
        return std::nullopt;
      }

      Rect placed{ FreeRects[bestIdx].X, FreeRects[bestIdx].Y, w, h };
      _splitFree(placed);
      _prune();
      return placed;
    }

    void _splitFree(const Rect& used) {
      std::vector<Rect> newFree;
      newFree.reserve(FreeRects.size() * 2);

      for(const Rect& fr : FreeRects) {
        if(!_intersects(fr, used)) {
          newFree.push_back(fr);
          continue;
        }

        // сверху
        if(used.Y > fr.Y) {
          newFree.push_back(Rect{ fr.X, fr.Y, fr.W, used.Y - fr.Y });
        }
        // снизу
        if(used.Y + used.H < fr.Y + fr.H) {
          newFree.push_back(Rect{ fr.X, used.Y + used.H, fr.W,
                                  (fr.Y + fr.H) - (used.Y + used.H) });
        }
        // слева
        if(used.X > fr.X) {
          const uint32_t x = fr.X;
          const uint32_t y = std::max(fr.Y, used.Y);
          const uint32_t h = std::min(fr.Y + fr.H, used.Y + used.H) - y;
          newFree.push_back(Rect{ x, y, used.X - fr.X, h });
        }
        // справа
        if(used.X + used.W < fr.X + fr.W) {
          const uint32_t x = used.X + used.W;
          const uint32_t y = std::max(fr.Y, used.Y);
          const uint32_t h = std::min(fr.Y + fr.H, used.Y + used.H) - y;
          newFree.push_back(Rect{ x, y, (fr.X + fr.W) - (used.X + used.W), h });
        }
      }

      // удаляем нулевые
      FreeRects.clear();
      FreeRects.reserve(newFree.size());
      for(const Rect& r : newFree) {
        if(r.W > 0 && r.H > 0)
          FreeRects.push_back(r);
      }
    }

    void free(const Rect& r) {
      FreeRects.push_back(r);
      _mergeAdjacent();
      _prune();
    }

    void _mergeAdjacent() {
      bool merged = true;
      while (merged) {
        merged = false;
        for(size_t i = 0; i < FreeRects.size() && !merged; ++i) {
          for(size_t j = i + 1; j < FreeRects.size(); ++j) {
            Rect a = FreeRects[i];
            Rect b = FreeRects[j];

            // vertical merge
            if(a.X == b.X && a.W == b.W) {
              if(a.Y + a.H == b.Y) {
                FreeRects[i] = Rect{ a.X, a.Y, a.W, a.H + b.H };
                FreeRects.erase(FreeRects.begin() + j);
                merged = true;
                break;
              } else if(b.Y + b.H == a.Y) {
                FreeRects[i] = Rect{ b.X, b.Y, b.W, b.H + a.H };
                FreeRects.erase(FreeRects.begin() + j);
                merged = true;
                break;
              }
            }

            // horizontal merge
            if(a.Y == b.Y && a.H == b.H) {
              if(a.X + a.W == b.X) {
                FreeRects[i] = Rect{ a.X, a.Y, a.W + b.W, a.H };
                FreeRects.erase(FreeRects.begin() + j);
                merged = true;
                break;
              } else if(b.X + b.W == a.X) {
                FreeRects[i] = Rect{ b.X, b.Y, b.W + a.W, b.H };
                FreeRects.erase(FreeRects.begin() + j);
                merged = true;
                break;
              }
            }
          }
        }
      }
    }
  };

  // ============================= Repack state =============================

  struct PlannedPlacement {
    uint32_t X = 0, Y = 0;
    uint32_t WP = 0, HP = 0;
    uint32_t Layer = 0;
  };

  struct RepackState {
    bool Requested = false;
    bool Active = false;
    bool SwapReady = false;
    bool WaitingGpuForReady = false;

    RepackMode Mode = RepackMode::Tightest;

    ImageRes Atlas{}; // новый атлас (пока не активный)

    std::unordered_map<TextureId, PlannedPlacement> Plan;

    std::deque<TextureId> Pending;           // что ещё нужно залить/перезалить
    std::vector<bool> InPending;             // чтобы не дублировать
    bool WroteSomethingThisFlush = false;
  };

  // ============================= Внутренняя логика =============================

  void _emitEventOncePerFlush(AtlasEvent e) {
    const uint32_t bit = 1u << static_cast<uint32_t>(e);
    if((FlushEventMask_ & bit) != 0)
      return;
    FlushEventMask_ |= bit;
    if(OnEvent_)
      OnEvent_(e);
  }

  DescriptorOut _buildDescriptorOut() const {
    DescriptorOut out{};
    out.AtlasImage = Atlas_.Image;
    out.AtlasView = Atlas_.View;
    out.Sampler = Sampler_;
    out.EntriesBuffer = Entries_.Buffer;
    out.AtlasSide = Atlas_.Side;
    out.AtlasLayers = Atlas_.Layers;

    out.ImageInfo.sampler = Sampler_;
    out.ImageInfo.imageView = Atlas_.View;
    out.ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    out.EntriesInfo.buffer = Entries_.Buffer;
    out.EntriesInfo.offset = 0;
    out.EntriesInfo.range = VK_WHOLE_SIZE;
    return out;
  }

  void _handleTooLarge(TextureId id) {
    Slot& s = Slots_[id];
    if(s.StateValue == State::VALID && s.HasPlacement) {
      _freePlacement(id);
    } else {
      _freePlacement(id);
    }

    s.CpuPixels = nullptr;
    s.CpuRowPitchBytes = 0;
    s.HasCpuData = false;

    s.TooLarge = true;
    s.StateValue = State::NOT_LOADED_TOO_LARGE;
    s.StateWasValid = false;

    _removeFromPending(id);
    _removeFromRepackPending(id);

    _setEntryInvalid(id, /*diagPending*/false, /*diagTooLarge*/true);
    EntriesDirty_ = true;
  }

  void _enqueuePending(TextureId id) {
    Slot& s = Slots_[id];
    if(!PendingInQueue_[id]) {
      Pending_.push_back(id);
      PendingInQueue_[id] = true;
    }
  }

  void _removeFromPending(TextureId id) {
    if(id >= PendingInQueue_.size() || !PendingInQueue_[id])
      return;
    PendingInQueue_[id] = false;
    // ленивое удаление из Pending_ делаем в Flush (чтобы не O(n) на каждый вызов)
  }

  void _enqueueRepackPending(TextureId id) {
    if(!Repack_.Active)
      return;

    if(id >= Repack_.InPending.size())
      return;

    if(!Repack_.InPending[id]) {
      Repack_.Pending.push_back(id);
      Repack_.InPending[id] = true;
    }
  }

  void _removeFromRepackPending(TextureId id) {
    if(!Repack_.Active)
      return;
    if(id >= Repack_.InPending.size())
      return;
    Repack_.InPending[id] = false;
    // очередь Repack_.Pending лениво отфильтруется в Flush
  }

  void _setEntryInvalid(TextureId id, bool diagPending, bool diagTooLarge) {
    Entry& e = EntriesCpu_[id];
    e.UVMinMax[0] = e.UVMinMax[1] = e.UVMinMax[2] = e.UVMinMax[3] = 0.0f;
    e.Layer = 0;
    e.Flags = 0;
    if(diagPending)
      e.Flags |= ENTRY_DIAG_PENDING;
    if(diagTooLarge)
      e.Flags |= ENTRY_DIAG_TOO_LARGE;
  }

  void _setEntryValid(TextureId id) {
    Slot& s = Slots_[id];
    if(!s.HasPlacement) {
      _setEntryInvalid(id, /*diagPending*/true, /*diagTooLarge*/false);
      return;
    }

    const float S = static_cast<float>(Atlas_.Side);
    const float x = static_cast<float>(s.Place.X);
    const float y = static_cast<float>(s.Place.Y);
    const float wP = static_cast<float>(s.Place.WP);
    const float hP = static_cast<float>(s.Place.HP);

    Entry& e = EntriesCpu_[id];
    e.UVMinMax[0] = (x + 0.5f) / S;
    e.UVMinMax[1] = (y + 0.5f) / S;
    e.UVMinMax[2] = (x + wP - 0.5f) / S;
    e.UVMinMax[3] = (y + hP - 0.5f) / S;
    e.Layer = s.Place.Layer;
    e.Flags = ENTRY_VALID;
  }

  void _scheduleLayerGrow(uint32_t targetLayers) {
    if(targetLayers > Cfg_.MaxLayers) {
      targetLayers = Cfg_.MaxLayers;
    }
    PendingLayerGrow_ = std::max(PendingLayerGrow_, targetLayers);
  }

  void _processPendingLayerGrow(VkCommandBuffer cmdBuffer) {
    if(PendingLayerGrow_ == 0)
      return;
    if(PendingLayerGrow_ <= Atlas_.Layers) {
      PendingLayerGrow_ = 0;
      return;
    }
    if(cmdBuffer == VK_NULL_HANDLE)
      return;
    if(_tryGrowAtlas(Atlas_.Side, PendingLayerGrow_, cmdBuffer)) {
      PendingLayerGrow_ = 0;
    }
  }

  // ============================= Размещение/packer =============================

  bool _tryPlaceWithGrow(TextureId id, uint32_t wP, uint32_t hP, VkCommandBuffer cmdBuffer) {
    // 1) текущие слои
    if(_tryPlaceInExistingLayers(id, wP, hP)) return true;

    // 2) добавляем новые слои до тех пор, пока есть лимит
    while (Atlas_.Layers < Cfg_.MaxLayers) {
      if(!_tryAddLayer(cmdBuffer)) {
        // рост слоя не удался (например, OOM) — дальше увеличивать сторону нельзя
        return false;
      }
      if(_tryPlaceInExistingLayers(id, wP, hP)) return true;
    }

    // 3) увеличить размер атласа 1024→2048→4096 (только когда достигли лимита по слоям)
    if(Atlas_.Layers >= Cfg_.MaxLayers) {
      const uint32_t nextSide = _nextAllowedSide(Atlas_.Side);
      if(nextSide != Atlas_.Side) {
        if(_tryGrowAtlas(nextSide, Atlas_.Layers, cmdBuffer)) {
          if(_tryPlaceInExistingLayers(id, wP, hP)) return true;
        } else {
          // OOM — событие уже отправили
          return false;
        }
      }
    }

    // 4) невозможно
    return false;
  }

  bool _tryPlaceInExistingLayers(TextureId id, uint32_t wP, uint32_t hP) {
    for(uint32_t layer = 0; layer < Atlas_.Layers; ++layer) {
      auto placed = Packers_[layer].insert(wP, hP);
      if(placed.has_value()) {
        Slot& s = Slots_[id];
        s.HasPlacement = true;
        s.Place = Placement{ placed->X, placed->Y, wP, hP, layer };
        // entry пока не VALID (станет VALID после копии), но UV уже можно пересчитать
        return true;
      }
    }
    return false;
  }

  bool _tryAddLayer(VkCommandBuffer cmdBuffer) {
    if(Atlas_.Layers >= Cfg_.MaxLayers) return false;
    const uint32_t newLayers = Atlas_.Layers + 1;
    if(cmdBuffer == VK_NULL_HANDLE) {
      _scheduleLayerGrow(newLayers);
      return false;
    }
    return _tryGrowAtlas(Atlas_.Side, newLayers, cmdBuffer);
  }

  uint32_t _nextAllowedSide(uint32_t s) const {
    if(s <= 1024u) return 2048u;
    if(s <= 2048u) return 4096u;
    return s;
  }

  bool _tryGrowAtlas(uint32_t newSide, uint32_t newLayers, VkCommandBuffer cmdBuffer) {
    // Создаём новый image+view
    ImageRes newAtlas{};
    if(!_createAtlasNoThrow(newSide, newLayers, newAtlas)) {
      _emitEventOncePerFlush(AtlasEvent::GpuOutOfMemory);
      return false;
    }

    // Если cmdBuffer == null — не можем записать copy сейчас.
    // Поэтому этот путь (рост layers) предполагает, что пользователь вызовет Flush,
    // и мы записываем копию только когда cmdBuffer валиден.
    // Чтобы не усложнять, если cmdBuffer == null — считаем, что нет роста (fail).
    if(cmdBuffer == VK_NULL_HANDLE) {
      _destroyImage(newAtlas);
      return false;
    }

    // Переводим layouts и копируем старый атлас в новый
    _transitionImage(cmdBuffer, Atlas_,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     (Atlas_.Layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT),
                     VK_ACCESS_TRANSFER_READ_BIT,
                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT);

    _transitionImage(cmdBuffer, newAtlas,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     0,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT);

    const uint32_t layersToCopy = std::min(Atlas_.Layers, newAtlas.Layers);
    VkImageCopy copy{};
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.mipLevel = 0;
    copy.srcSubresource.baseArrayLayer = 0;
    copy.srcSubresource.layerCount = layersToCopy;
    copy.dstSubresource = copy.srcSubresource;
    copy.extent = { Atlas_.Side, Atlas_.Side, 1 };

    vkCmdCopyImage(cmdBuffer,
                   Atlas_.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   newAtlas.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copy);

    // Новый делаем читаемым (активным станет сразу после Flush)
    _transitionImage(cmdBuffer, newAtlas,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // Старый вернём в читаемый тоже (на всякий случай)
    _transitionImage(cmdBuffer, Atlas_,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_READ_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // deferred destroy старого (после Notify)
    DeferredImages_.push_back(Atlas_);

    // переключаемся на новый
    Atlas_ = newAtlas;

    // После роста пересчитываем entries для всех VALID (S изменился)
    for(TextureId id = 0; id < Cfg_.MaxTextureId; ++id) {
      Slot& s = Slots_[id];
      if(!s.InUse)
        continue;

      if(s.HasPlacement && s.StateValue == State::VALID) {
        _setEntryValid(id);
      } else if(s.HasPlacement && s.StateValue == State::PENDING_UPLOAD && s.StateWasValid) {
        // Был VALID, а теперь ждёт перезаливки — UV всё равно должны соответствовать новому S
        // (данные лежат по тем же пиксельным координатам, но нормализация изменилась).
        _setEntryValid(id);
      }
    }
    EntriesDirty_ = true;

    // Перестраиваем packer по текущим placements и новому размеру/слоям
    _rebuildPackersFromPlacements();

    GrewThisFlush_ = true;
    return true;
  }

  void _freePlacement(TextureId id) {
    Slot& s = Slots_[id];
    if(!s.HasPlacement) return;

    if(s.Place.Layer < Packers_.size()) {
      Packers_[s.Place.Layer].free(Rect{s.Place.X, s.Place.Y, s.Place.WP, s.Place.HP});
    }
    s.HasPlacement = false;
    s.Place = Placement{};
    s.StateWasValid = false;
  }

  void _rebuildPackersFromPlacements() {
    Packers_.clear();
    Packers_.resize(Atlas_.Layers);
    for(uint32_t l = 0; l < Atlas_.Layers; ++l) {
      Packers_[l].reset(Atlas_.Side, Atlas_.Side);
    }

    // Занимаем прямоугольники: проще "вставить" их, временно сбросив FreeRects и применив split.
    // Здесь делаем упрощённо: для каждого placement просто "split free" через Insert (в том же месте нельзя),
    // поэтому вместо этого: удаляем из free списка все пересечения и добавляем остатки — сложно.
    // Практичный компромисс: строим "новый packer" через последовательные Insert в порядке убывания площади,
    // сохраняя уже существующие координаты нельзя. Поэтому делаем корректнее:
    // - Создадим маску занятости списком usedRects, а затем выведем FreeRects как "полосы" — это сложно.
    //
    // В этой реализации: после роста/свапа мы допускаем, что packer не обязан идеально отражать текущее заполнение,
    // и рекомендуем пользователю при сильной фрагментации вызывать requestFullRepack().
    //
    // Однако, чтобы не потерять место полностью, добавим занятые как "запрещённые" через SplitFree_ вручную.
    for(TextureId id = 0; id < Cfg_.MaxTextureId; ++id) {
      const Slot& s = Slots_[id];
      if(!s.InUse || !s.HasPlacement) continue;
      if(s.Place.Layer >= Packers_.size()) continue;
      // имитируем занятие used (не проверяем пересечения — считаем, что данных не битые)
      MaxRectsBin& bin = Packers_[s.Place.Layer];
      bin._splitFree(Rect{s.Place.X, s.Place.Y, s.Place.WP, s.Place.HP});
      bin._prune();
    }
  }

  // ============================= Padding edge-extend =============================

  static void _writePaddedRGBA8(uint8_t* dst,
                                uint32_t dstRowPitch,
                                uint32_t w,
                                uint32_t h,
                                uint32_t p,
                                const uint8_t* src,
                                uint32_t srcRowPitch) {
    const uint32_t wP = w + 2u * p;
    const uint32_t hP = h + 2u * p;

    // 1) центр
    for(uint32_t y = 0; y < h; ++y) {
      uint8_t* row = dst + static_cast<size_t>(y + p) * dstRowPitch;
      std::memcpy(row + static_cast<size_t>(p) * 4u,
                  src + static_cast<size_t>(y) * srcRowPitch,
                  static_cast<size_t>(w) * 4u);
    }

    // 2) расширение по X для строк центра
    for(uint32_t y = 0; y < h; ++y) {
      uint8_t* row = dst + static_cast<size_t>(y + p) * dstRowPitch;

      const uint8_t* firstPx = row + static_cast<size_t>(p) * 4u;
      const uint8_t* lastPx  = row + static_cast<size_t>(p + w - 1) * 4u;

      for(uint32_t x = 0; x < p; ++x) {
        std::memcpy(row + static_cast<size_t>(x) * 4u, firstPx, 4u);
      }
      for(uint32_t x = 0; x < p; ++x) {
        std::memcpy(row + static_cast<size_t>(p + w + x) * 4u, lastPx, 4u);
      }
    }

    // 3) верхние p строк = первая строка центра
    const uint8_t* topSrc = dst + static_cast<size_t>(p) * dstRowPitch;
    for(uint32_t y = 0; y < p; ++y) {
      std::memcpy(dst + static_cast<size_t>(y) * dstRowPitch,
                  topSrc,
                  static_cast<size_t>(wP) * 4u);
    }

    // 4) нижние p строк = последняя строка центра
    const uint8_t* botSrc = dst + static_cast<size_t>(p + h - 1) * dstRowPitch;
    for(uint32_t y = 0; y < p; ++y) {
      std::memcpy(dst + static_cast<size_t>(p + h + y) * dstRowPitch,
                  botSrc,
                  static_cast<size_t>(wP) * 4u);
    }

    (void)hP;
  }

  bool _tryPackWithRectpack(uint32_t side,
                            uint32_t layers,
                            const std::vector<TextureId>& ids,
                            std::unordered_map<TextureId, PlannedPlacement>& outPlan) 
  {
    if(side == 0 || layers == 0) 
      return false;

    std::vector<rbp::MaxRectsBinPack> bins;
    bins.reserve(layers);
    for(uint32_t l = 0; l < layers; ++l)
      bins.emplace_back(static_cast<int>(side), static_cast<int>(side), false);

    outPlan.clear();
    outPlan.reserve(ids.size());

    for(TextureId id : ids) {
      const Slot& s = Slots_[id];
      const uint32_t wP = s.W + 2u * Cfg_.PaddingPx;
      const uint32_t hP = s.H + 2u * Cfg_.PaddingPx;

      bool placed = false;
      for(uint32_t layer = 0; layer < layers; ++layer) {
        rbp::Rect rect = bins[layer].Insert(
            static_cast<int>(wP),
            static_cast<int>(hP),
            rbp::MaxRectsBinPack::RectBestShortSideFit);

        if(rect.width > 0 && rect.height > 0) {
          outPlan[id] = PlannedPlacement{
              static_cast<uint32_t>(rect.x),
              static_cast<uint32_t>(rect.y),
              wP,
              hP,
              layer};
          placed = true;
          break;
        }
      }

      if(!placed) {
        outPlan.clear();
        return false;
      }
    }

    return true;
  }

  // ============================= Repack =============================

  void _startRepackIfPossible() {
    // Собираем кандидаты: все с доступными данными, кроме TOO_LARGE
    std::vector<TextureId> ids;
    ids.reserve(Cfg_.MaxTextureId);

    for(TextureId id = 0; id < Cfg_.MaxTextureId; ++id) {
      const Slot& s = Slots_[id];
      if(!s.InUse) continue;
      if(!s.HasCpuData) continue;
      if(s.TooLarge) continue;
      ids.push_back(id);
    }

    if(ids.empty()) {
      Repack_.Requested = false;
      return;
    }

    // сортировка по площади убыванию (wP*hP)
    std::sort(ids.begin(), ids.end(), [&](TextureId a, TextureId b) {
      const Slot& A = Slots_[a];
      const Slot& B = Slots_[b];
      const uint64_t areaA = uint64_t(A.W + 2u*Cfg_.PaddingPx) * uint64_t(A.H + 2u*Cfg_.PaddingPx);
      const uint64_t areaB = uint64_t(B.W + 2u*Cfg_.PaddingPx) * uint64_t(B.H + 2u*Cfg_.PaddingPx);
      return areaA > areaB;
    });

    // Выбираем capacity по mode
    uint32_t targetSide = Atlas_.Side;
    uint32_t targetLayers = Atlas_.Layers;

    std::unordered_map<TextureId, PlannedPlacement> plan;

    if(Repack_.Mode == RepackMode::KeepCurrentCapacity) {
      if(!_tryPackWithRectpack(targetSide, targetLayers, ids, plan)) {
        _emitEventOncePerFlush(AtlasEvent::AtlasOutOfSpace);
        Repack_.Requested = false;
        return;
      }
    } else if(Repack_.Mode == RepackMode::Tightest) {
      // Минимальный side/layers из допустимых
      const std::array<uint32_t, 3> sides{1024u, 2048u, 4096u};
      bool ok = false;
      for(uint32_t s : sides) {
        for(uint32_t l = 1; l <= Cfg_.MaxLayers; ++l) {
          if(_tryPackWithRectpack(s, l, ids, plan)) {
            targetSide = s;
            targetLayers = l;
            ok = true;
            break;
          }
        }
        if(ok) break;
      }
      if(!ok) {
        _emitEventOncePerFlush(AtlasEvent::AtlasOutOfSpace);
        Repack_.Requested = false;
        return;
      }
    } else { // AllowGrow
      // Сначала текущая capacity, потом растим layers, потом side.
      bool ok = _tryPackWithRectpack(targetSide, targetLayers, ids, plan);
      if(!ok) {
        // grow layers
        for(uint32_t l = targetLayers + 1; l <= Cfg_.MaxLayers && !ok; ++l) {
          if(_tryPackWithRectpack(targetSide, l, ids, plan)) {
            targetLayers = l;
            ok = true;
          }
        }
      }
      if(!ok) {
        // grow side
        const std::array<uint32_t, 3> sides{1024u, 2048u, 4096u};
        for(uint32_t s : sides) {
          if(s < targetSide) continue;
          for(uint32_t l = 1; l <= Cfg_.MaxLayers; ++l) {
            if(_tryPackWithRectpack(s, l, ids, plan)) {
              targetSide = s;
              targetLayers = l;
              ok = true;
              break;
            }
          }
          if(ok) break;
        }
      }
      if(!ok) {
        _emitEventOncePerFlush(AtlasEvent::AtlasOutOfSpace);
        Repack_.Requested = false;
        return;
      }
    }

    // Создаём новый atlas (пока не активный)
    ImageRes newAtlas{};
    if(!_createAtlasNoThrow(targetSide, targetLayers, newAtlas)) {
      _emitEventOncePerFlush(AtlasEvent::GpuOutOfMemory);
      // оставляем requested=true, чтобы попробовать снова в следующий Flush
      return;
    }

    // Ставим repack active
    Repack_.Active = true;
    Repack_.Requested = false;
    Repack_.SwapReady = false;
    Repack_.WaitingGpuForReady = false;
    Repack_.Atlas = newAtlas;
    Repack_.Plan = std::move(plan);

    Repack_.Pending.clear();
    Repack_.InPending.assign(Cfg_.MaxTextureId, false);

    // Заполняем очередь аплоада всеми текстурами из плана
    Repack_.Pending.resize(0);
    for(TextureId id : ids) {
      if(Repack_.Plan.count(id) == 0) continue;
      Repack_.Pending.push_back(id);
      Repack_.InPending[id] = true;
    }

    _emitEventOncePerFlush(AtlasEvent::RepackStarted);
  }

  void _swapToRepackedAtlas() {
    // Переключаем текущий atlas на Repack_.Atlas, а старый — в deferred destroy
    DeferredImages_.push_back(Atlas_);
    Atlas_ = Repack_.Atlas;
    Repack_.Atlas = ImageRes{};

    // Применяем placements из плана
    for(TextureId id = 0; id < Cfg_.MaxTextureId; ++id) {
      Slot& s = Slots_[id];
      if(!s.InUse) continue;

      auto it = Repack_.Plan.find(id);
      if(it != Repack_.Plan.end() && s.HasCpuData && !s.TooLarge) {
        const PlannedPlacement& pp = it->second;
        s.HasPlacement = true;
        s.Place = Placement{pp.X, pp.Y, pp.WP, pp.HP, pp.Layer};
        s.StateValue = State::VALID;
        s.StateWasValid = true;
        _setEntryValid(id);
      } else {
        // Если данные доступны, но в план не попала — оставим PENDING_UPLOAD без placement
        if(s.HasCpuData && !s.TooLarge) {
          s.StateValue = State::PENDING_UPLOAD;
          s.StateWasValid = false;
          s.HasPlacement = false;
          _enqueuePending(id);
          _setEntryInvalid(id, /*diagPending*/true, /*diagTooLarge*/false);
        } else {
          // REGISTERED/TOO_LARGE и т.п.
          s.HasPlacement = false;
          s.StateWasValid = false;
          if(s.TooLarge) {
            _setEntryInvalid(id, /*diagPending*/false, /*diagTooLarge*/true);
          } else {
            _setEntryInvalid(id, /*diagPending*/false, /*diagTooLarge*/false);
          }
        }
      }
    }

    EntriesDirty_ = true;

    // Перестроить packer по placements нового атласа
    _rebuildPackersFromPlacements();

    // Сброс repack state
    Repack_.Active = false;
    Repack_.SwapReady = false;
    Repack_.WaitingGpuForReady = false;
    Repack_.Plan.clear();
    Repack_.Pending.clear();
    Repack_.InPending.clear();

    _emitEventOncePerFlush(AtlasEvent::RepackFinished);
  }

  // ============================= Layout/барьеры =============================

  void _ensureImageLayoutForTransferDst(VkCommandBuffer cmd, ImageRes& img, bool& anyWritesFlag) {
    if(img.Layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      _transitionImage(cmd, img,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       (img.Layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT),
                       VK_ACCESS_TRANSFER_WRITE_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
    }
    anyWritesFlag = true;
  }

  void _transitionImage(VkCommandBuffer cmd,
                        ImageRes& img,
                        VkImageLayout newLayout,
                        VkAccessFlags srcAccess,
                        VkAccessFlags dstAccess,
                        VkPipelineStageFlags srcStage,
                        VkPipelineStageFlags dstStage) {
    if(img.Image == VK_NULL_HANDLE) return;
    if(img.Layout == newLayout) return;

    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = img.Layout;
    b.newLayout = newLayout;
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img.Image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = img.Layers;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    img.Layout = newLayout;
  }

  // ============================= Vulkan create/destroy =============================

  uint32_t _findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(Phys_, &mp);
    for(uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
      if((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) {
        return i;
      }
    }
    throw std::runtime_error("TextureAtlas: no suitable memory type");
  }

  void _createEntriesBufferOrThrow() {
    Entries_.Size = static_cast<VkDeviceSize>(Cfg_.MaxTextureId) * sizeof(Entry);

    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = Entries_.Size;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(Device_, &bi, nullptr, &Entries_.Buffer) != VK_SUCCESS) {
      throw std::runtime_error("TextureAtlas: vkCreateBuffer(entries) failed");
    }

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(Device_, Entries_.Buffer, &mr);

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = _findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if(vkAllocateMemory(Device_, &ai, nullptr, &Entries_.Memory) != VK_SUCCESS) {
      throw std::runtime_error("TextureAtlas: vkAllocateMemory(entries) failed");
    }

    vkBindBufferMemory(Device_, Entries_.Buffer, Entries_.Memory, 0);
  }

  void _createSamplerOrThrow() {
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = Cfg_.SamplerFilter;
    si.minFilter = Cfg_.SamplerFilter;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.mipLodBias = 0.0f;
    si.anisotropyEnable = Cfg_.SamplerAnisotropyEnable ? VK_TRUE : VK_FALSE;
    si.maxAnisotropy = Cfg_.SamplerAnisotropyEnable ? 4.0f : 1.0f;
    si.compareEnable = VK_FALSE;
    si.minLod = 0.0f;
    si.maxLod = 0.0f; // mipLevels=1
    si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    si.unnormalizedCoordinates = VK_FALSE;

    if(vkCreateSampler(Device_, &si, nullptr, &Sampler_) != VK_SUCCESS) {
      throw std::runtime_error("TextureAtlas: vkCreateSampler failed");
    }
  }

  void _createAtlasOrThrow(uint32_t side, uint32_t layers) {
    if(!_createAtlasNoThrow(side, layers, Atlas_)) {
      throw std::runtime_error("TextureAtlas: create atlas failed (OOM?)");
    }
  }

  bool _createAtlasNoThrow(uint32_t side, uint32_t layers, ImageRes& out) {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = { side, side, 1 };
    ii.mipLevels = 1;
    ii.arrayLayers = layers;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage img = VK_NULL_HANDLE;
    if(vkCreateImage(Device_, &ii, nullptr, &img) != VK_SUCCESS) {
      return false;
    }

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(Device_, img, &mr);

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    uint32_t memType = 0;
    try {
      memType = _findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    } catch (...) {
      vkDestroyImage(Device_, img, nullptr);
      return false;
    }
    ai.memoryTypeIndex = memType;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    if(vkAllocateMemory(Device_, &ai, nullptr, &mem) != VK_SUCCESS) {
      vkDestroyImage(Device_, img, nullptr);
      return false;
    }

    vkBindImageMemory(Device_, img, mem, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.baseMipLevel = 0;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount = layers;

    VkImageView view = VK_NULL_HANDLE;
    if(vkCreateImageView(Device_, &vi, nullptr, &view) != VK_SUCCESS) {
      vkFreeMemory(Device_, mem, nullptr);
      vkDestroyImage(Device_, img, nullptr);
      return false;
    }

    out.Image = img;
    out.Memory = mem;
    out.View = view;
    out.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    out.Side = side;
    out.Layers = layers;
    return true;
  }

  void _destroyImage(ImageRes& img) {
    if(img.View) vkDestroyImageView(Device_, img.View, nullptr);
    if(img.Image) vkDestroyImage(Device_, img.Image, nullptr);
    if(img.Memory) vkFreeMemory(Device_, img.Memory, nullptr);
    img = ImageRes{};
  }

  void _destroyBuffer(BufferRes& b) {
    if(b.Buffer)
      vkDestroyBuffer(Device_, b.Buffer, nullptr);
    if(b.Memory)
      vkFreeMemory(Device_, b.Memory, nullptr);

    b = BufferRes{};
  }

  void _shutdownNoThrow() {
    if(!Alive_) return;

    // deferred
    for(auto& img : DeferredImages_) _destroyImage(img);
    DeferredImages_.clear();

    // repack atlas (если был)
    if(Repack_.Atlas.Image) {
      _destroyImage(Repack_.Atlas);
    }

    _destroyImage(Atlas_);
    _destroyBuffer(Entries_);

    Staging_.reset();

    if(OwnsSampler_ && Sampler_) {
      vkDestroySampler(Device_, Sampler_, nullptr);
    }
    Sampler_ = VK_NULL_HANDLE;

    Alive_ = false;
  }

  // ============================= Данные/поля =============================

  VkDevice Device_ = VK_NULL_HANDLE;
  VkPhysicalDevice Phys_ = VK_NULL_HANDLE;
  Config Cfg_{};
  EventCallback OnEvent_;

  bool Alive_ = false;

  VkDeviceSize CopyOffsetAlignment_ = 4;

  std::shared_ptr<SharedStagingBuffer> Staging_;
  BufferRes Entries_{};
  ImageRes Atlas_{};
  VkSampler Sampler_ = VK_NULL_HANDLE;
  bool OwnsSampler_ = false;

  std::vector<Entry> EntriesCpu_;
  bool EntriesDirty_ = false;

  std::vector<Slot> Slots_;
  std::vector<TextureId> FreeIds_;
  TextureId NextId_ = 0;

  // pending очередь (ленивое удаление)
  std::deque<TextureId> Pending_;
  std::vector<bool> PendingInQueue_ = std::vector<bool>(Cfg_.MaxTextureId, false);

  // packer по слоям
  std::vector<MaxRectsBin> Packers_;

  // deferred destroy старых атласов
  std::vector<ImageRes> DeferredImages_;

  // события "один раз за Flush"
  uint32_t FlushEventMask_ = 0;
  uint32_t PendingLayerGrow_ = 0;

  // рост индикатор
  bool GrewThisFlush_ = false;

  // repack state
  RepackState Repack_;
};
