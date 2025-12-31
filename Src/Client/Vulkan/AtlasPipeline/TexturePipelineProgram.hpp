#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <cstring>

// ========================
// External texture view
// ========================
struct Texture {
  uint32_t Width, Height;
  const uint32_t* Pixels; // assumed 0xAARRGGBB
};

// ========================
// Bytecode words are uint16_t
// ========================
class TexturePipelineProgram {
public:
  using Word = uint16_t;

  struct OwnedTexture {
    uint32_t Width = 0, Height = 0;
    std::vector<uint32_t> Pixels;
    Texture view() const { return Texture{Width, Height, Pixels.data()}; }
  };

  // name -> uint32 id
  using IdResolver      = std::function<std::optional<uint32_t>(std::string_view)>;
  // id -> Texture view
  using TextureProvider = std::function<std::optional<Texture>(uint32_t)>;

  // Patch points to two consecutive u16 words where uint32 texId lives (lo, hi)
  struct Patch {
    size_t WordIndexLo = 0; // Code_[lo], Code_[lo+1] is hi
    std::string Name;
  };

  // ---- compile / link / bake ----
  bool compile(std::string src, std::string* err = nullptr) {
    Source_ = std::move(src);
    Code_.clear();
    Patches_.clear();
    return _parseProgram(err);
  }

  bool link(const IdResolver& resolver, std::string* err = nullptr) {
    for(const auto& p : Patches_) {
      auto id = resolver(p.Name);
      if(!id) {
        if(err) *err = "Unresolved texture name: " + p.Name;
        return false;
      }
      if(p.WordIndexLo + 1 >= Code_.size()) {
        if(err) *err = "Internal error: patch out of range";
        return false;
      }
      Code_[p.WordIndexLo + 0] = _lo16(*id);
      Code_[p.WordIndexLo + 1] = _hi16(*id);
    }
    return true;
  }

  bool bake(const TextureProvider& provider, OwnedTexture& out, std::string* err = nullptr) const {
    VM vm(provider);
    return vm.run(Code_, out, err);
  }

  const std::vector<Word>& words() const { return Code_; }
  const std::vector<Patch>& patches() const { return Patches_; }

  // Serialize words to bytes (little-endian)
  std::vector<uint8_t> toBytes() const {
    std::vector<uint8_t> bytes(Code_.size() * sizeof(Word));
    std::memcpy(bytes.data(), Code_.data(), bytes.size());
    return bytes;
  }

  void fromWords(std::vector<Word> words) {
    Code_ = std::move(words);
    Patches_.clear();
    Source_.clear();
  }

private:
  // ========================
  // Word helpers
  // ========================
  static constexpr uint32_t _make_u32(uint16_t lo, uint16_t hi) {
    return uint32_t(lo) | (uint32_t(hi) << 16);
  }
  static constexpr uint16_t _lo16(uint32_t v) { return uint16_t(v & 0xFFFFu); }
  static constexpr uint16_t _hi16(uint32_t v) { return uint16_t((v >> 16) & 0xFFFFu); }

  // ========================
  // SrcRef encoding in u16 words
  //    kind + a + b  (3 words)
  //    kind=0 TexId: a=_lo16(id), b=_hi16(id)
  //    kind=1 Sub  : a=offsetWords, b=lenWords
  // ========================
  enum class SrcKind : Word { TexId = 0, Sub = 1 };

  struct SrcRef {
    SrcKind Kind;
    Word A;
    Word B;
  };

  // ========================
  // Opcodes (fixed-length headers; some are variable like Combine)
  // ========================
  enum class Op : Word {
    End = 0,

    // Base producers (top-level expression must start with one of these)
    Base_Tex   = 1, // args: SrcRef(TexId) -> kind,lo,hi
    Base_Fill  = 2, // args: w, h, color_lo, color_hi

    // Unary ops on current image
    Resize     = 10, // w, h
    Transform  = 11, // t(0..7)
    Opacity    = 12, // a(0..255)
    NoAlpha    = 13, // -
    MakeAlpha  = 14, // rgb_lo(0xRRGG), rgb_hi(0x00BB) packed as 24-bit in 2 words
    Invert     = 15, // mask bits (r=1 g=2 b=4 a=8)
    Brighten   = 16, // -
    Contrast   = 17, // contrast_bias(0..254), bright_bias(0..254) where v = bias-127
    Multiply   = 18, // color_lo, color_hi (0xAARRGGBB)
    Screen     = 19, // color_lo, color_hi
    Colorize   = 20, // color_lo, color_hi, ratio(0..255)

    // Ops that consume a SrcRef (TexId or Sub)
    Overlay    = 30, // SrcRef (3 words)
    Mask       = 31, // SrcRef
    LowPart    = 32, // percent(0..100), SrcRef (1 + 3 words)

    // Variable example (optional): Combine
    // Combine: w,h, n, then n times: x,y, SrcRef (x,y,kind,a,b)
    Combine    = 40
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
  // VM (executes u16 words)
  // ========================
  struct Image {
    uint32_t W=0,H=0;
    std::vector<uint32_t> Px;
  };

  class VM {
  public:
    explicit VM(TextureProvider provider) : Provider_(std::move(provider)) {}

