#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstring>

// ========================
// External texture view
// ========================
struct Texture {
  uint32_t Width, Height;
  const uint32_t* Pixels; // assumed 0xAARRGGBB
};

// ========================
// Bytecode words are uint8_t (1 byte machine word)
// TexId is u24 (3 bytes, little-endian)
// Subprogram refs use off24/len24 in BYTES (<=65535)
// ========================
class TexturePipelineProgram {
public:
  using Word = uint8_t;

  enum AnimFlags : Word {
    AnimSmooth     = 1u << 0,
    AnimHorizontal = 1u << 1,
    AnimGrid       = 1u << 2
  };

  static constexpr uint16_t DefaultAnimFpsQ = uint16_t(8u * 256u);
  static constexpr size_t MaxCodeBytes = (1u << 16) + 1u; // 65537

  struct OwnedTexture {
    uint32_t Width = 0, Height = 0;
    std::vector<uint32_t> Pixels;
    Texture view() const { return Texture{Width, Height, Pixels.data()}; }
  };

  using IdResolverFunc      = std::function<std::optional<uint32_t>(std::string_view)>;
  using TextureProviderFunc = std::function<std::optional<Texture>(uint32_t)>;

  // Patch point to 3 consecutive bytes where u24 texId lives (b0,b1,b2)
  struct Patch {
    size_t ByteIndex0 = 0; // Code_[i], Code_[i+1], Code_[i+2]
    std::string Name;
  };

  bool compile(std::string_view src, std::string* err = nullptr);
  bool link(const IdResolverFunc& resolver, std::string* err = nullptr);
  bool bake(const TextureProviderFunc& provider, OwnedTexture& out, std::string* err = nullptr) const;
  bool bake(const TextureProviderFunc& provider, OwnedTexture& out, double timeSeconds, std::string* err = nullptr) const;

  const std::vector<Word>& words() const { return Code_; }
  const std::vector<Patch>& patches() const { return Patches_; }

  std::vector<uint8_t> toBytes() const { return Code_; }

  struct AnimSpec {
    uint32_t TexId = 0;
    bool HasTexId = false;
    uint16_t FrameW = 0;
    uint16_t FrameH = 0;
    uint16_t FrameCount = 0;
    uint16_t FpsQ = 0;
    uint16_t Flags = 0;
  };

  static std::vector<AnimSpec> extractAnimationSpecs(const Word* code, size_t size);
  static bool remapTexIds(std::vector<uint8_t>& code, const std::vector<uint32_t>& remap, std::string* err = nullptr);

  static std::vector<AnimSpec> extractAnimationSpecs(const std::vector<Word>& code) {
    return extractAnimationSpecs(code.data(), code.size());
  }

  void fromBytes(std::vector<uint8_t> bytes);

private:
  // ========================
  // Byte helpers (little-endian)
  // ========================
  static inline uint16_t _rd16(const std::vector<uint8_t>& c, size_t& ip) {
    uint16_t v = uint16_t(c[ip]) | (uint16_t(c[ip+1]) << 8);
    ip += 2;
    return v;
  }
  static inline uint32_t _rd24(const std::vector<uint8_t>& c, size_t& ip) {
    uint32_t v = uint32_t(c[ip]) | (uint32_t(c[ip+1]) << 8) | (uint32_t(c[ip+2]) << 16);
    ip += 3;
    return v;
  }
  static inline uint32_t _rd32(const std::vector<uint8_t>& c, size_t& ip) {
    uint32_t v = uint32_t(c[ip]) |
                 (uint32_t(c[ip+1]) << 8) |
                 (uint32_t(c[ip+2]) << 16) |
                 (uint32_t(c[ip+3]) << 24);
    ip += 4;
    return v;
  }

