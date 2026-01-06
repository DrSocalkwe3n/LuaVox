#include "Common/TexturePipelineProgram.hpp"

bool TexturePipelineProgram::compile(std::string_view src, std::string* err) {
  Source_ = src;
  Code_.clear();
  Patches_.clear();
  PendingSub_.clear();
  return _parseProgram(err);
}

bool TexturePipelineProgram::link(const IdResolverFunc& resolver, std::string* err) {
  for (const auto& p : Patches_) {
    auto idOpt = resolver(p.Name);
    if(!idOpt) {
      if(err) *err = "Не удалось разрешить имя текстуры: " + p.Name;
      return false;
    }
    uint32_t id = *idOpt;
    if(id >= (1u << 24)) {
      if(err) *err = "TexId выходит за 24 бита (u24): " + p.Name + " => " + std::to_string(id);
      return false;
    }
    if(p.ByteIndex0 + 2 >= Code_.size()) {
      if(err) *err = "Внутренняя ошибка: применение идентификатора выходит за рамки кода";
      return false;
    }
    Code_[p.ByteIndex0 + 0] = uint8_t(id & 0xFFu);
    Code_[p.ByteIndex0 + 1] = uint8_t((id >> 8) & 0xFFu);
    Code_[p.ByteIndex0 + 2] = uint8_t((id >> 16) & 0xFFu);
  }
  return true;
}

bool TexturePipelineProgram::bake(const TextureProviderFunc& provider, OwnedTexture& out, std::string* err) const {
  return bake(provider, out, 0.0, err);
}

bool TexturePipelineProgram::bake(const TextureProviderFunc& provider, OwnedTexture& out, double timeSeconds, std::string* err) const {
  VM vm(provider);
  return vm.run(Code_, out, timeSeconds, err);
}

std::vector<TexturePipelineProgram::AnimSpec> TexturePipelineProgram::extractAnimationSpecs(const Word* code, size_t size) {
  std::vector<AnimSpec> specs;
  if(!code || size == 0) {
    return specs;
  }

  struct Range {
    size_t Start = 0;
    size_t End = 0;
  };

  std::vector<Range> visited;

  auto read8 = [&](size_t& ip, uint8_t& out)->bool{
    if(ip >= size) return false;
    out = code[ip++];
    return true;
  };
  auto read16 = [&](size_t& ip, uint16_t& out)->bool{
    if(ip + 1 >= size) return false;
    out = uint16_t(code[ip]) | (uint16_t(code[ip + 1]) << 8);
    ip += 2;
    return true;
  };
  auto read24 = [&](size_t& ip, uint32_t& out)->bool{
    if(ip + 2 >= size) return false;
    out = uint32_t(code[ip]) |
          (uint32_t(code[ip + 1]) << 8) |
          (uint32_t(code[ip + 2]) << 16);
    ip += 3;
    return true;
  };
  auto read32 = [&](size_t& ip, uint32_t& out)->bool{
    if(ip + 3 >= size) return false;
    out = uint32_t(code[ip]) |
          (uint32_t(code[ip + 1]) << 8) |
          (uint32_t(code[ip + 2]) << 16) |
          (uint32_t(code[ip + 3]) << 24);
    ip += 4;
    return true;
  };

  struct SrcMeta {
    SrcKind Kind = SrcKind::TexId;
    uint32_t TexId = 0;
    uint32_t Off = 0;
    uint32_t Len = 0;
  };

  auto readSrc = [&](size_t& ip, SrcMeta& out)->bool{
    uint8_t kind = 0;
    if(!read8(ip, kind)) return false;
    out.Kind = static_cast<SrcKind>(kind);
    if(out.Kind == SrcKind::TexId) {
      return read24(ip, out.TexId);
    }
    if(out.Kind == SrcKind::Sub) {
      return read24(ip, out.Off) && read24(ip, out.Len);
    }
    return false;
  };

  auto scan = [&](auto&& self, size_t start, size_t end) -> void {
    if(start >= end || end > size) {
      return;
    }
    for(const auto& r : visited) {
      if(r.Start == start && r.End == end) {
        return;
      }
    }
    visited.push_back(Range{start, end});

    size_t ip = start;
    while(ip < end) {
      uint8_t opByte = 0;
      if(!read8(ip, opByte)) return;
      Op op = static_cast<Op>(opByte);
      switch(op) {
        case Op::End:
          return;

        case Op::Base_Tex: {
          SrcMeta src{};
          if(!readSrc(ip, src)) return;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(subStart < subEnd && subEnd <= size) {
              self(self, subStart, subEnd);
            }
          }
        } break;

        case Op::Base_Fill: {
          uint16_t tmp16 = 0;
          uint32_t tmp32 = 0;
          if(!read16(ip, tmp16)) return;
          if(!read16(ip, tmp16)) return;
          if(!read32(ip, tmp32)) return;
        } break;

        case Op::Base_Anim: {
          SrcMeta src{};
          if(!readSrc(ip, src)) return;
          uint16_t frameW = 0;
          uint16_t frameH = 0;
          uint16_t frameCount = 0;
          uint16_t fpsQ = 0;
          uint8_t flags = 0;
          if(!read16(ip, frameW)) return;
          if(!read16(ip, frameH)) return;
          if(!read16(ip, frameCount)) return;
          if(!read16(ip, fpsQ)) return;
          if(!read8(ip, flags)) return;

          if(src.Kind == SrcKind::TexId) {
            AnimSpec spec{};
            spec.TexId = src.TexId;
            spec.HasTexId = true;
            spec.FrameW = frameW;
            spec.FrameH = frameH;
            spec.FrameCount = frameCount;
            spec.FpsQ = fpsQ;
            spec.Flags = flags;
            specs.push_back(spec);
          } else if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(subStart < subEnd && subEnd <= size) {
              self(self, subStart, subEnd);
            }
          }
        } break;

        case Op::Resize: {
          uint16_t tmp16 = 0;
          if(!read16(ip, tmp16)) return;
          if(!read16(ip, tmp16)) return;
        } break;

        case Op::Transform:
        case Op::Opacity:
        case Op::Invert:
          if(!read8(ip, opByte)) return;
          break;

        case Op::NoAlpha:
        case Op::Brighten:
          break;

        case Op::MakeAlpha:
          if(ip + 2 >= size) return;
          ip += 3;
          break;

        case Op::Contrast:
          if(ip + 1 >= size) return;
          ip += 2;
          break;

        case Op::Multiply:
        case Op::Screen: {
          uint32_t tmp32 = 0;
          if(!read32(ip, tmp32)) return;
        } break;

        case Op::Colorize: {
          uint32_t tmp32 = 0;
          if(!read32(ip, tmp32)) return;
          if(!read8(ip, opByte)) return;
        } break;

        case Op::Anim: {
          uint16_t frameW = 0;
          uint16_t frameH = 0;
          uint16_t frameCount = 0;
          uint16_t fpsQ = 0;
          uint8_t flags = 0;
          if(!read16(ip, frameW)) return;
          if(!read16(ip, frameH)) return;
          if(!read16(ip, frameCount)) return;
          if(!read16(ip, fpsQ)) return;
          if(!read8(ip, flags)) return;

          AnimSpec spec{};
          spec.HasTexId = false;
          spec.FrameW = frameW;
          spec.FrameH = frameH;
          spec.FrameCount = frameCount;
          spec.FpsQ = fpsQ;
          spec.Flags = flags;
          specs.push_back(spec);
        } break;

        case Op::Overlay:
        case Op::Mask: {
          SrcMeta src{};
          if(!readSrc(ip, src)) return;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(subStart < subEnd && subEnd <= size) {
              self(self, subStart, subEnd);
            }
          }
        } break;

        case Op::LowPart: {
          if(!read8(ip, opByte)) return;
          SrcMeta src{};
          if(!readSrc(ip, src)) return;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(subStart < subEnd && subEnd <= size) {
              self(self, subStart, subEnd);
            }
          }
        } break;

        case Op::Combine: {
          uint16_t w = 0, h = 0, n = 0;
          if(!read16(ip, w)) return;
          if(!read16(ip, h)) return;
          if(!read16(ip, n)) return;
          for(uint16_t i = 0; i < n; ++i) {
            uint16_t tmp16 = 0;
            if(!read16(ip, tmp16)) return;
            if(!read16(ip, tmp16)) return;
            SrcMeta src{};
            if(!readSrc(ip, src)) return;
            if(src.Kind == SrcKind::Sub) {
              size_t subStart = src.Off;
              size_t subEnd = subStart + src.Len;
              if(subStart < subEnd && subEnd <= size) {
                self(self, subStart, subEnd);
              }
            }
          }
          (void)w; (void)h;
        } break;

        default:
          return;
      }
    }
  };

  scan(scan, 0, size);
  return specs;
}