    bool run(const std::vector<Word>& code, OwnedTexture& out, std::string* err) {
      if(code.empty()) { if(err) *err="Empty bytecode"; return false; }

      Image cur;
      std::unordered_map<uint32_t, Image> texCache;
      std::unordered_map<uint32_t, Image> subCache; // key = (offset<<16)|len (fits if <=65535)

      size_t ip = 0;
      auto need = [&](size_t n)->bool{
        if(ip + n > code.size()) { if(err) *err="Bytecode truncated"; return false; }
        return true;
      };

      while (true) {
        if(!need(1)) return false;
        Op op = static_cast<Op>(code[ip++]);
        if(op == Op::End) break;

        switch (op) {
          case Op::Base_Tex: {
            if(!need(3)) return false;
            SrcRef src = _readSrc(code, ip);
            if(src.Kind != SrcKind::TexId) return _bad(err, "Base_Tex must be TexId");
            cur = _loadTex(_make_u32(src.A, src.B), texCache, err);
            if(cur.W == 0) return false;
          } break;

          case Op::Base_Fill: {
            if(!need(4)) return false;
            uint32_t w = code[ip++], h = code[ip++];
            uint32_t colorLo = code[ip++];
            uint32_t colorHi = code[ip++];
            uint32_t color = _make_u32(colorLo, colorHi);
            cur = _makeSolid(w, h, color);
          } break;

          case Op::Overlay: {
            if(!need(3)) return false;
            SrcRef src = _readSrc(code, ip);
            Image over = _loadSrc(code, src, texCache, subCache, err);
            if(over.W == 0) return false;
            if(!cur.W) { cur = std::move(over); break; } // if no base, adopt
            over = _resizeNN_ifNeeded(over, cur.W, cur.H);
            _alphaOver(cur, over);
          } break;

          case Op::Mask: {
            if(!need(3)) return false;
            SrcRef src = _readSrc(code, ip);
            Image m = _loadSrc(code, src, texCache, subCache, err);
            if(m.W == 0) return false;
            if(!cur.W) return _bad(err, "Mask requires base image");
            m = _resizeNN_ifNeeded(m, cur.W, cur.H);
            _applyMask(cur, m);
          } break;

          case Op::LowPart: {
            if(!need(1+3)) return false;
            uint32_t pct = std::min<uint32_t>(100u, code[ip++]);
            SrcRef src = _readSrc(code, ip);
            Image over = _loadSrc(code, src, texCache, subCache, err);
            if(over.W == 0) return false;
            if(!cur.W) return _bad(err, "LowPart requires base image");
            over = _resizeNN_ifNeeded(over, cur.W, cur.H);
            _lowpart(cur, over, pct);
          } break;

          case Op::Resize: {
            if(!need(2)) return false;
            uint32_t w = code[ip++], h = code[ip++];
            if(!cur.W) return _bad(err, "Resize requires base image");
            cur = _resizeNN(cur, w, h);
          } break;

          case Op::Transform: {
            if(!need(1)) return false;
            uint32_t t = code[ip++] & 7u;
            if(!cur.W) return _bad(err, "Transform requires base image");
            cur = _transform(cur, t);
          } break;

          case Op::Opacity: {
            if(!need(1)) return false;
            uint32_t a = code[ip++] & 0xFFu;
            if(!cur.W) return _bad(err, "Opacity requires base image");
            _opacity(cur, uint8_t(a));
          } break;

          case Op::NoAlpha: {
            if(!cur.W) return _bad(err, "NoAlpha requires base image");
            _noAlpha(cur);
          } break;

          case Op::MakeAlpha: {
            if(!need(2)) return false;
            uint32_t rgb24 = (uint32_t(code[ip+1]) << 16) | uint32_t(code[ip]); // lo has RR GG, hi has 00 BB
            ip += 2;
            if(!cur.W) return _bad(err, "MakeAlpha requires base image");
            _makeAlpha(cur, rgb24 & 0x00FFFFFFu);
          } break;

          case Op::Invert: {
            if(!need(1)) return false;
            uint32_t mask = code[ip++] & 0xFu;
            if(!cur.W) return _bad(err, "Invert requires base image");
            _invert(cur, mask);
          } break;

          case Op::Brighten: {
            if(!cur.W) return _bad(err, "Brighten requires base image");
            _brighten(cur);
          } break;

          case Op::Contrast: {
            if(!need(2)) return false;
            int c = int(code[ip++]) - 127;
            int b = int(code[ip++]) - 127;
            if(!cur.W) return _bad(err, "Contrast requires base image");
            _contrast(cur, c, b);
          } break;

          case Op::Multiply: {
            if(!need(2)) return false;
            uint32_t colorLo = code[ip++];
            uint32_t colorHi = code[ip++];
            uint32_t color = _make_u32(colorLo, colorHi);
            if(!cur.W) return _bad(err, "Multiply requires base image");
            _multiply(cur, color);
          } break;

          case Op::Screen: {
            if(!need(2)) return false;
            uint32_t colorLo = code[ip++];
            uint32_t colorHi = code[ip++];
            uint32_t color = _make_u32(colorLo, colorHi);
            if(!cur.W) return _bad(err, "Screen requires base image");
            _screen(cur, color);
          } break;

          case Op::Colorize: {
            if(!need(3)) return false;
            uint32_t colorLo = code[ip++];
            uint32_t colorHi = code[ip++];
            uint32_t color = _make_u32(colorLo, colorHi);
            uint32_t ratio = code[ip++] & 0xFFu;
            if(!cur.W) return _bad(err, "Colorize requires base image");
            _colorize(cur, color, uint8_t(ratio));
          } break;

          case Op::Combine: {
            // variable length:
            // w,h,n then for each: x,y, SrcRef(3)
            if(!need(3)) return false;
            uint32_t w = code[ip++], h = code[ip++], n = code[ip++];
            Image outImg; outImg.W=w; outImg.H=h; outImg.Px.assign(size_t(w)*size_t(h), 0u);
            for(uint32_t i=0;i<n;i++){
              if(!need(2+3)) return false;
              int x = int(code[ip++]);
              int y = int(code[ip++]);
              SrcRef src = _readSrc(code, ip);
              Image part = _loadSrc(code, src, texCache, subCache, err);
              if(part.W == 0) return false;
              _overlayAt(outImg, part, x, y);
            }
            cur = std::move(outImg);
          } break;

          default:
            return _bad(err, "Unknown opcode (no skip table in this minimal VM)");
        }
      }

      out.Width = cur.W;
      out.Height = cur.H;
      out.Pixels = std::move(cur.Px);
      return true;
    }

  private:
    TextureProvider Provider_;

    static bool _bad(std::string* err, const char* msg){ if(err) *err = msg; return false; }

    static SrcRef _readSrc(const std::vector<Word>& code, size_t& ip) {
      SrcRef r;
      r.Kind = static_cast<SrcKind>(code[ip++]);
      r.A = code[ip++];
      r.B = code[ip++];
      return r;
    }

    Image _loadTex(uint32_t id, std::unordered_map<uint32_t, Image>& cache, std::string* err) {
      auto it = cache.find(id);
      if(it != cache.end()) return it->second;
      auto t = Provider_(id);
      if(!t || !t->Pixels || !t->Width || !t->Height) {
        if(err) *err = "Texture id not found: " + std::to_string(id);
        return {};
      }
      Image img;
      img.W = t->Width; img.H = t->Height;
      img.Px.assign(t->Pixels, t->Pixels + size_t(img.W)*size_t(img.H));
      cache.emplace(id, img);
      return img;
    }