  static inline void _wr8 (std::vector<uint8_t>& o, uint32_t v){ o.push_back(uint8_t(v & 0xFFu)); }
  static inline void _wr16(std::vector<uint8_t>& o, uint32_t v){
    o.push_back(uint8_t(v & 0xFFu));
    o.push_back(uint8_t((v >> 8) & 0xFFu));
  }
  static inline void _wr24(std::vector<uint8_t>& o, uint32_t v){
    o.push_back(uint8_t(v & 0xFFu));
    o.push_back(uint8_t((v >> 8) & 0xFFu));
    o.push_back(uint8_t((v >> 16) & 0xFFu));
  }
  static inline void _wr32(std::vector<uint8_t>& o, uint32_t v){
    o.push_back(uint8_t(v & 0xFFu));
    o.push_back(uint8_t((v >> 8) & 0xFFu));
    o.push_back(uint8_t((v >> 16) & 0xFFu));
    o.push_back(uint8_t((v >> 24) & 0xFFu));
  }

  // ========================
  // SrcRef encoding in bytes (variable length)
  //   kind(1) + payload
  //   TexId: id24(3)              => total 4
  //   Sub  : off16(3) + len16(3)  => total 7
  // ========================
  enum class SrcKind : uint8_t { TexId = 0, Sub = 1 };

  struct SrcRef {
    SrcKind Kind{};
    uint32_t TexId24 = 0;   // for TexId
    uint16_t Off24 = 0;     // for Sub
    uint16_t Len24 = 0;     // for Sub
  };

  // ========================
  // Opcodes (1 byte)
  // ========================
  enum class Op : uint8_t {
    End = 0,

    Base_Tex  = 1,  // SrcRef(TexId)
    Base_Fill = 2,  // w16, h16, color32
    Base_Anim = 3,  // SrcRef(TexId), frameW16, frameH16, frames16, fpsQ16, flags8

    Resize    = 10, // w16, h16
    Transform = 11, // t8
    Opacity   = 12, // a8
    NoAlpha   = 13, // -
    MakeAlpha = 14, // rgb24 (3 bytes) RR,GG,BB
    Invert    = 15, // mask8
    Brighten  = 16, // -
    Contrast  = 17, // cBias8, bBias8 (bias-127)
    Multiply  = 18, // color32
    Screen    = 19, // color32
    Colorize  = 20, // color32, ratio8
    Anim      = 21, // frameW16, frameH16, frames16, fpsQ16, flags8

    Overlay   = 30, // SrcRef (var)
    Mask      = 31, // SrcRef (var)
    LowPart   = 32, // percent8, SrcRef (var)

    Combine   = 40  // w16,h16,n16 then n*(x16,y16,SrcRef)  (если понадобится — допишем DSL)
  };

  // ========================
  // Pixel helpers (assume 0xAARRGGBB)
  // ========================
  static inline uint8_t _a(uint32_t c){ return uint8_t((c >> 24) & 0xFF); }
  static inline uint8_t _r(uint32_t c){ return uint8_t((c >> 16) & 0xFF); }
  static inline uint8_t _g(uint32_t c){ return uint8_t((c >>  8) & 0xFF); }
  static inline uint8_t _b(uint32_t c){ return uint8_t((c >>  0) & 0xFF); }
  static inline uint32_t _pack(uint8_t a,uint8_t r,uint8_t g,uint8_t b){
    return (uint32_t(a)<<24)|(uint32_t(r)<<16)|(uint32_t(g)<<8)|(uint32_t(b));
  }
  static inline uint8_t _clampu8(int v){ return uint8_t(std::min(255, std::max(0, v))); }

  // ========================
  // VM (executes bytes)
  // ========================
  struct Image {
    uint32_t W=0,H=0;
    std::vector<uint32_t> Px;
  };

  class VM {
  public:
    using TextureProvider = TexturePipelineProgram::TextureProviderFunc;

    explicit VM(TextureProvider provider);
    bool run(const std::vector<uint8_t>& code, OwnedTexture& out, double timeSeconds, std::string* err);

  private:
    TextureProvider Provider_;