bool TexturePipelineProgram::remapTexIds(std::vector<uint8_t>& code, const std::vector<uint32_t>& remap, std::string* err) {
  struct Range {
    size_t Start = 0;
    size_t End = 0;
  };

  struct SrcMeta {
    SrcKind Kind = SrcKind::TexId;
    uint32_t TexId = 0;
    uint32_t Off = 0;
    uint32_t Len = 0;
    size_t TexIdOffset = 0;
  };

  const size_t size = code.size();
  std::vector<Range> visited;

  auto read8 = [&](size_t& ip, uint8_t& out)->bool{
    if(ip >= size) return false;
    out = code[ip++];
    return true;
  };
  auto read16 = [&](size_t& ip, uint16_t& out)->bool{
    if(ip + 1 >= size) return false;
    out = uint16_t(code[ip]) | (uint16_t(code[ip + 1]) << 8);
    ip += 2;
    return true;
  };
  auto read24 = [&](size_t& ip, uint32_t& out)->bool{
    if(ip + 2 >= size) return false;
    out = uint32_t(code[ip]) |
          (uint32_t(code[ip + 1]) << 8) |
          (uint32_t(code[ip + 2]) << 16);
    ip += 3;
    return true;
  };
  auto read32 = [&](size_t& ip, uint32_t& out)->bool{
    if(ip + 3 >= size) return false;
    out = uint32_t(code[ip]) |
          (uint32_t(code[ip + 1]) << 8) |
          (uint32_t(code[ip + 2]) << 16) |
          (uint32_t(code[ip + 3]) << 24);
    ip += 4;
    return true;
  };

  auto readSrc = [&](size_t& ip, SrcMeta& out)->bool{
    uint8_t kind = 0;
    if(!read8(ip, kind)) return false;
    out.Kind = static_cast<SrcKind>(kind);
    if(out.Kind == SrcKind::TexId) {
      out.TexIdOffset = ip;
      return read24(ip, out.TexId);
    }
    if(out.Kind == SrcKind::Sub) {
      return read24(ip, out.Off) && read24(ip, out.Len);
    }
    return false;
  };

  auto patchTexId = [&](const SrcMeta& src)->bool{
    if(src.Kind != SrcKind::TexId)
      return true;
    if(src.TexId >= remap.size()) {
      if(err) *err = "Идентификатор текстуры вне диапазона переназначения";
      return false;
    }
    uint32_t newId = remap[src.TexId];
    if(newId >= (1u << 24)) {
      if(err) *err = "TexId выходит за 24 бита (u24)";
      return false;
    }
    if(src.TexIdOffset + 2 >= code.size()) {
      if(err) *err = "Применение идентификатора выходит за рамки кода";
      return false;
    }
    code[src.TexIdOffset + 0] = uint8_t(newId & 0xFFu);
    code[src.TexIdOffset + 1] = uint8_t((newId >> 8) & 0xFFu);
    code[src.TexIdOffset + 2] = uint8_t((newId >> 16) & 0xFFu);
    return true;
  };

  std::move_only_function<bool(size_t, size_t)> scan;
  scan = [&](size_t start, size_t end) -> bool {
    if(start >= end || end > size) {
      return true;
    }
    for(const auto& r : visited) {
      if(r.Start == start && r.End == end) {
        return true;
      }
    }
    visited.push_back(Range{start, end});

    size_t ip = start;
    while(ip < end) {
      uint8_t opByte = 0;
      if(!read8(ip, opByte)) return false;
      Op op = static_cast<Op>(opByte);
      switch(op) {
        case Op::End:
          return true;

        case Op::Base_Tex: {
          SrcMeta src{};
          if(!readSrc(ip, src)) return false;
          if(!patchTexId(src)) return false;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(!scan(subStart, subEnd)) return false;
          }
        } break;

        case Op::Base_Fill: {
          uint16_t tmp16 = 0;
          uint32_t tmp32 = 0;
          if(!read16(ip, tmp16)) return false;
          if(!read16(ip, tmp16)) return false;
          if(!read32(ip, tmp32)) return false;
        } break;

        case Op::Base_Anim: {
          SrcMeta src{};
          if(!readSrc(ip, src)) return false;
          if(!patchTexId(src)) return false;
          uint16_t frameW = 0;
          uint16_t frameH = 0;
          uint16_t frameCount = 0;
          uint16_t fpsQ = 0;
          uint8_t flags = 0;
          if(!read16(ip, frameW)) return false;
          if(!read16(ip, frameH)) return false;
          if(!read16(ip, frameCount)) return false;
          if(!read16(ip, fpsQ)) return false;
          if(!read8(ip, flags)) return false;
          (void)frameW; (void)frameH; (void)frameCount; (void)fpsQ; (void)flags;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(!scan(subStart, subEnd)) return false;
          }
        } break;

        case Op::Resize: {
          uint16_t tmp16 = 0;
          if(!read16(ip, tmp16)) return false;
          if(!read16(ip, tmp16)) return false;
        } break;

        case Op::Transform:
        case Op::Opacity:
        case Op::Invert:
          if(!read8(ip, opByte)) return false;
          break;

        case Op::NoAlpha:
        case Op::Brighten:
          break;

        case Op::MakeAlpha:
          if(ip + 2 >= size) return false;
          ip += 3;
          break;

        case Op::Contrast:
          if(ip + 1 >= size) return false;
          ip += 2;
          break;

        case Op::Multiply:
        case Op::Screen: {
          uint32_t tmp32 = 0;
          if(!read32(ip, tmp32)) return false;
        } break;

        case Op::Colorize: {
          uint32_t tmp32 = 0;
          if(!read32(ip, tmp32)) return false;
          if(!read8(ip, opByte)) return false;
        } break;

        case Op::Anim: {
          uint16_t frameW = 0;
          uint16_t frameH = 0;
          uint16_t frameCount = 0;
          uint16_t fpsQ = 0;
          uint8_t flags = 0;
          if(!read16(ip, frameW)) return false;
          if(!read16(ip, frameH)) return false;
          if(!read16(ip, frameCount)) return false;
          if(!read16(ip, fpsQ)) return false;
          if(!read8(ip, flags)) return false;
          (void)frameW; (void)frameH; (void)frameCount; (void)fpsQ; (void)flags;
        } break;

        case Op::Overlay:
        case Op::Mask: {
          SrcMeta src{};
          if(!readSrc(ip, src)) return false;
          if(!patchTexId(src)) return false;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(!scan(subStart, subEnd)) return false;
          }
        } break;

        case Op::LowPart: {
          if(!read8(ip, opByte)) return false;
          SrcMeta src{};
          if(!readSrc(ip, src)) return false;
          if(!patchTexId(src)) return false;
          if(src.Kind == SrcKind::Sub) {
            size_t subStart = src.Off;
            size_t subEnd = subStart + src.Len;
            if(!scan(subStart, subEnd)) return false;
          }
        } break;

        case Op::Combine: {
          uint16_t w = 0, h = 0, n = 0;
          if(!read16(ip, w)) return false;
          if(!read16(ip, h)) return false;
          if(!read16(ip, n)) return false;
          for(uint16_t i = 0; i < n; ++i) {
            uint16_t tmp16 = 0;
            if(!read16(ip, tmp16)) return false;
            if(!read16(ip, tmp16)) return false;
            SrcMeta src{};
            if(!readSrc(ip, src)) return false;
            if(!patchTexId(src)) return false;
            if(src.Kind == SrcKind::Sub) {
              size_t subStart = src.Off;
              size_t subEnd = subStart + src.Len;
              if(!scan(subStart, subEnd)) return false;
            }
          }
          (void)w; (void)h;
        } break;

        default:
          return false;
      }
    }
    return true;
  };

  return scan(0, size);
}