    Image _loadSub(const std::vector<Word>& code,
                  Word off, Word len,
                  std::unordered_map<uint32_t, Image>& texCache,
                  std::unordered_map<uint32_t, Image>& subCache,
                  std::string* err) {
      uint32_t key = (uint32_t(off) << 16) | uint32_t(len);
      auto it = subCache.find(key);
      if(it != subCache.end()) return it->second;

      size_t start = size_t(off);
      size_t end = start + size_t(len);
      if(end > code.size()) { if(err) *err="Subprogram out of range"; return {}; }

      // Run subprogram slice by copying minimal (simple + safe).
      std::vector<Word> slice(code.begin()+start, code.begin()+end);
      OwnedTexture tmp;
      VM nested(Provider_);
      if(!nested.run(slice, tmp, err)) return {};

      Image img;
      img.W = tmp.Width; img.H = tmp.Height; img.Px = std::move(tmp.Pixels);
      subCache.emplace(key, img);
      return img;
    }

    Image _loadSrc(const std::vector<Word>& code,
                  const SrcRef& src,
                  std::unordered_map<uint32_t, Image>& texCache,
                  std::unordered_map<uint32_t, Image>& subCache,
                  std::string* err) {
      if(src.Kind == SrcKind::TexId) {
        return _loadTex(_make_u32(src.A, src.B), texCache, err);
      }
      if(src.Kind == SrcKind::Sub) {
        return _loadSub(code, src.A, src.B, texCache, subCache, err);
      }
      if(err) *err = "Unknown SrcKind";
      return {};
    }

    // ---- image ops ----
    static Image _makeSolid(uint32_t w, uint32_t h, uint32_t color) {
      Image img; img.W=w; img.H=h;
      img.Px.assign(size_t(w)*size_t(h), color);
      return img;
    }

    static Image _resizeNN(const Image& src, uint32_t nw, uint32_t nh) {
      Image dst; dst.W=nw; dst.H=nh;
      dst.Px.resize(size_t(nw)*size_t(nh));
      for(uint32_t y=0;y<nh;y++){
        uint32_t sy = (uint64_t(y) * src.H) / nh;
        for(uint32_t x=0;x<nw;x++){
          uint32_t sx = (uint64_t(x) * src.W) / nw;
          dst.Px[size_t(y)*nw + x] = src.Px[size_t(sy)*src.W + sx];
        }
      }
      return dst;
    }

    static Image _resizeNN_ifNeeded(Image img, uint32_t w, uint32_t h) {
      if(img.W == w && img.H == h) return img;
      return _resizeNN(img, w, h);
    }

    static void _alphaOver(Image& base, const Image& over) {
      const size_t n = base.Px.size();
      for(size_t i=0;i<n;i++){
        uint32_t b = base.Px[i], o = over.Px[i];
        uint8_t ba=_a(b), br=_r(b), bg=_g(b), bb=_b(b);
        uint8_t oa=_a(o), or_=_r(o), og=_g(o), ob=_b(o);

        uint32_t brp = (uint32_t(br) * ba) / 255;
        uint32_t bgp = (uint32_t(bg) * ba) / 255;
        uint32_t bbp = (uint32_t(bb) * ba) / 255;

        uint32_t orp = (uint32_t(or_) * oa) / 255;
        uint32_t ogp = (uint32_t(og)  * oa) / 255;
        uint32_t obp = (uint32_t(ob)  * oa) / 255;

        uint32_t inv = 255 - oa;
        uint32_t outA  = oa + (uint32_t(ba) * inv) / 255;
        uint32_t outRp = orp + (brp * inv) / 255;
        uint32_t outGp = ogp + (bgp * inv) / 255;
        uint32_t outBp = obp + (bbp * inv) / 255;

        uint8_t outR=0,outG=0,outB=0;
        if(outA) {
          outR = uint8_t(std::min<uint32_t>(255, (outRp * 255) / outA));
          outG = uint8_t(std::min<uint32_t>(255, (outGp * 255) / outA));
          outB = uint8_t(std::min<uint32_t>(255, (outBp * 255) / outA));
        }
        base.Px[i] = _pack(uint8_t(outA), outR, outG, outB);
      }
    }

    static void _overlayAt(Image& dst, const Image& src, int ox, int oy) {
      for(uint32_t y=0;y<src.H;y++){
        int dy = oy + int(y);
        if(dy < 0 || dy >= int(dst.H)) continue;
        for(uint32_t x=0;x<src.W;x++){
          int dx = ox + int(x);
          if(dx < 0 || dx >= int(dst.W)) continue;
          size_t di = size_t(dy)*dst.W + uint32_t(dx);
          uint32_t b = dst.Px[di], o = src.Px[size_t(y)*src.W + x];

          uint8_t ba=_a(b), br=_r(b), bg=_g(b), bb=_b(b);
          uint8_t oa=_a(o), or_=_r(o), og=_g(o), ob=_b(o);

          uint32_t brp = (uint32_t(br) * ba) / 255;
          uint32_t bgp = (uint32_t(bg) * ba) / 255;
          uint32_t bbp = (uint32_t(bb) * ba) / 255;

          uint32_t orp = (uint32_t(or_) * oa) / 255;
          uint32_t ogp = (uint32_t(og)  * oa) / 255;
          uint32_t obp = (uint32_t(ob)  * oa) / 255;

          uint32_t inv = 255 - oa;
          uint32_t outA  = oa + (uint32_t(ba) * inv) / 255;
          uint32_t outRp = orp + (brp * inv) / 255;
          uint32_t outGp = ogp + (bgp * inv) / 255;
          uint32_t outBp = obp + (bbp * inv) / 255;

          uint8_t outR=0,outG=0,outB=0;
          if(outA) {
            outR = uint8_t(std::min<uint32_t>(255, (outRp * 255) / outA));
            outG = uint8_t(std::min<uint32_t>(255, (outGp * 255) / outA));
            outB = uint8_t(std::min<uint32_t>(255, (outBp * 255) / outA));
          }
          dst.Px[di] = _pack(uint8_t(outA), outR, outG, outB);
        }
      }
    }