    static bool _bad(std::string* err, const char* msg);
    static bool _readSrc(const std::vector<uint8_t>& code, size_t& ip, SrcRef& out, std::string* err);
    Image _loadTex(uint32_t id, std::unordered_map<uint32_t, Image>& cache, std::string* err);
    Image _loadSub(const std::vector<uint8_t>& code,
                   uint32_t off, uint32_t len,
                   std::unordered_map<uint32_t, Image>& texCache,
                   std::unordered_map<uint64_t, Image>& subCache,
                   double timeSeconds,
                   std::string* err);
    Image _loadSrc(const std::vector<uint8_t>& code,
                   const SrcRef& src,
                   std::unordered_map<uint32_t, Image>& texCache,
                   std::unordered_map<uint64_t, Image>& subCache,
                   double timeSeconds,
                   std::string* err);

    // ---- image ops (как в исходнике) ----
    static Image _makeSolid(uint32_t w, uint32_t h, uint32_t color);
    static Image _resizeNN(const Image& src, uint32_t nw, uint32_t nh);
    static Image _resizeNN_ifNeeded(Image img, uint32_t w, uint32_t h);
    static Image _cropFrame(const Image& sheet, uint32_t index, uint32_t fw, uint32_t fh, bool horizontal);
    static Image _cropFrameGrid(const Image& sheet, uint32_t index, uint32_t fw, uint32_t fh);
    static void _lerp(Image& base, const Image& over, double t);
    static void _alphaOver(Image& base, const Image& over);
    static void _applyMask(Image& base, const Image& mask);
    static void _opacity(Image& img, uint8_t mul);
    static void _noAlpha(Image& img);
    static void _makeAlpha(Image& img, uint32_t rgb24);
    static void _invert(Image& img, uint32_t maskBits);
    static void _brighten(Image& img);
    static void _contrast(Image& img, int c, int br);
    static void _multiply(Image& img, uint32_t color);
    static void _screen(Image& img, uint32_t color);
    static void _colorize(Image& img, uint32_t color, uint8_t ratio);
    static void _lowpart(Image& base, const Image& over, uint32_t percent);
    static Image _transform(const Image& src, uint32_t t);
  };

  // ========================
  // Minimal DSL Lexer/Parser
  // now supports:
  //   name |> op(...)
  //   32x32 "#RRGGBBAA"
  // optional prefix:
  //   tex name |> op(...)
  // nested only where op expects a texture arg:
  //   overlay( tex other |> ... )
  // Also supports overlay(other) / mask(other) / lowpart(50, other)
  // ========================
  enum class TokKind { End, Ident, Number, String, Pipe, Comma, LParen, RParen, Eq, X };

  struct Tok {
    TokKind Kind = TokKind::End;
    std::string Text;
    uint32_t U32 = 0;
  };

  struct Lexer {
    std::string_view S;
    size_t I=0;

    bool HasBuf = false;
    Tok  Buf;

    static bool isAlpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
    static bool isNum(char c){ return (c>='0'&&c<='9'); }
    static bool isAlnum(char c){ return isAlpha(c)||isNum(c); }

    void unread(const Tok& t);
    Tok peek();
    void skipWs();
    Tok next();
  };


  struct ArgVal {
    enum class ValueKind { U32, Str, Ident };
    ValueKind Kind = ValueKind::U32;
    uint32_t U32 = 0;
    std::string S;
  };

  struct ParsedOp {
    std::string Name;
    std::vector<ArgVal> Pos;
    std::unordered_map<std::string, ArgVal> Named;
  };

  // ========================
  // Compiler state
  // ========================
  std::string Source_;
  std::vector<uint8_t> Code_;
  std::vector<Patch> Patches_;

  // ---- emit helpers (target = arbitrary out vector) ----
  static inline void _emitOp(std::vector<uint8_t>& out, Op op) { _wr8(out, uint8_t(op)); }
  static inline void _emitU8(std::vector<uint8_t>& out, uint32_t v){ _wr8(out, v); }
  static inline void _emitU16(std::vector<uint8_t>& out, uint32_t v){ _wr16(out, v); }
  static inline void _emitU24(std::vector<uint8_t>& out, uint32_t v){ _wr24(out, v); }
  static inline void _emitU32(std::vector<uint8_t>& out, uint32_t v){ _wr32(out, v); }

  // reserve 3 bytes for u24 texId and register patch (absolute or relative)
  struct RelPatch { size_t Rel0; std::string Name; };