void TexturePipelineProgram::fromBytes(std::vector<uint8_t> bytes) {
  Code_ = std::move(bytes);
  Patches_.clear();
  Source_.clear();
  PendingSub_.clear();
}

void TexturePipelineProgram::Lexer::unread(const Tok& t) {
  // allow only 1-level unread
  HasBuf = true;
  Buf = t;
}

TexturePipelineProgram::Tok TexturePipelineProgram::Lexer::peek() {
  Tok t = next();
  unread(t);
  return t;
}

void TexturePipelineProgram::Lexer::skipWs() {
  while (I < S.size()) {
    char c = S[I];
    if(c==' '||c=='\t'||c=='\r'||c=='\n'){ I++; continue; }
    if(c=='#'){ while (I<S.size() && S[I]!='\n') I++; continue; }
    if(c=='/' && I+1<S.size() && S[I+1]=='/'){ I+=2; while (I<S.size() && S[I]!='\n') I++; continue; }
    break;
  }
}

TexturePipelineProgram::Tok TexturePipelineProgram::Lexer::next() {
  if(HasBuf) { HasBuf = false; return Buf; }

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
    while (I<S.size()) {
      char ch = S[I];
      if(isAlnum(ch) || ch=='_' || ch=='.' || ch=='-' || ch=='#') { I++; continue; }
      break;
    }
    return {TokKind::Ident, std::string(S.substr(start, I-start)), 0};
  }

  I = S.size();
  return {TokKind::End, {}, 0};
}

TexturePipelineProgram::VM::VM(TextureProvider provider) : Provider_(std::move(provider)) {}