    static void _applyMask(Image& base, const Image& mask) {
      const size_t n = base.Px.size();
      for(size_t i=0;i<n;i++){
        uint32_t b = base.Px[i], m = mask.Px[i];
        uint8_t outA = uint8_t((uint32_t(_a(b)) * uint32_t(_a(m))) / 255);
        base.Px[i] = _pack(outA, _r(b), _g(b), _b(b));
      }
    }

    static void _opacity(Image& img, uint8_t mul) {
      for(auto& p : img.Px) {
        uint8_t na = uint8_t((uint32_t(_a(p)) * mul) / 255);
        p = _pack(na, _r(p), _g(p), _b(p));
      }
    }
    static void _noAlpha(Image& img) {
      for(auto& p : img.Px) p = _pack(255, _r(p), _g(p), _b(p));
    }
    static void _makeAlpha(Image& img, uint32_t rgb24) {
      uint8_t rr = uint8_t((rgb24 >> 16) & 0xFF);
      uint8_t gg = uint8_t((rgb24 >>  8) & 0xFF);
      uint8_t bb = uint8_t((rgb24 >>  0) & 0xFF);
      for(auto& p : img.Px) {
        if(_r(p)==rr && _g(p)==gg && _b(p)==bb) p = _pack(0, _r(p), _g(p), _b(p));
      }
    }
    static void _invert(Image& img, uint32_t maskBits) {
      for(auto& p : img.Px) {
        uint8_t a=_a(p), r=_r(p), g=_g(p), b=_b(p);
        if(maskBits & 1u) r = 255 - r;
        if(maskBits & 2u) g = 255 - g;
        if(maskBits & 4u) b = 255 - b;
        if(maskBits & 8u) a = 255 - a;
        p = _pack(a,r,g,b);
      }
    }
    static void _brighten(Image& img) {
      for(auto& p : img.Px) {
        int r = _r(p), g = _g(p), b = _b(p);
        r = r + (255 - r) / 3;
        g = g + (255 - g) / 3;
        b = b + (255 - b) / 3;
        p = _pack(_a(p), _clampu8(r), _clampu8(g), _clampu8(b));
      }
    }
    static void _contrast(Image& img, int c, int br) {
      double C = double(std::max(-127, std::min(127, c)));
      double factor = (259.0 * (C + 255.0)) / (255.0 * (259.0 - C));
      for(auto& p : img.Px) {
        int r = int(factor * (int(_r(p)) - 128) + 128) + br;
        int g = int(factor * (int(_g(p)) - 128) + 128) + br;
        int b = int(factor * (int(_b(p)) - 128) + 128) + br;
        p = _pack(_a(p), _clampu8(r), _clampu8(g), _clampu8(b));
      }
    }
    static void _multiply(Image& img, uint32_t color) {
      uint8_t cr=_r(color), cg=_g(color), cb=_b(color);
      for(auto& p : img.Px) {
        uint8_t r = uint8_t((uint32_t(_r(p)) * cr) / 255);
        uint8_t g = uint8_t((uint32_t(_g(p)) * cg) / 255);
        uint8_t b = uint8_t((uint32_t(_b(p)) * cb) / 255);
        p = _pack(_a(p), r,g,b);
      }
    }
    static void _screen(Image& img, uint32_t color) {
      uint8_t cr=_r(color), cg=_g(color), cb=_b(color);
      for(auto& p : img.Px) {
        uint8_t r = uint8_t(255 - ((255 - _r(p)) * (255 - cr)) / 255);
        uint8_t g = uint8_t(255 - ((255 - _g(p)) * (255 - cg)) / 255);
        uint8_t b = uint8_t(255 - ((255 - _b(p)) * (255 - cb)) / 255);
        p = _pack(_a(p), r,g,b);
      }
    }
    static void _colorize(Image& img, uint32_t color, uint8_t ratio) {
      uint8_t cr=_r(color), cg=_g(color), cb=_b(color);
      for(auto& p : img.Px) {
        int r = (int(_r(p)) * (255 - ratio) + int(cr) * ratio) / 255;
        int g = (int(_g(p)) * (255 - ratio) + int(cg) * ratio) / 255;
        int b = (int(_b(p)) * (255 - ratio) + int(cb) * ratio) / 255;
        p = _pack(_a(p), uint8_t(r), uint8_t(g), uint8_t(b));
      }
    }
    static void _lowpart(Image& base, const Image& over, uint32_t percent) {
      uint32_t startY = base.H - (base.H * percent) / 100;
      for(uint32_t y=startY; y<base.H; y++){
        for(uint32_t x=0; x<base.W; x++){
          size_t i = size_t(y)*base.W + x;
          // overlay one pixel
          uint32_t b = base.Px[i], o = over.Px[i];
          uint8_t ba=_a(b), br=_r(b), bg=_g(b), bb=_b(b);
          uint8_t oa=_a(o), or_=_r(o), og=_g(o), ob=_b(o);

          uint32_t brp = (uint32_t(br) * ba) / 255;
          uint32_t bgp = (uint32_t(bg) * ba) / 255;
          uint32_t bbp = (uint32_t(bb) * ba) / 255;

          uint32_t orp = (uint32_t(or_) * oa) / 255;
          uint32_t ogp = (uint32_t(og)  * oa) / 255;
          uint32_t obp = (uint32_t(ob)  * oa) / 255;

          uint32_t inv = 255 - oa;
          uint32_t outA  = oa + (uint32_t(ba) * inv) / 255;
          uint32_t outRp = orp + (brp * inv) / 255;
          uint32_t outGp = ogp + (bgp * inv) / 255;
          uint32_t outBp = obp + (bbp * inv) / 255;

          uint8_t outR=0,outG=0,outB=0;
          if(outA) {
            outR = uint8_t(std::min<uint32_t>(255, (outRp * 255) / outA));
            outG = uint8_t(std::min<uint32_t>(255, (outGp * 255) / outA));
            outB = uint8_t(std::min<uint32_t>(255, (outBp * 255) / outA));
          }
          base.Px[i] = _pack(uint8_t(outA), outR, outG, outB);
        }
      }
    }