  static void _emitTexPatchU24(std::vector<uint8_t>& out,
                              std::vector<Patch>* absPatches,
                              std::vector<RelPatch>* relPatches,
                              const std::string& name) {
    const size_t idx = out.size();
    out.push_back(0); out.push_back(0); out.push_back(0);
    if(absPatches) absPatches->push_back(Patch{idx, name});
    if(relPatches) relPatches->push_back(RelPatch{idx, name});
  }

  static void _emitSrcTexName(std::vector<uint8_t>& out,
                              std::vector<Patch>* absPatches,
                              std::vector<RelPatch>* relPatches,
                              const std::string& name) {
    _emitU8(out, uint8_t(SrcKind::TexId));
    _emitTexPatchU24(out, absPatches, relPatches, name);
  }

  static void _emitSrcSub(std::vector<uint8_t>& out, uint32_t off24, uint32_t len24) {
    _emitU8(out, uint8_t(SrcKind::Sub));
    _emitU24(out, off24);
    _emitU24(out, len24);
  }

  // ========================
  // Color parsing: #RRGGBB or #RRGGBBAA -> 0xAARRGGBB
  // ========================
  static bool _parseHexColor(std::string_view s, uint32_t& outARGB);

  // ========================
  // Parsing entry: full program
  // ========================
  bool _parseProgram(std::string* err);

  // ========================
  // Base compilation (optionally after 'tex')
  // supports:
  //   1) name
  //   2) "name(.png/.jpg/.jpeg)"  (allowed but normalized)
  //   3) anim(...)
  //   4) 32x32 "#RRGGBBAA"
  // optional: all of the above may be prefixed with 'tex'
  // ========================
  bool _compileBaseAfterTex(Lexer& lx,
                            std::vector<uint8_t>& out,
                            std::vector<Patch>* absPatches,
                            std::vector<RelPatch>* relPatches,
                            std::string* err);

  bool _compileBaseFromToken(Lexer& lx,
                             const Tok& a,
                             std::vector<uint8_t>& out,
                             std::vector<Patch>* absPatches,
                             std::vector<RelPatch>* relPatches,
                             std::string* err);

  // ========================
  // Args parsing:
  //  - normal args: (a,b,key=v)
  //  - OR if first token inside '(' is 'tex' => parse nested program until ')'
  // ========================
  bool _parseArgListOrTextureExpr(Lexer& lx, ParsedOp& op, std::string* err);

  bool _parseArgList(Lexer& lx, ParsedOp& op, std::string* err);

  bool _tokToVal(const Tok& t, ArgVal& out, std::string* err);

  // ========================
  // Subprogram compilation:
  // we already consumed 'tex'. Parse base + pipeline until next token is ')'
  // DO NOT consume ')'
  // ========================
  struct PendingSubData {
    std::vector<uint8_t> Bytes;
    std::vector<RelPatch> RelPatches;
  };

  bool _compileSubProgramFromAlreadySawTex(Lexer& lx, PendingSubData& outSub, std::string* err);

  // pending subprogram associated with ParsedOp pointer (created during parsing)
  mutable std::unordered_map<const ParsedOp*, PendingSubData> PendingSub_;

  // Append subprogram to `out` and emit SrcRef(Sub, off16, len16), migrating patches properly.
  static bool _appendSubprogram(std::vector<uint8_t>& out,
                                PendingSubData&& sub,
                                std::vector<Patch>* absPatches,
                                std::vector<RelPatch>* relPatches,
                                uint32_t& outOff,
                                uint32_t& outLen,
                                std::string* err);

  // ========================
  // Compile operations into arbitrary `out`
  // absPatches != nullptr => patches recorded as absolute for this buffer
  // relPatches != nullptr => patches recorded as relative for this buffer
  // ========================
  bool _compileOpInto(Lexer& lx,
                      const ParsedOp& op,
                      std::vector<uint8_t>& out,
                      std::vector<Patch>* absPatches,
                      std::vector<RelPatch>* relPatches,
                      std::string* err);
};