bool TexturePipelineProgram::VM::run(const std::vector<uint8_t>& code, OwnedTexture& out, double timeSeconds, std::string* err) {
  if(code.empty()) { if(err) *err="Пустой байткод"; return false; }

  Image cur;
  std::unordered_map<uint32_t, Image> texCache;
  std::unordered_map<uint64_t, Image> subCache; // key = (off<<24) | len

  size_t ip = 0;

  auto need = [&](size_t n)->bool{
    if(ip + n > code.size()) { if(err) *err="Байткод усечен"; return false; }
    return true;
  };

  while(true) {
    if(!need(1)) return false;
    Op op = static_cast<Op>(code[ip++]);
    if(op == Op::End) break;

    switch(op) {
      case Op::Base_Tex: {
        SrcRef src;
        if(!_readSrc(code, ip, src, err)) return false;
        if(src.Kind != SrcKind::TexId) return _bad(err, "Base_Tex must be TexId");
        cur = _loadTex(src.TexId24, texCache, err);
        if(cur.W == 0) return false;
      } break;

      case Op::Base_Fill: {
        if(!need(2+2+4)) return false;
        uint32_t w = _rd16(code, ip);
        uint32_t h = _rd16(code, ip);
        uint32_t color = _rd32(code, ip);
        cur = _makeSolid(w, h, color);
      } break;

      case Op::Base_Anim: {
        SrcRef src;
        if(!_readSrc(code, ip, src, err)) return false;
        if(src.Kind != SrcKind::TexId) return _bad(err, "Base_Anim must be TexId");
        if(!need(2+2+2+2+1)) return false;

        uint32_t frameW = _rd16(code, ip);
        uint32_t frameH = _rd16(code, ip);
        uint32_t frameCount = _rd16(code, ip);
        uint32_t fpsQ = _rd16(code, ip);
        uint32_t flags = code[ip++];

        Image sheet = _loadTex(src.TexId24, texCache, err);
        if(sheet.W == 0) return false;

        uint32_t fw = frameW ? frameW : sheet.W;
        uint32_t fh = frameH ? frameH : sheet.H;
        if(fw == 0 || fh == 0) return _bad(err, "Base_Anim invalid frame size");

        bool useGrid = (flags & AnimGrid) != 0;
        bool horizontal = (flags & AnimHorizontal) != 0;
        if(frameCount == 0) {
          uint32_t avail = 0;
          if(useGrid) {
            uint32_t cols = sheet.W / fw;
            uint32_t rows = sheet.H / fh;
            avail = cols * rows;
          } else {
            avail = horizontal ? (sheet.W / fw) : (sheet.H / fh);
          }
          frameCount = std::max<uint32_t>(1u, avail);
        }

        uint32_t fpsQv = fpsQ ? fpsQ : DefaultAnimFpsQ;
        double fps = double(fpsQv) / 256.0;
        double frameTime = timeSeconds * fps;
        if(frameTime < 0.0) frameTime = 0.0;

        uint32_t frameIndex = frameCount ? (uint32_t(frameTime) % frameCount) : 0u;
        double frac = frameTime - std::floor(frameTime);

        cur = useGrid ? _cropFrameGrid(sheet, frameIndex, fw, fh)
                      : _cropFrame(sheet, frameIndex, fw, fh, horizontal);

        if(flags & AnimSmooth) {
          uint32_t nextIndex = frameCount ? ((frameIndex + 1u) % frameCount) : 0u;
          Image next = useGrid ? _cropFrameGrid(sheet, nextIndex, fw, fh)
                               : _cropFrame(sheet, nextIndex, fw, fh, horizontal);
          _lerp(cur, next, frac);
        }
      } break;

      case Op::Anim: {
        if(!cur.W || !cur.H) return _bad(err, "Anim requires base image");
        if(!need(2+2+2+2+1)) return false;

        uint32_t frameW = _rd16(code, ip);
        uint32_t frameH = _rd16(code, ip);
        uint32_t frameCount = _rd16(code, ip);
        uint32_t fpsQ = _rd16(code, ip);
        uint32_t flags = code[ip++];

        const Image& sheet = cur;
        uint32_t fw = frameW ? frameW : sheet.W;
        uint32_t fh = frameH ? frameH : sheet.H;
        if(fw == 0 || fh == 0) return _bad(err, "Anim invalid frame size");

        bool useGrid = (flags & AnimGrid) != 0;
        bool horizontal = (flags & AnimHorizontal) != 0;
        if(frameCount == 0) {
          uint32_t avail = 0;
          if(useGrid) {
            uint32_t cols = sheet.W / fw;
            uint32_t rows = sheet.H / fh;
            avail = cols * rows;
          } else {
            avail = horizontal ? (sheet.W / fw) : (sheet.H / fh);
          }
          frameCount = std::max<uint32_t>(1u, avail);
        }

        uint32_t fpsQv = fpsQ ? fpsQ : DefaultAnimFpsQ;
        double fps = double(fpsQv) / 256.0;
        double frameTime = timeSeconds * fps;
        if(frameTime < 0.0) frameTime = 0.0;

        uint32_t frameIndex = frameCount ? (uint32_t(frameTime) % frameCount) : 0u;
        double frac = frameTime - std::floor(frameTime);

        cur = useGrid ? _cropFrameGrid(sheet, frameIndex, fw, fh)
                      : _cropFrame(sheet, frameIndex, fw, fh, horizontal);
        if(flags & AnimSmooth) {
          uint32_t nextIndex = frameCount ? ((frameIndex + 1u) % frameCount) : 0u;
          Image next = useGrid ? _cropFrameGrid(sheet, nextIndex, fw, fh)
                               : _cropFrame(sheet, nextIndex, fw, fh, horizontal);
          _lerp(cur, next, frac);
        }
      } break;

      case Op::Overlay: {
        SrcRef src;
        if(!_readSrc(code, ip, src, err)) return false;
        Image over = _loadSrc(code, src, texCache, subCache, timeSeconds, err);
        if(over.W == 0) return false;
        if(!cur.W) { cur = std::move(over); break; }
        over = _resizeNN_ifNeeded(over, cur.W, cur.H);
        _alphaOver(cur, over);
      } break;

      case Op::Mask: {
        SrcRef src;
        if(!_readSrc(code, ip, src, err)) return false;
        Image m = _loadSrc(code, src, texCache, subCache, timeSeconds, err);
        if(m.W == 0) return false;
        if(!cur.W) return _bad(err, "Mask requires base image");
        m = _resizeNN_ifNeeded(m, cur.W, cur.H);
        _applyMask(cur, m);
      } break;

      case Op::LowPart: {
        if(!need(1)) return false;
        uint32_t pct = std::min<uint32_t>(100u, uint32_t(code[ip++]));
        SrcRef src;
        if(!_readSrc(code, ip, src, err)) return false;
        Image over = _loadSrc(code, src, texCache, subCache, timeSeconds, err);
        if(over.W == 0) return false;
        if(!cur.W) return _bad(err, "LowPart requires base image");
        over = _resizeNN_ifNeeded(over, cur.W, cur.H);
        _lowpart(cur, over, pct);
      } break;

      case Op::Resize: {
        if(!cur.W) return _bad(err, "Resize requires base image");
        if(!need(2+2)) return false;
        uint32_t w = _rd16(code, ip);
        uint32_t h = _rd16(code, ip);
        cur = _resizeNN(cur, w, h);
      } break;

      case Op::Transform: {
        if(!cur.W) return _bad(err, "Transform requires base image");
        if(!need(1)) return false;
        uint32_t t = code[ip++] & 7u;
        cur = _transform(cur, t);
      } break;

      case Op::Opacity: {
        if(!cur.W) return _bad(err, "Opacity requires base image");
        if(!need(1)) return false;
        uint32_t a = code[ip++] & 0xFFu;
        _opacity(cur, uint8_t(a));
      } break;

      case Op::NoAlpha: {
        if(!cur.W) return _bad(err, "NoAlpha requires base image");
        _noAlpha(cur);
      } break;

      case Op::MakeAlpha: {
        if(!cur.W) return _bad(err, "MakeAlpha requires base image");
        if(!need(3)) return false;
        uint32_t rr = code[ip++], gg = code[ip++], bb = code[ip++];
        uint32_t rgb24 = (rr << 16) | (gg << 8) | bb;
        _makeAlpha(cur, rgb24);
      } break;

      case Op::Invert: {
        if(!cur.W) return _bad(err, "Invert requires base image");
        if(!need(1)) return false;
        uint32_t mask = code[ip++] & 0xFu;
        _invert(cur, mask);
      } break;

      case Op::Brighten: {
        if(!cur.W) return _bad(err, "Brighten requires base image");
        _brighten(cur);
      } break;

      case Op::Contrast: {
        if(!cur.W) return _bad(err, "Contrast requires base image");
        if(!need(2)) return false;
        int c = int(code[ip++]) - 127;
        int b = int(code[ip++]) - 127;
        _contrast(cur, c, b);
      } break;

      case Op::Multiply: {
        if(!cur.W) return _bad(err, "Multiply requires base image");
        if(!need(4)) return false;
        uint32_t color = _rd32(code, ip);
        _multiply(cur, color);
      } break;

      case Op::Screen: {
        if(!cur.W) return _bad(err, "Screen requires base image");
        if(!need(4)) return false;
        uint32_t color = _rd32(code, ip);
        _screen(cur, color);
      } break;

      case Op::Colorize: {
        if(!cur.W) return _bad(err, "Colorize requires base image");
        if(!need(4+1)) return false;
        uint32_t color = _rd32(code, ip);
        uint32_t ratio = code[ip++] & 0xFFu;
        _colorize(cur, color, uint8_t(ratio));
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

bool TexturePipelineProgram::VM::_bad(std::string* err, const char* msg) {
  if(err) *err = msg;
  return false;
}

bool TexturePipelineProgram::VM::_readSrc(const std::vector<uint8_t>& code, size_t& ip, SrcRef& out, std::string* err) {
  if(ip >= code.size()) return _bad(err, "Bytecode truncated (SrcRef.kind)");
  out.Kind = static_cast<SrcKind>(code[ip++]);
  if(out.Kind == SrcKind::TexId) {
    if(ip + 3 > code.size()) return _bad(err, "Bytecode truncated (TexId24)");
    out.TexId24 = _rd24(code, ip);
    out.Off24 = 0; out.Len24 = 0;
    return true;
  }
  if(out.Kind == SrcKind::Sub) {
    if(ip + 6 > code.size()) return _bad(err, "Bytecode truncated (Sub off/len)");
    out.Off24 = _rd24(code, ip);
    out.Len24 = _rd24(code, ip);
    out.TexId24 = 0;
    return true;
  }
  return _bad(err, "Unknown SrcKind");
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_loadTex(uint32_t id, std::unordered_map<uint32_t, Image>& cache, std::string* err) {
  auto it = cache.find(id);
  if(it != cache.end()) return it->second;

  auto t = Provider_(id);
  if(!t || !t->Pixels || !t->Width || !t->Height) {
    if(err) *err = "Идентификатор текстуры не найден: " + std::to_string(id);
    return {};
  }
  Image img;
  img.W = t->Width; img.H = t->Height;
  img.Px.assign(t->Pixels, t->Pixels + size_t(img.W)*size_t(img.H));
  cache.emplace(id, img);
  return img;
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_loadSub(const std::vector<uint8_t>& code,
               uint32_t off, uint32_t len,
               std::unordered_map<uint32_t, Image>& /*texCache*/,
               std::unordered_map<uint64_t, Image>& subCache,
               double timeSeconds,
               std::string* err) {
  uint64_t key = (uint32_t(off) << 24) | uint32_t(len);
  auto it = subCache.find(key);
  if(it != subCache.end()) return it->second;

  size_t start = size_t(off);
  size_t end = start + size_t(len);
  if(end > code.size()) { if(err) *err="Подпрограмма выходит за пределы"; return {}; }

  std::vector<uint8_t> slice(code.begin()+start, code.begin()+end);
  OwnedTexture tmp;
  VM nested(Provider_);
  if(!nested.run(slice, tmp, timeSeconds, err)) return {};

  Image img;
  img.W = tmp.Width; img.H = tmp.Height; img.Px = std::move(tmp.Pixels);
  subCache.emplace(key, img);
  return img;
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_loadSrc(const std::vector<uint8_t>& code,
               const SrcRef& src,
               std::unordered_map<uint32_t, Image>& texCache,
               std::unordered_map<uint64_t, Image>& subCache,
               double timeSeconds,
               std::string* err) {
  if(src.Kind == SrcKind::TexId) return _loadTex(src.TexId24, texCache, err);
  if(src.Kind == SrcKind::Sub)   return _loadSub(code, src.Off24, src.Len24, texCache, subCache, timeSeconds, err);
  if(err) *err = "Неизвестный SrcKind";
  return {};
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_makeSolid(uint32_t w, uint32_t h, uint32_t color) {
  Image img; img.W=w; img.H=h;
  img.Px.assign(size_t(w)*size_t(h), color);
  return img;
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_resizeNN(const Image& src, uint32_t nw, uint32_t nh) {
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

TexturePipelineProgram::Image TexturePipelineProgram::VM::_resizeNN_ifNeeded(Image img, uint32_t w, uint32_t h) {
  if(img.W == w && img.H == h) return img;
  return _resizeNN(img, w, h);
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_cropFrame(const Image& sheet, uint32_t index, uint32_t fw, uint32_t fh, bool horizontal) {
  Image out;
  out.W = fw;
  out.H = fh;
  out.Px.assign(size_t(fw) * size_t(fh), 0u);

  uint32_t baseX = horizontal ? (index * fw) : 0u;
  uint32_t baseY = horizontal ? 0u : (index * fh);

  for(uint32_t y = 0; y < fh; ++y) {
    uint32_t sy = baseY + y;
    if(sy >= sheet.H) continue;
    for(uint32_t x = 0; x < fw; ++x) {
      uint32_t sx = baseX + x;
      if(sx >= sheet.W) continue;
      out.Px[size_t(y) * fw + x] = sheet.Px[size_t(sy) * sheet.W + sx];
    }
  }
  return out;
}

TexturePipelineProgram::Image TexturePipelineProgram::VM::_cropFrameGrid(const Image& sheet, uint32_t index, uint32_t fw, uint32_t fh) {
  Image out;
  out.W = fw;
  out.H = fh;
  out.Px.assign(size_t(fw) * size_t(fh), 0u);
  if(fw == 0 || fh == 0) return out;

  uint32_t cols = sheet.W / fw;
  uint32_t rows = sheet.H / fh;
  if(cols == 0 || rows == 0) return out;

  uint32_t col = index % cols;
  uint32_t row = index / cols;
  if(row >= rows) return out;

  uint32_t baseX = col * fw;
  uint32_t baseY = row * fh;

  for(uint32_t y = 0; y < fh; ++y) {
    uint32_t sy = baseY + y;
    if(sy >= sheet.H) continue;
    for(uint32_t x = 0; x < fw; ++x) {
      uint32_t sx = baseX + x;
      if(sx >= sheet.W) continue;
      out.Px[size_t(y) * fw + x] = sheet.Px[size_t(sy) * sheet.W + sx];
    }
  }
  return out;
}

void TexturePipelineProgram::VM::_lerp(Image& base, const Image& over, double t) {
  if(t <= 0.0) return;
  if(t >= 1.0) { base = over; return; }
  if(base.W != over.W || base.H != over.H) return;

  const size_t n = base.Px.size();
  for(size_t i = 0; i < n; ++i) {
    uint32_t a = base.Px[i];
    uint32_t b = over.Px[i];
    int ar = _r(a), ag = _g(a), ab = _b(a), aa = _a(a);
    int br = _r(b), bg = _g(b), bb = _b(b), ba = _a(b);

    uint8_t rr = _clampu8(int(ar + (br - ar) * t));
    uint8_t rg = _clampu8(int(ag + (bg - ag) * t));
    uint8_t rb = _clampu8(int(ab + (bb - ab) * t));
    uint8_t ra = _clampu8(int(aa + (ba - aa) * t));

    base.Px[i] = _pack(ra, rr, rg, rb);
  }
}

void TexturePipelineProgram::VM::_alphaOver(Image& base, const Image& over) {
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

void TexturePipelineProgram::VM::_applyMask(Image& base, const Image& mask) {
  const size_t n = base.Px.size();
  for(size_t i=0;i<n;i++){
    uint32_t b = base.Px[i], m = mask.Px[i];
    uint8_t outA = uint8_t((uint32_t(_a(b)) * uint32_t(_a(m))) / 255);
    base.Px[i] = _pack(outA, _r(b), _g(b), _b(b));
  }
}

void TexturePipelineProgram::VM::_opacity(Image& img, uint8_t mul) {
  for(auto& p : img.Px) {
    uint8_t na = uint8_t((uint32_t(_a(p)) * mul) / 255);
    p = _pack(na, _r(p), _g(p), _b(p));
  }
}

void TexturePipelineProgram::VM::_noAlpha(Image& img) {
  for(auto& p : img.Px) p = _pack(255, _r(p), _g(p), _b(p));
}

void TexturePipelineProgram::VM::_makeAlpha(Image& img, uint32_t rgb24) {
  uint8_t rr = uint8_t((rgb24 >> 16) & 0xFF);
  uint8_t gg = uint8_t((rgb24 >>  8) & 0xFF);
  uint8_t bb = uint8_t((rgb24 >>  0) & 0xFF);
  for(auto& p : img.Px) {
    if(_r(p)==rr && _g(p)==gg && _b(p)==bb) p = _pack(0, _r(p), _g(p), _b(p));
  }
}

void TexturePipelineProgram::VM::_invert(Image& img, uint32_t maskBits) {
  for(auto& p : img.Px) {
    uint8_t a=_a(p), r=_r(p), g=_g(p), b=_b(p);
    if(maskBits & 1u) r = 255 - r;
    if(maskBits & 2u) g = 255 - g;
    if(maskBits & 4u) b = 255 - b;
    if(maskBits & 8u) a = 255 - a;
    p = _pack(a,r,g,b);
  }
}

void TexturePipelineProgram::VM::_brighten(Image& img) {
  for(auto& p : img.Px) {
    int r = _r(p), g = _g(p), b = _b(p);
    r = r + (255 - r) / 3;
    g = g + (255 - g) / 3;
    b = b + (255 - b) / 3;
    p = _pack(_a(p), _clampu8(r), _clampu8(g), _clampu8(b));
  }
}

void TexturePipelineProgram::VM::_contrast(Image& img, int c, int br) {
  double C = double(std::max(-127, std::min(127, c)));
  double factor = (259.0 * (C + 255.0)) / (255.0 * (259.0 - C));
  for(auto& p : img.Px) {
    int r = int(factor * (int(_r(p)) - 128) + 128) + br;
    int g = int(factor * (int(_g(p)) - 128) + 128) + br;
    int b = int(factor * (int(_b(p)) - 128) + 128) + br;
    p = _pack(_a(p), _clampu8(r), _clampu8(g), _clampu8(b));
  }
}

void TexturePipelineProgram::VM::_multiply(Image& img, uint32_t color) {
  uint8_t cr=_r(color), cg=_g(color), cb=_b(color);
  for(auto& p : img.Px) {
    uint8_t r = uint8_t((uint32_t(_r(p)) * cr) / 255);
    uint8_t g = uint8_t((uint32_t(_g(p)) * cg) / 255);
    uint8_t b = uint8_t((uint32_t(_b(p)) * cb) / 255);
    p = _pack(_a(p), r,g,b);
  }
}

void TexturePipelineProgram::VM::_screen(Image& img, uint32_t color) {
  uint8_t cr=_r(color), cg=_g(color), cb=_b(color);
  for(auto& p : img.Px) {
    uint8_t r = uint8_t(255 - ((255 - _r(p)) * (255 - cr)) / 255);
    uint8_t g = uint8_t(255 - ((255 - _g(p)) * (255 - cg)) / 255);
    uint8_t b = uint8_t(255 - ((255 - _b(p)) * (255 - cb)) / 255);
    p = _pack(_a(p), r,g,b);
  }
}

void TexturePipelineProgram::VM::_colorize(Image& img, uint32_t color, uint8_t ratio) {
  uint8_t cr=_r(color), cg=_g(color), cb=_b(color);
  for(auto& p : img.Px) {
    int r = (int(_r(p)) * (255 - ratio) + int(cr) * ratio) / 255;
    int g = (int(_g(p)) * (255 - ratio) + int(cg) * ratio) / 255;
    int b = (int(_b(p)) * (255 - ratio) + int(cb) * ratio) / 255;
    p = _pack(_a(p), uint8_t(r), uint8_t(g), uint8_t(b));
  }
}

void TexturePipelineProgram::VM::_lowpart(Image& base, const Image& over, uint32_t percent) {
  uint32_t startY = base.H - (base.H * percent) / 100;
  for(uint32_t y=startY; y<base.H; y++){
    for(uint32_t x=0; x<base.W; x++){
      size_t i = size_t(y)*base.W + x;
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

TexturePipelineProgram::Image TexturePipelineProgram::VM::_transform(const Image& src, uint32_t t) {
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

bool TexturePipelineProgram::_parseHexColor(std::string_view s, uint32_t& outARGB) {
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

bool TexturePipelineProgram::_parseProgram(std::string* err) {
  Lexer lx{Source_};
  Tok t = lx.next();
  bool hasTexPrefix = (t.Kind == TokKind::Ident && t.Text == "tex");

  // compile base into main Code_
  if(hasTexPrefix) {
    if(!_compileBaseAfterTex(lx, Code_, /*abs*/&Patches_, /*rel*/nullptr, err)) return false;
  } else {
    if(!_compileBaseFromToken(lx, t, Code_, /*abs*/&Patches_, /*rel*/nullptr, err)) return false;
  }

  // pipeline: |> op ...
  Tok nt = lx.next();
  while (nt.Kind == TokKind::Pipe) {
    Tok opName = lx.next();
    if(opName.Kind != TokKind::Ident) { if(err) *err="Ожидалось имя операции после |>"; return false; }
    ParsedOp op; op.Name = opName.Text;

    Tok peek = lx.next();
    if(peek.Kind == TokKind::LParen) {
      if(!_parseArgListOrTextureExpr(lx, op, err)) return false;
      nt = lx.next();
    } else {
      nt = peek; // no-arg op
    }

    if(!_compileOpInto(lx, op, Code_, /*abs*/&Patches_, /*rel*/nullptr, err)) return false;
  }

  _emitOp(Code_, Op::End);
  if (Code_.size() > MaxCodeBytes) {
    if (err)
      *err = "Байткод пайплайна слишком большой: " + std::to_string(Code_.size()) +
        " > MaxCodeBytes(" + std::to_string(MaxCodeBytes) + ")";
    return false;
  }

  return true;
}

bool TexturePipelineProgram::_compileBaseAfterTex(Lexer& lx,
                          std::vector<uint8_t>& out,
                          std::vector<Patch>* absPatches,
                          std::vector<RelPatch>* relPatches,
                          std::string* err) {
  Tok a = lx.next();
  return _compileBaseFromToken(lx, a, out, absPatches, relPatches, err);
}

bool TexturePipelineProgram::_compileBaseFromToken(Lexer& lx,
                           const Tok& a,
                           std::vector<uint8_t>& out,
                           std::vector<Patch>* absPatches,
                           std::vector<RelPatch>* relPatches,
                           std::string* err) {
  if(a.Kind == TokKind::End) {
    if(err) *err="Ожидалось текстурное выражение";
    return false;
  }

  if(a.Kind == TokKind::Ident && a.Text == "anim") {
    Tok lp = lx.next();
    if(lp.Kind != TokKind::LParen) { if(err) *err="Ожидалась '(' после anim"; return false; }

    ParsedOp op; op.Name="anim";
    if(!_parseArgList(lx, op, err)) return false;

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

    std::string tex = namedS("tex").value_or(posS(0).value_or(""));
    if(tex.empty()) { if(err) *err="anim требует имя текстуры"; return false; }

    uint32_t frameW = namedU("frame_w").value_or(namedU("w").value_or(posU(1).value_or(0)));
    uint32_t frameH = namedU("frame_h").value_or(namedU("h").value_or(posU(2).value_or(0)));
    uint32_t frames = namedU("frames").value_or(namedU("count").value_or(posU(3).value_or(0)));
    uint32_t fps    = namedU("fps").value_or(posU(4).value_or(0));
    uint32_t smooth = namedU("smooth").value_or(posU(5).value_or(0));

    std::string axis = namedS("axis").value_or("");
    bool axisHorizontal = (!axis.empty() && (axis[0] == 'x' || axis[0] == 'h'));
    bool axisVertical = (!axis.empty() && (axis[0] == 'y' || axis[0] == 'v'));
    bool useGrid = axis.empty() || axis[0] == 'g';

    if(frameW > 65535u || frameH > 65535u || frames > 65535u) {
      if(err) *err="параметры anim должны помещаться в uint16";
      return false;
    }

    uint32_t fpsQ = fps ? std::min<uint32_t>(0xFFFFu, fps * 256u) : DefaultAnimFpsQ;
    uint32_t flags = (smooth ? AnimSmooth : 0);
    if(useGrid) flags |= AnimGrid;
    else if(axisHorizontal) flags |= AnimHorizontal;
    else if(!axisVertical) flags |= AnimGrid;

    _emitOp(out, Op::Base_Anim);
    _emitSrcTexName(out, absPatches, relPatches, tex);
    _emitU16(out, frameW);
    _emitU16(out, frameH);
    _emitU16(out, frames);
    _emitU16(out, fpsQ);
    _emitU8(out, flags);
    return true;
  }

  if(a.Kind == TokKind::Ident || a.Kind == TokKind::String) {
    // name   (or "name.png" => normalized)
    _emitOp(out, Op::Base_Tex);
    _emitSrcTexName(out, absPatches, relPatches, a.Text);
    return true;
  }

  if(a.Kind == TokKind::Number) {
    // 32x32 "#RRGGBBAA"
    Tok xTok = lx.next();
    Tok b = lx.next();
    Tok colTok = lx.next();
    if(xTok.Kind != TokKind::X || b.Kind != TokKind::Number || (colTok.Kind!=TokKind::Ident && colTok.Kind!=TokKind::String)) {
      if(err) *err="Ожидалось: <w>x<h> <#color>";
      return false;
    }
    uint32_t w = a.U32, h = b.U32;
    uint32_t color = 0;
    if(!_parseHexColor(colTok.Text, color)) {
      if(err) *err="Неверный литерал цвета (используйте #RRGGBB или #RRGGBBAA)";
      return false;
    }
    if(w>65535u || h>65535u) { if(err) *err="w/h должны помещаться в uint16"; return false; }
    _emitOp(out, Op::Base_Fill);
    _emitU16(out, w);
    _emitU16(out, h);
    _emitU32(out, color);
    return true;
  }

  if(err) *err="Некорректное базовое текстурное выражение";
  return false;
}

bool TexturePipelineProgram::_parseArgListOrTextureExpr(Lexer& lx, ParsedOp& op, std::string* err) {
  Tok first = lx.next();

  if(first.Kind==TokKind::Ident && first.Text=="tex") {
    // marker
    ArgVal av; av.Kind = ArgVal::ValueKind::Ident; av.S = "__SUBTEX__";
    op.Pos.push_back(av);

    PendingSubData sub;
    if(!_compileSubProgramFromAlreadySawTex(lx, sub, err)) return false;

    Tok end = lx.next();
    if(end.Kind != TokKind::RParen) { if(err) *err="Ожидалась ')' после подвыражения текстуры"; return false; }

    PendingSub_[&op] = std::move(sub);
    return true;
  }

  // otherwise parse as normal arg list, where `first` is first token inside '('
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

    if(err) *err = "Ожидалась ',' или ')' в списке аргументов";
    return false;
  }
}

bool TexturePipelineProgram::_parseArgList(Lexer& lx, ParsedOp& op, std::string* err) {
  Tok t = lx.next();
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

    if(err) *err = "Ожидалась ',' или ')' в списке аргументов";
    return false;
  }
}

bool TexturePipelineProgram::_tokToVal(const Tok& t, ArgVal& out, std::string* err) {
  if(t.Kind == TokKind::Number) { out.Kind=ArgVal::ValueKind::U32; out.U32=t.U32; return true; }
  if(t.Kind == TokKind::String) { out.Kind=ArgVal::ValueKind::Str; out.S=t.Text; return true; }
  if(t.Kind == TokKind::Ident)  { out.Kind=ArgVal::ValueKind::Ident; out.S=t.Text; return true; }
  if(err) *err = "Ожидалось значение";
  return false;
}

bool TexturePipelineProgram::_compileSubProgramFromAlreadySawTex(Lexer& lx, PendingSubData& outSub, std::string* err) {
  outSub.Bytes.clear();
  outSub.RelPatches.clear();

  // base
  if(!_compileBaseAfterTex(lx, outSub.Bytes, /*abs*/nullptr, /*rel*/&outSub.RelPatches, err))
    return false;

  // pipeline until ')'
  while(true) {
    // peek
    Tok nt = lx.peek();
    if(nt.Kind == TokKind::RParen) break;
    if(nt.Kind != TokKind::Pipe) { if(err) *err="Подтекстура: ожидалось '|>' или ')'"; return false; }

    // consume pipe
    lx.next();
    Tok opName = lx.next();
    if(opName.Kind != TokKind::Ident) { if(err) *err="Подтекстура: ожидалось имя операции"; return false; }

    ParsedOp op; op.Name = opName.Text;

    Tok lp = lx.next();
    if(lp.Kind == TokKind::LParen) {
      if(!_parseArgListOrTextureExpr(lx, op, err)) return false;
    } else {
      // no-arg op, lp already is next token (pipe or ')'), so we need to "unread" — can't.
      // simplest: treat it as next token for outer loop by rewinding lexer state.
      // We'll do it by storing the token back via a small hack: rebuild peek? Too heavy.
      // Instead: enforce parentheses for ops in subprogram except no-arg ops (brighten/noalpha) which can be without.
      // To keep behavior identical to main, we handle no-arg by rewinding I one token is not possible,
      // so we accept that in subprogram, no-arg ops must be written as brighten() etc.
      if(err) *err="Подтекстура: операции без аргументов должны использовать скобки, напр. brighten()";
      return false;
    }

    if(!_compileOpInto(lx, op, outSub.Bytes, /*abs*/nullptr, /*rel*/&outSub.RelPatches, err))
      return false;
  }

  // Pipeline until we see ')'
  while (true) {
    Tok nt = lx.peek();
    if(nt.Kind == TokKind::RParen) break;
    if(nt.Kind != TokKind::Pipe) { if(err) *err="Подтекстура: ожидалось '|>' или ')'"; return false; }

    // consume pipe
    lx.next();

    Tok opName = lx.next();
    if(opName.Kind != TokKind::Ident) { if(err) *err="Подтекстура: ожидалось имя операции"; return false; }

    ParsedOp op; op.Name = opName.Text;

    // allow both op and op(...)
    Tok maybe = lx.peek();
    if(maybe.Kind == TokKind::LParen) {
      lx.next(); // consume '('
      if(!_parseArgListOrTextureExpr(lx, op, err)) return false;
    } else {
      // no-arg op; nothing to parse
    }

    if(!_compileOpInto(lx, op, outSub.Bytes, /*abs*/nullptr, /*rel*/&outSub.RelPatches, err))
      return false;
  }

  _emitOp(outSub.Bytes, Op::End);
  return true;
}

bool TexturePipelineProgram::_appendSubprogram(std::vector<uint8_t>& out,
                              PendingSubData&& sub,
                              std::vector<Patch>* absPatches,
                              std::vector<RelPatch>* relPatches,
                              uint32_t& outOff,
                              uint32_t& outLen,
                              std::string* err) {
  const size_t offset = out.size();
  const size_t len = sub.Bytes.size();

  if(offset > 0xFFFFFFu || len > 0xFFFFFFu || (offset + len) > 0xFFFFFFu) {
    if(err) *err = "Подпрограмма слишком большая (off/len должны влезать в u24 байт)";
    return false;
  }

  if(offset + len > MaxCodeBytes) {
    if(err) *err = "Байткод пайплайна слишком большой после вставки подпрограммы: " +
                  std::to_string(offset + len) + " > MaxCodeBytes(" + std::to_string(MaxCodeBytes) + ")";
    return false;
  }

  // migrate patches
  if(absPatches) {
    for(const auto& rp : sub.RelPatches) {
      absPatches->push_back(Patch{offset + rp.Rel0, rp.Name});
    }
  }
  if(relPatches) {
    for(const auto& rp : sub.RelPatches) {
      relPatches->push_back(RelPatch{offset + rp.Rel0, rp.Name});
    }
  }

  out.insert(out.end(), sub.Bytes.begin(), sub.Bytes.end());

  outOff = uint32_t(offset);
  outLen = uint32_t(len);
  return true;
}

bool TexturePipelineProgram::_compileOpInto(Lexer& /*lx*/,
                    const ParsedOp& op,
                    std::vector<uint8_t>& out,
                    std::vector<Patch>* absPatches,
                    std::vector<RelPatch>* relPatches,
                    std::string* err) {
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

  auto emitSrcFromName = [&](const std::string& n){
    _emitSrcTexName(out, absPatches, relPatches, n);
  };

  auto emitSrcFromPendingSub = [&]()->bool{
    auto it = PendingSub_.find(&op);
    if(it == PendingSub_.end()) { if(err) *err="Внутренняя ошибка: отсутствует подпрограмма"; return false; }
    uint32_t off=0, len=0;
    if(!_appendSubprogram(out, std::move(it->second), absPatches, relPatches, off, len, err)) return false;
    PendingSub_.erase(it);
    _emitSrcSub(out, off, len);
    return true;
  };

  // --- Ops that accept a "texture" argument: overlay/mask/lowpart ---
  if(op.Name == "overlay") {
    _emitOp(out, Op::Overlay);
    if(!op.Pos.empty() && op.Pos[0].S == "__SUBTEX__") return emitSrcFromPendingSub();

    // allow overlay(name) or overlay(tex=name)
    std::string tex = namedS("tex").value_or(posS(0).value_or(""));
    if(tex.empty()) { if(err) *err="overlay требует аргумент-текстуру"; return false; }
    emitSrcFromName(tex);
    return true;
  }

  if(op.Name == "mask") {
    _emitOp(out, Op::Mask);
    if(!op.Pos.empty() && op.Pos[0].S == "__SUBTEX__") return emitSrcFromPendingSub();

    std::string tex = namedS("tex").value_or(posS(0).value_or(""));
    if(tex.empty()) { if(err) *err="mask требует аргумент-текстуру"; return false; }
    emitSrcFromName(tex);
    return true;
  }

  if(op.Name == "lowpart") {
    uint32_t pct = namedU("percent").value_or(posU(0).value_or(0));
    if(!pct) { if(err) *err="lowpart требует процент"; return false; }

    _emitOp(out, Op::LowPart);
    _emitU8(out, std::min<uint32_t>(100u, pct));

    // 2nd arg can be nested subtex or name
    if(op.Pos.size() >= 2 && op.Pos[1].S == "__SUBTEX__") return emitSrcFromPendingSub();

    std::string tex = namedS("tex").value_or(posS(1).value_or(""));
    if(tex.empty()) { if(err) *err="lowpart требует аргумент-текстуру"; return false; }
    emitSrcFromName(tex);
    return true;
  }

  // --- Unary ops ---
  if(op.Name == "resize") {
    uint32_t w = namedU("w").value_or(posU(0).value_or(0));
    uint32_t h = namedU("h").value_or(posU(1).value_or(0));
    if(!w || !h || w>65535u || h>65535u) { if(err) *err="resize(w,h) должны помещаться в uint16"; return false; }
    _emitOp(out, Op::Resize); _emitU16(out, w); _emitU16(out, h);
    return true;
  }

  if(op.Name == "transform") {
    uint32_t t = namedU("t").value_or(posU(0).value_or(0));
    _emitOp(out, Op::Transform); _emitU8(out, t & 7u);
    return true;
  }

  if(op.Name == "opacity") {
    uint32_t a = namedU("a").value_or(posU(0).value_or(255));
    _emitOp(out, Op::Opacity); _emitU8(out, a & 0xFFu);
    return true;
  }

  if(op.Name == "remove_alpha" || op.Name == "noalpha") {
    _emitOp(out, Op::NoAlpha);
    return true;
  }

  if(op.Name == "make_alpha") {
    std::string col = namedS("color").value_or(posS(0).value_or(""));
    uint32_t argb=0;
    if(!_parseHexColor(col, argb)) { if(err) *err="make_alpha требует цвет #RRGGBB"; return false; }
    uint32_t rgb24 = argb & 0x00FFFFFFu;
    _emitOp(out, Op::MakeAlpha);
    _emitU8(out, (rgb24 >> 16) & 0xFFu);
    _emitU8(out, (rgb24 >>  8) & 0xFFu);
    _emitU8(out, (rgb24 >>  0) & 0xFFu);
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
    _emitOp(out, Op::Invert); _emitU8(out, mask & 0xFu);
    return true;
  }

  if(op.Name == "brighten") {
    _emitOp(out, Op::Brighten);
    return true;
  }

  if(op.Name == "contrast") {
    int c = int(namedU("value").value_or(posU(0).value_or(0)));
    int b = int(namedU("brightness").value_or(posU(1).value_or(0)));
    c = std::max(-127, std::min(127, c));
    b = std::max(-127, std::min(127, b));
    _emitOp(out, Op::Contrast);
    _emitU8(out, uint32_t(c + 127));
    _emitU8(out, uint32_t(b + 127));
    return true;
  }

  auto compileColorOp = [&](Op opcode, bool needsRatio)->bool{
    std::string col = namedS("color").value_or(posS(0).value_or(""));
    uint32_t argb=0;
    if(!_parseHexColor(col, argb)) { if(err) *err="Неверный литерал цвета"; return false; }
    _emitOp(out, opcode);
    _emitU32(out, argb);
    if(needsRatio) {
      uint32_t ratio = namedU("ratio").value_or(posU(1).value_or(255));
      _emitU8(out, ratio & 0xFFu);
    }
    return true;
  };

  if(op.Name == "multiply") return compileColorOp(Op::Multiply, false);
  if(op.Name == "screen")   return compileColorOp(Op::Screen, false);
  if(op.Name == "colorize") return compileColorOp(Op::Colorize, true);

  if(op.Name == "anim") {
    uint32_t frameW = namedU("frame_w").value_or(namedU("w").value_or(posU(0).value_or(0)));
    uint32_t frameH = namedU("frame_h").value_or(namedU("h").value_or(posU(1).value_or(0)));
    uint32_t frames = namedU("frames").value_or(namedU("count").value_or(posU(2).value_or(0)));
    uint32_t fps    = namedU("fps").value_or(posU(3).value_or(0));
    uint32_t smooth = namedU("smooth").value_or(posU(4).value_or(0));

    std::string axis = namedS("axis").value_or("");
    bool axisHorizontal = (!axis.empty() && (axis[0] == 'x' || axis[0] == 'h'));
    bool axisVertical = (!axis.empty() && (axis[0] == 'y' || axis[0] == 'v'));
    bool useGrid = axis.empty() || axis[0] == 'g';

    if(frameW > 65535u || frameH > 65535u || frames > 65535u) {
      if(err) *err="параметры anim должны помещаться в uint16";
      return false;
    }

    uint32_t fpsQ = fps ? std::min<uint32_t>(0xFFFFu, fps * 256u) : DefaultAnimFpsQ;
    uint32_t flags = (smooth ? AnimSmooth : 0);
    if(useGrid) flags |= AnimGrid;
    else if(axisHorizontal) flags |= AnimHorizontal;
    else if(!axisVertical) flags |= AnimGrid;

    _emitOp(out, Op::Anim);
    _emitU16(out, frameW);
    _emitU16(out, frameH);
    _emitU16(out, frames);
    _emitU16(out, fpsQ);
    _emitU8(out, flags);
    return true;
  }

  if(err) *err = "Неизвестная операция: " + op.Name;
  return false;
}