    static Image _transform(const Image& src, uint32_t t) {
      Image dst;
      auto at = [&](uint32_t x, uint32_t y)->uint32_t { return src.Px[size_t(y)*src.W + x]; };
      auto make = [&](uint32_t w, uint32_t h){
        Image d; d.W=w; d.H=h; d.Px.resize(size_t(w)*size_t(h));
        return d;
      };
      auto set = [&](Image& im, uint32_t x, uint32_t y, uint32_t v){
        im.Px[size_t(y)*im.W + x] = v;
      };

      switch (t & 7u) {
        case 0: return src;
        case 1: { dst = make(src.H, src.W);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(y, src.H-1-x));
        } break;
        case 2: { dst = make(src.W, src.H);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(src.W-1-x, src.H-1-y));
        } break;
        case 3: { dst = make(src.H, src.W);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(src.W-1-y, x));
        } break;
        case 4: { dst = make(src.W, src.H);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(src.W-1-x, y));
        } break;
        case 5: { dst = make(src.H, src.W);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(src.H-1-y, src.H-1-x));
        } break;
        case 6: { dst = make(src.W, src.H);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(x, src.H-1-y));
        } break;
        case 7: { dst = make(src.H, src.W);
          for(uint32_t y=0;y<dst.H;y++) for(uint32_t x=0;x<dst.W;x++)
            set(dst, x,y, at(y, x));
        } break;
      }
      return dst;
    }
  };

  // ========================
  // Minimal DSL Lexer/Parser
  // Supports:
  //   tex "name" |> op(args...)
  //   tex 32x32 "#RRGGBBAA" |> ...
  // Grouping (subprogram) only where an op expects a texture arg:
  //   overlay( tex "b" |> ... )
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

    static bool isAlpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
    static bool isNum(char c){ return (c>='0'&&c<='9'); }
    static bool isAlnum(char c){ return isAlpha(c)||isNum(c); }

    void skipWs() {
      while (I < S.size()) {
        char c = S[I];
        if(c==' '||c=='\t'||c=='\r'||c=='\n'){ I++; continue; }
        if(c=='#'){ while (I<S.size() && S[I]!='\n') I++; continue; } // line comment
        if(c=='/' && I+1<S.size() && S[I+1]=='/'){ I+=2; while (I<S.size() && S[I]!='\n') I++; continue; }
        break;
      }
    }

    Tok next() {
      skipWs();
      if(I >= S.size()) return {TokKind::End, {}, 0};

      if(S[I]=='|' && I+1<S.size() && S[I+1]=='>') { I+=2; return {TokKind::Pipe, "|>",0}; }
      char c = S[I];
      if(c==','){ I++; return {TokKind::Comma,",",0}; }
      if(c=='('){ I++; return {TokKind::LParen,"(",0}; }
      if(c==')'){ I++; return {TokKind::RParen,")",0}; }
      if(c=='='){ I++; return {TokKind::Eq,"=",0}; }
      if(c=='x' || c=='X'){ I++; return {TokKind::X,"x",0}; }

      if(c=='"') {
        I++;
        std::string out;
        while (I < S.size()) {
          char ch = S[I++];
          if(ch=='"') break;
          if(ch=='\\' && I<S.size()) {
            char esc = S[I++];
            switch (esc) {
              case 'n': out.push_back('\n'); break;
              case 't': out.push_back('\t'); break;
              case '"': out.push_back('"');  break;
              case '\\':out.push_back('\\'); break;
              default: out.push_back(esc); break;
            }
          } else out.push_back(ch);
        }
        return {TokKind::String, std::move(out), 0};
      }

      if(isNum(c)) {
        uint64_t v=0;
        size_t start=I;
        while (I<S.size() && isNum(S[I])) { v = v*10 + uint64_t(S[I]-'0'); I++; }
        return {TokKind::Number, std::string(S.substr(start, I-start)), uint32_t(v)};
      }

      if(isAlpha(c) || c=='#') {
        size_t start=I;
        I++;
        while (I<S.size() && (isAlnum(S[I]) || S[I]=='.' || S[I]=='#')) I++;
        return {TokKind::Ident, std::string(S.substr(start, I-start)), 0};
      }

      I = S.size();
      return {TokKind::End, {}, 0};
    }
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
    // For ops that accept texture expression, we allow first positional arg to be "subexpr marker"
    // but we handle that at compile-time by parsing texture expr inside parentheses.
  };

  // ========================
  // Compiler state
  // ========================
  std::string Source_;
  std::vector<Word> Code_;
  std::vector<Patch> Patches_;

  // ---- _emit helpers ----
  void _emit(Op op) { Code_.push_back(Word(op)); }
  void _emitW(uint32_t v) { Code_.push_back(Word(v & 0xFFFFu)); }
  void _emitU32(uint32_t v) { Code_.push_back(_lo16(v)); Code_.push_back(_hi16(v)); }

  void _emitTexRefName(const std::string& name) {
    // reserve lo+hi for uint32 texId
    size_t lo = Code_.size();
    Code_.push_back(0);
    Code_.push_back(0);
    Patches_.push_back(Patch{lo, name});
  }

  void _emitSrcRef(const SrcRef& r) {
    Code_.push_back(Word(r.Kind));
    Code_.push_back(r.A);
    Code_.push_back(r.B);
  }

  // ========================
  // Color parsing: #RRGGBB or #RRGGBBAA
  // Stored as 0xAARRGGBB
  // ========================
  static bool _parseHexColor(std::string_view s, uint32_t& outARGB) {
    if(s.size()!=7 && s.size()!=9) return false;
    if(s[0] != '#') return false;
    auto hex = [](char c)->int{
      if(c>='0'&&c<='9') return c-'0';
      if(c>='a'&&c<='f') return 10+(c-'a');
      if(c>='A'&&c<='F') return 10+(c-'A');
      return -1;
    };
    auto byteAt = [&](size_t idx)->std::optional<uint8_t>{
      int hi=hex(s[idx]), lo=hex(s[idx+1]);
      if(hi<0||lo<0) return std::nullopt;
      return uint8_t((hi<<4)|lo);
    };
    auto r = byteAt(1), g = byteAt(3), b = byteAt(5);
    if(!r||!g||!b) return false;
    uint8_t a = 255;
    if(s.size()==9) {
      auto aa = byteAt(7);
      if(!aa) return false;
      a = *aa;
    }
    outARGB = (uint32_t(a)<<24) | (uint32_t(*r)<<16) | (uint32_t(*g)<<8) | (uint32_t(*b));
    return true;
  }

  // ========================
  // Parsing entry: full program
  // ========================
  bool _parseProgram(std::string* err) {
    Lexer lx{Source_};
    Tok t = lx.next();
    if(!(t.Kind==TokKind::Ident && t.Text=="tex")) {
      if(err) *err="Expected 'tex' at start";
      return false;
    }

    // Parse base expression after tex:
    //   1) "name"
    //   2) Number X Number Ident(color)
    //   3) (future) png("...")
    Tok a = lx.next();

    if(a.Kind == TokKind::String || a.Kind == TokKind::Ident) {
      // tex "name.png"
      _emit(Op::Base_Tex);
      // SrcRef(TexId): kind + id(lo/hi)
      Code_.push_back(Word(SrcKind::TexId));
      _emitTexRefName(a.Text); // lo+hi patched later
    } else if(a.Kind == TokKind::Number) {
      // tex 32x32 "#RRGGBBAA"
      Tok xTok = lx.next();
      Tok b = lx.next();
      Tok colTok = lx.next();
      if(xTok.Kind != TokKind::X || b.Kind != TokKind::Number || (colTok.Kind!=TokKind::Ident && colTok.Kind!=TokKind::String)) {
        if(err) *err="Expected: tex <w>x<h> <#color>";
        return false;
      }
      uint32_t w = a.U32, h = b.U32;
      uint32_t color = 0;
      if(!_parseHexColor(colTok.Text, color)) {
        if(err) *err="Bad color literal (use #RRGGBB or #RRGGBBAA)";
        return false;
      }
      if(w>65535u || h>65535u) { if(err) *err="w/h must fit in uint16"; return false; }
      _emit(Op::Base_Fill);
      _emitW(w); _emitW(h);
      _emitU32(color);
    } else {
      if(err) *err="Bad 'tex' base expression";
      return false;
    }

    // pipeline: |> op ...
    Tok nt = lx.next();
    while (nt.Kind == TokKind::Pipe) {
      Tok opName = lx.next();
      if(opName.Kind != TokKind::Ident) { if(err) *err="Expected op name after |>"; return false; }
      ParsedOp op;
      op.Name = opName.Text;

      Tok peek = lx.next();
      if(peek.Kind == TokKind::LParen) {
        if(!_parseArgListOrTextureExpr(lx, op, err)) return false;
        nt = lx.next();
      } else {
        // no-arg op (like brighten) must be followed by next |> or end
        nt = peek;
      }

      if(!_compileOp(lx, op, err)) return false;
    }

    _emit(Op::End);
    return true;
  }

  // Parses either:
  //  - normal args list: (a,b,key=v)
  //  - OR for ops that take texture, allow: ( tex ... |> ... ) as the *first* positional "special"
  bool _parseArgListOrTextureExpr(Lexer& lx, ParsedOp& op, std::string* err) {
    // Lookahead: if next token is 'tex' => parse sub texture expression until ')'
    Tok first = lx.next();
    if(first.Kind==TokKind::Ident && first.Text=="tex") {
      // We parse a full texture expression (starting after 'tex') into a subprogram bytecode vector.
      // We'll store a marker in op.Named["_subtex"] with special string "<compiled later>"
      // But easier: store the subprogram words immediately as a pseudo-arg in op.Pos[0].S = "<SUB>"
      ArgVal av; av.Kind = ArgVal::ValueKind::Ident; av.S = "__SUBTEX__";
      op.Pos.push_back(std::move(av));

      // compile subprogram into vector<Word> sub
      std::vector<Word> sub;
      if(!_compileSubProgramFromAlreadySawTex(lx, sub, err)) return false;

      // Expect ')'
      Tok end = lx.next();
      if(end.Kind != TokKind::RParen) { if(err) *err="Expected ')' after sub texture expr"; return false; }

      // Stash the subprogram into an internal buffer attached to this op (hack: store in a map)
      PendingSub_[&op] = std::move(sub);
      return true;
    }

    // Otherwise parse normal arg list, where `first` is first token inside '('
    Tok t = first;
    if(t.Kind == TokKind::RParen) return true;

    while (true) {
      if(t.Kind == TokKind::Ident) {
        Tok maybeEq = lx.next();
        if(maybeEq.Kind == TokKind::Eq) {
          Tok v = lx.next();
          ArgVal av;
          if(!_tokToVal(v, av, err)) return false;
          op.Named[t.Text] = std::move(av);
          t = lx.next();
        } else {
          ArgVal av; av.Kind = ArgVal::ValueKind::Ident; av.S = t.Text;
          op.Pos.push_back(std::move(av));
          t = maybeEq;
        }
      } else {
        ArgVal av;
        if(!_tokToVal(t, av, err)) return false;
        op.Pos.push_back(std::move(av));
        t = lx.next();
      }

      if(t.Kind == TokKind::Comma) { t = lx.next(); continue; }
      if(t.Kind == TokKind::RParen) return true;

      if(err) *err = "Expected ',' or ')' in argument list";
      return false;
    }
  }

  bool _tokToVal(const Tok& t, ArgVal& out, std::string* err) {
    if(t.Kind == TokKind::Number) { out.Kind=ArgVal::ValueKind::U32; out.U32=t.U32; return true; }
    if(t.Kind == TokKind::String) { out.Kind=ArgVal::ValueKind::Str; out.S=t.Text; return true; }
    if(t.Kind == TokKind::Ident)  { out.Kind=ArgVal::ValueKind::Ident; out.S=t.Text; return true; }
    if(err) *err = "Expected value token";
    return false;
  }

  // ========================
  // Subprogram compilation
  // We already consumed 'tex' token. Now we parse base + pipeline until we hit ')'
  // Strategy:
  //  - compile into `sub` vector<Word>
  //  - stop when next token would be ')'
  //  - do NOT consume ')'
  // ========================
  bool _compileSubProgramFromAlreadySawTex(Lexer& lx, std::vector<Word>& sub, std::string* err) {
    // We reuse a mini-compiler that writes into `sub` instead of Code_
    auto emitS  = [&](Op op){ sub.push_back(Word(op)); };
    auto emitSW = [&](uint32_t v){ sub.push_back(Word(v & 0xFFFFu)); };
    auto emitSU32 = [&](uint32_t v){ sub.push_back(_lo16(v)); sub.push_back(_hi16(v)); };
    auto emitSTexName = [&](const std::string& name){
      // IMPORTANT: patches must point into main Code_, not sub.
      // Solution: subprogram words are appended into main Code_ later, so we can patch after append.
      // Here we place placeholder lo/hi and store a *relative patch* into SubPatchesTemp_.
      size_t lo = sub.size();
      sub.push_back(0); sub.push_back(0);
      SubPatchesTemp_.push_back({lo, name}); // relative to sub start
    };

    Tok a = lx.next();
    if(a.Kind == TokKind::String || a.Kind == TokKind::Ident) {
      emitS(Op::Base_Tex);
      sub.push_back(Word(SrcKind::TexId));
      emitSTexName(a.Text);
    } else if(a.Kind == TokKind::Number) {
      Tok xTok = lx.next();
      Tok b = lx.next();
      Tok colTok = lx.next();
      if(xTok.Kind != TokKind::X || b.Kind != TokKind::Number || (colTok.Kind!=TokKind::Ident && colTok.Kind!=TokKind::String)) {
        if(err) *err="Sub tex: expected <w>x<h> <#color>";
        return false;
      }
      uint32_t w = a.U32, h = b.U32;
      uint32_t color=0;
      if(!_parseHexColor(colTok.Text, color)) { if(err) *err="Sub tex: bad color"; return false; }
      if(w>65535u || h>65535u) { if(err) *err="Sub tex: w/h must fit uint16"; return false; }
      emitS(Op::Base_Fill);
      emitSW(w); emitSW(h);
      emitSU32(color);
    } else {
      if(err) *err="Sub tex: bad base";
      return false;
    }

    // Pipeline until we see ')' lookahead (we can’t unread, so we detect by peeking in a copy)
    while (true) {
      // Peek next non-ws token without consuming by copying lexer
      Lexer peek = lx;
      Tok nt = peek.next();
      if(nt.Kind == TokKind::RParen) break;
      if(nt.Kind != TokKind::Pipe) { if(err) *err="Sub tex: expected '|>' or ')'"; return false; }
      // consume pipe
      lx.next();
      Tok opName = lx.next();
      if(opName.Kind != TokKind::Ident) { if(err) *err="Sub tex: expected op name"; return false; }
      ParsedOp op; op.Name = opName.Text;

      Tok lp = lx.next();
      if(lp.Kind == TokKind::LParen) {
        if(!_parseArgListOrTextureExpr(lx, op, err)) return false;
      } else {
        // no-arg op
      }

      // compile op into `sub` by temporarily swapping buffers
      if(!_compileOpInto(lx, op, sub, emitS, emitSW, emitSU32, emitSTexName, err)) return false;
    }

    emitS(Op::End);
    return true;
  }

  // Temporary relative patches inside subprogram being built
  struct RelPatch { size_t RelLo; std::string Name; };
  mutable std::vector<RelPatch> SubPatchesTemp_;

  // Stash compiled subprogram per op pointer (simplifies this one-file example)
  mutable std::unordered_map<const ParsedOp*, std::vector<Word>> PendingSub_;

  // Append a subprogram to main Code_, returning SrcRef(Sub, offset,len) and migrating its patches
  SrcRef _appendSubprogram(std::vector<Word>&& sub) {
    // offset/len must fit u16
    size_t offset = Code_.size();
    size_t len = sub.size();

    // migrate relative patches -> absolute patches into main Code_
    // Each rel patch points to lo word within sub vector.
    for(const auto& rp : SubPatchesTemp_) {
      size_t absLo = offset + rp.RelLo;
      Patches_.push_back(Patch{absLo, rp.Name});
    }
    SubPatchesTemp_.clear();

    Code_.insert(Code_.end(), sub.begin(), sub.end());

    SrcRef r;
    r.Kind = SrcKind::Sub;
    r.A = Word(offset & 0xFFFFu);
    r.B = Word(len & 0xFFFFu);
    return r;
  }

  // ========================
  // compile operations
  // ========================
  bool _compileOp(Lexer& lx, const ParsedOp& op, std::string* err) {
    // Normal compile into main Code_
    auto it = PendingSub_.find(&op);
    const bool hasSub = (it != PendingSub_.end());
    return _compileOpInto(
      lx, op, Code_,
      [&](Op o){ _emit(o); },
      [&](uint32_t v){ _emitW(v); },
      [&](uint32_t v){ _emitU32(v); },
      [&](const std::string& name){ _emitTexRefName(name); },
      err,
      hasSub ? &it->second : nullptr
    );
  }

  // Core compiler that can target either main `Code_` or a `sub` vector.
  template <class EmitOp, class EmitWFn, class EmitU32Fn, class EmitTexNameFn>
  bool _compileOpInto(Lexer& /*lx*/,
                     const ParsedOp& op,
                     std::vector<Word>& out,
                     EmitOp emitOpFn,
                     EmitWFn emitWFn,
                     EmitU32Fn emitU32Fn,
                     EmitTexNameFn emitTexNameFn,
                     std::string* err,
                     std::vector<Word>* pendingSub = nullptr) {
    auto posU = [&](size_t i)->std::optional<uint32_t>{
      if(i >= op.Pos.size()) return std::nullopt;
      if(op.Pos[i].Kind != ArgVal::ValueKind::U32) return std::nullopt;
      return op.Pos[i].U32;
    };
    auto posS = [&](size_t i)->std::optional<std::string>{
      if(i >= op.Pos.size()) return std::nullopt;
      return op.Pos[i].S;
    };
    auto namedU = [&](std::string_view k)->std::optional<uint32_t>{
      auto it = op.Named.find(std::string(k));
      if(it==op.Named.end() || it->second.Kind!=ArgVal::ValueKind::U32) return std::nullopt;
      return it->second.U32;
    };
    auto namedS = [&](std::string_view k)->std::optional<std::string>{
      auto it = op.Named.find(std::string(k));
      if(it==op.Named.end()) return std::nullopt;
      return it->second.S;
    };

    auto emitSrcTexName = [&](const std::string& texName){
      // SrcRef(TexId): kind + id(lo/hi)
      out.push_back(Word(SrcKind::TexId));
      emitTexNameFn(texName);
    };

    auto emitSrcFromPendingSub = [&]()->bool{
      if(!pendingSub) { if(err) *err="Internal: missing subprogram"; return false; }
      // move pendingSub into main Code_ ONLY (grouping only makes sense there)
      // If we're compiling inside a subprogram and we see another nested subprogram,
      // this demo keeps it simple: it will still append into the *same* vector (out),
      // so we can just inline by "append here". For production, you likely want a
      // separate sub-table or a more structured approach.
      //
      // For simplicity: we append nested subprogram right into `out` and reference it by offset/len.
      size_t offset = out.size();
      size_t len = pendingSub->size();
      out.insert(out.end(), pendingSub->begin(), pendingSub->end());
      out.push_back(Word(SrcKind::Sub));
      out.push_back(Word(offset & 0xFFFFu));
      out.push_back(Word(len & 0xFFFFu));
      return true;
    };

    // --- Ops that accept a "texture" argument: overlay/mask/lowpart/combine parts ---
    if(op.Name == "overlay") {
      emitOpFn(Op::Overlay);
      if(!op.Pos.empty() && op.Pos[0].S == "__SUBTEX__") {
        // Subprogram source
        // In main compile path, we prefer storing subprograms at end and referencing by offset/len.
        // Here we already have the compiled sub in pendingSub; we append + _emit SrcRef(Sub,...).
        // For main program, we use _appendSubprogram() outside; in this generic function we inline.
        return emitSrcFromPendingSub();
      }
      std::string tex = namedS("tex").value_or(posS(0).value_or(""));
      if(tex.empty()) { if(err) *err="overlay requires texture arg"; return false; }
      emitSrcTexName(tex);
      return true;
    }

    if(op.Name == "mask") {
      emitOpFn(Op::Mask);
      if(!op.Pos.empty() && op.Pos[0].S == "__SUBTEX__") return emitSrcFromPendingSub();
      std::string tex = namedS("tex").value_or(posS(0).value_or(""));
      if(tex.empty()) { if(err) *err="mask requires texture arg"; return false; }
      emitSrcTexName(tex);
      return true;
    }

    if(op.Name == "lowpart") {
      uint32_t pct = namedU("percent").value_or(posU(0).value_or(0));
      if(!pct) { if(err) *err="lowpart requires percent"; return false; }
      emitOpFn(Op::LowPart);
      emitWFn(std::min<uint32_t>(100u, pct));
      if(op.Pos.size() >= 2 && op.Pos[1].S == "__SUBTEX__") return emitSrcFromPendingSub();
      std::string tex = namedS("tex").value_or(posS(1).value_or(""));
      if(tex.empty()) { if(err) *err="lowpart requires tex"; return false; }
      emitSrcTexName(tex);
      return true;
    }

    // --- Unary ops ---
    if(op.Name == "resize") {
      uint32_t w = namedU("w").value_or(posU(0).value_or(0));
      uint32_t h = namedU("h").value_or(posU(1).value_or(0));
      if(!w || !h || w>65535u || h>65535u) { if(err) *err="resize(w,h) must fit uint16"; return false; }
      emitOpFn(Op::Resize); emitWFn(w); emitWFn(h);
      return true;
    }

    if(op.Name == "transform") {
      uint32_t t = namedU("t").value_or(posU(0).value_or(0));
      emitOpFn(Op::Transform); emitWFn(t & 7u);
      return true;
    }

    if(op.Name == "opacity") {
      uint32_t a = namedU("a").value_or(posU(0).value_or(255));
      emitOpFn(Op::Opacity); emitWFn(a & 0xFFu);
      return true;
    }

    if(op.Name == "remove_alpha" || op.Name == "noalpha") {
      emitOpFn(Op::NoAlpha);
      return true;
    }

    if(op.Name == "make_alpha") {
      std::string col = namedS("color").value_or(posS(0).value_or(""));
      uint32_t argb=0;
      if(!_parseHexColor(col, argb)) { if(err) *err="make_alpha requires color #RRGGBB"; return false; }
      uint32_t rgb24 = argb & 0x00FFFFFFu;
      // pack rgb24 into two u16: lo=0xRRGG, hi=0x00BB
      emitOpFn(Op::MakeAlpha);
      emitWFn((rgb24 >> 8) & 0xFFFFu); // RR GG
      emitWFn(rgb24 & 0x00FFu);        // BB
      return true;
    }

    if(op.Name == "invert") {
      std::string ch = namedS("channels").value_or(posS(0).value_or("rgb"));
      uint32_t mask=0;
      for(char c : ch) {
        if(c=='r') mask |= 1;
        if(c=='g') mask |= 2;
        if(c=='b') mask |= 4;
        if(c=='a') mask |= 8;
      }
      emitOpFn(Op::Invert); emitWFn(mask);
      return true;
    }

    if(op.Name == "brighten") {
      emitOpFn(Op::Brighten);
      return true;
    }

    if(op.Name == "contrast") {
      int c = int(namedU("value").value_or(posU(0).value_or(0)));
      int b = int(namedU("brightness").value_or(posU(1).value_or(0)));
      c = std::max(-127, std::min(127, c));
      b = std::max(-127, std::min(127, b));
      emitOpFn(Op::Contrast);
      emitWFn(uint32_t(c + 127));
      emitWFn(uint32_t(b + 127));
      return true;
    }

    auto compileColorOp = [&](Op opcode, bool needsRatio)->bool{
      std::string col = namedS("color").value_or(posS(0).value_or(""));
      uint32_t argb=0;
      if(!_parseHexColor(col, argb)) { if(err) *err="Bad color literal"; return false; }
      emitOpFn(opcode);
      emitU32Fn(argb);
      if(needsRatio) {
        uint32_t ratio = namedU("ratio").value_or(posU(1).value_or(255));
        emitWFn(ratio & 0xFFu);
      }
      return true;
    };

    if(op.Name == "multiply") return compileColorOp(Op::Multiply, false);
    if(op.Name == "screen")   return compileColorOp(Op::Screen, false);
    if(op.Name == "colorize") return compileColorOp(Op::Colorize, true);

    if(err) *err = "Unknown op: " + op.Name;
    return false;
  }
};
