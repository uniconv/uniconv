# uniconv - Universal Converter & Content Intelligence Tool

## 프로젝트 요약

컨텍스트 메뉴 통합과 AI 기반 콘텐츠 이해 기능을 갖춘 크로스 플랫폼 파일 변환 도구.
"우클릭 한 방"의 편리함과 파워유저를 위한 CLI 확장성을 동시에 제공.

**단순 포맷 변환을 넘어서:**
- 콘텐츠를 **변환**하고 (Transform)
- 원하는 것을 **추출**하고 (Extract)
- 결과를 **적재**하는 (Load)

**ETL 개념 기반**의 콘텐츠 인텔리전스 도구.

---

## 1. 핵심 가치

### 1.1 차별화 포인트 (vs ImageMagick, FFmpeg)

| 기능 | ImageMagick/FFmpeg | uniconv |
|------|-------------------|---------|
| 프리셋 시스템 | ❌ | ✅ 저장/불러오기 |
| Watch 모드 | ❌ | ✅ 폴더 감시 자동 변환 |
| JSON 출력 | ❌ | ✅ LLM/자동화 친화적 |
| 타겟 용량 지정 | ❌ | ✅ `--target-size 25MB` |
| AI 이미지 처리 | ❌ | ✅ 배경 제거, 자동 보정 등 |
| 컨텍스트 메뉴 통합 | 수동 스크립트 | ✅ 자동 설치 |
| Interactive 모드 | ❌ | ✅ 초보자 친화적 |
| 콘텐츠 추출 | ❌ | ✅ 얼굴, 텍스트, 표, 장면 등 |
| 의미 기반 검색 | ❌ | ✅ "해변 사진 찾아줘" |
| 구조화 파싱 | ❌ | ✅ 영수증, 명함 → JSON |
| 플러그인 시스템 | ❌ | ✅ 다중 언어 지원 |

### 1.2 타겟 사용자

- **일반 사용자**: 컨텍스트 메뉴로 간편 변환
- **파워 유저**: CLI + 프리셋 + 자동화
- **개발자**: JSON 출력, 플러그인 개발 (다중 언어)
- **LLM/Agent**: 구조화된 I/O

---

## 2. 핵심 개념: ETL 기반 액션

### 2.1 세 가지 액션

모든 동작은 ETL(Extract, Transform, Load) 개념을 따름:

| 액션 | 옵션 | 의미 | 예시 |
|------|------|------|------|
| **Transform** | `-t` | 변환 | 포맷 변환, 리사이즈, 품질 조정 |
| **Extract** | `-e` | 추출 | 얼굴, 텍스트, 오디오, 장면, 검색 |
| **Load** | `-l` | 적재 | 클라우드 업로드, 외부 서비스 전송 |

### 2.2 CLI 기본 구조

```bash
uniconv <source> -<e|t|l> <target>[@plugin] [core옵션] [-- 플러그인옵션]
```

- `source`: 파일, 디렉토리, URL, stdin(-)
- `target`: 변환/추출/적재 대상
- `@plugin`: (선택) 특정 플러그인 지정
- `--`: core 옵션과 플러그인 옵션 구분

### 2.3 사용 예시

```bash
# Transform: 변환
uniconv photo.heic -t jpg
uniconv photo.heic -t jpg -q 90
uniconv photo.heic -t jpg@vips -- --strip-exif
uniconv video.mov -t mp4@ffmpeg -- --crf 23
uniconv video.mov -t gif@ffmpeg -- --fps 15 --width 480

# Extract: 추출
uniconv photo.jpg -e faces
uniconv photo.jpg -e faces@mediapipe -- --min-confidence 0.8
uniconv photo.jpg -e text -o result.json
uniconv video.mp4 -e audio
uniconv video.mp4 -e scenes@ffmpeg
uniconv ./photos -e "sunset at beach"          # 의미 기반 검색
uniconv ./photos -e similar:reference.jpg      # 유사 이미지 검색
uniconv ./photos -e duplicates                 # 중복 찾기
uniconv receipt.jpg -e data -o receipt.json    # 구조화 파싱

# Load: 적재
uniconv photo.jpg -l gdrive -- --folder /photos
uniconv video.mp4 -l s3 -- --bucket my-bucket
uniconv photo.jpg -l notion -- --database images

# 조합 (ETL 파이프라인)
uniconv video.mp4 -e highlights -t gif
uniconv photo.heic -t jpg -l gdrive
uniconv video.mp4 -e highlights -t gif -l gdrive
```

---

## 3. 플러그인 시스템

### 3.1 설계 원칙

- **하나의 플러그인 = 하나의 ETL 타입**
- **하나의 플러그인 → 여러 타겟 지원 가능**
- **여러 플러그인 → 같은 타겟 지원 가능**
- **같은 그룹명 공유 가능** (예: ffmpeg.transform, ffmpeg.extract)

### 3.2 플러그인 식별

```
<그룹명>.<etl타입>
```

| 식별자 | 그룹명 | ETL |
|--------|--------|-----|
| `ffmpeg.transform` | ffmpeg | transform |
| `ffmpeg.extract` | ffmpeg | extract |
| `ai-vision.extract` | ai-vision | extract |
| `gdrive.load` | gdrive | load |

사용 시에는 ETL 옵션(`-t`, `-e`, `-l`)이 컨텍스트를 결정하므로 그룹명만 사용:

```bash
uniconv video.mov -t mp4@ffmpeg      # → ffmpeg.transform
uniconv video.mov -e audio@ffmpeg    # → ffmpeg.extract
```

### 3.3 플러그인 타입

#### Native 플러그인 (C/C++)

성능 크리티컬한 경우:

```cpp
// plugin.h
extern "C" {
    PluginInfo* uniconv_plugin_info();
    Result* uniconv_plugin_execute(Request* req);
    void uniconv_plugin_free(void* ptr);
}
```

```cpp
struct PluginInfo {
    const char* name;           // "ffmpeg"
    ETLType etl;                // ETL_TRANSFORM
    const char** targets;       // ["mp4", "webm", "gif", ...]
    const char* version;
};
```

#### CLI 플러그인 (언어 무관)

개발 편의성 우선:

```json
{
  "name": "face-extractor",
  "etl": "extract",
  "targets": ["faces"],
  "executable": "face-extractor",
  "interface": "cli"
}
```

아무 언어로 개발 가능:

```python
#!/usr/bin/env python3
# face-extractor (Python)
import sys, json, argparse

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--min-confidence', type=float, default=0.5)
    args = parser.parse_args()

    # 처리 로직
    results = extract_faces(args.input, args.min_confidence)
    save_results(results, args.output)

    # JSON 결과 stdout 출력
    print(json.dumps({"success": True, "count": len(results)}))

if __name__ == '__main__':
    main()
```

### 3.4 플러그인 매니페스트

```json
{
  "name": "ai-vision",
  "version": "1.2.0",
  "etl": "extract",
  "targets": ["faces", "text", "objects", "labels"],
  "interface": "cli",
  "executable": "ai-vision-extract",
  "options": [
    {"name": "--confidence", "type": "float", "default": 0.5},
    {"name": "--language", "type": "string", "default": "auto"}
  ]
}
```

### 3.5 플러그인 설치 및 관리

```bash
# 개별 설치
uniconv plugin install ffmpeg.transform
uniconv plugin install ffmpeg.extract

# 그룹 전체 설치
uniconv plugin install ffmpeg

# 온라인에서 검색/설치
uniconv plugin search face
uniconv plugin install ai-vision

# 목록 조회
uniconv plugins

# 특정 타겟 지원 플러그인 조회
uniconv plugins --target faces

# 그룹 상세 조회
uniconv plugins --group ffmpeg

# 기본 플러그인 설정
uniconv config set transform.jpg vips
uniconv config set extract.faces mediapipe
```

### 3.6 플러그인 조회 예시

```bash
$ uniconv plugins

NAME                    ETL         TARGETS                         VERSION
image-core.transform    transform   jpg,png,webp,heic               0.1.0 (built-in)
ffmpeg.transform        transform   mp4,webm,mkv,gif,mp3,wav        0.1.0 (built-in)
ffmpeg.extract          extract     audio,frames,thumbnail          0.1.0 (built-in)
ai-vision.extract       extract     faces,text,objects,labels       1.2.0
gdrive.load             load        gdrive                          0.5.0

$ uniconv plugins --target faces

TARGET    PLUGIN                  VERSION   DEFAULT
faces     ai-vision.extract       1.2.0     ✓
faces     face-yolo.extract       2.0.0
faces     face-mediapipe.extract  1.1.0

$ uniconv plugins --group ffmpeg

GROUP       ffmpeg
PLUGINS
  ffmpeg.transform    mp4, webm, mkv, avi, gif, mp3, wav
  ffmpeg.extract      audio, frames, thumbnail, subtitle
```

### 3.7 플러그인 충돌 해결

같은 타겟을 여러 플러그인이 지원하는 경우:

```bash
# 기본값 미설정 + interactive
uniconv photo.jpg -e faces
# → "어떤 플러그인을 사용할까요?"
# → [1] ai-vision (recommended)
# → [2] face-yolo
# → [3] face-mediapipe

# 기본값 미설정 + non-interactive
uniconv photo.jpg -e faces --no-interactive
# → 경고 출력 후 첫 번째 사용
# ⚠ Multiple plugins support 'faces'. Using 'ai-vision'.
#   Use -e faces@mediapipe to specify.

# 명시적 지정
uniconv photo.jpg -e faces@mediapipe

# 기본값 설정
uniconv config set extract.faces mediapipe
```

---

## 4. CLI 설계

### 4.1 명령어 구조

첫 번째 인자 해석:
- **예약어(키워드)** → 해당 명령 실행
- **파일/경로/URL** → ETL 모드

### 4.2 Core 옵션 vs 플러그인 옵션

**Core 옵션 (공통):**
```
-o, --output       출력 경로
-f, --force        덮어쓰기
-q, --quality      품질 (transform 공통)
-w, --width        너비 (transform 공통)
-h, --height       높이 (transform 공통)
--json             JSON 출력
--quiet            조용히
--verbose          상세 로그
--dry-run          실제 실행 안 함
--no-interactive   interactive 모드 끄기
--preset           프리셋 사용
--watch            폴더 감시 모드
```

**플러그인 옵션 (-- 이후):**
```bash
# face-extractor 플러그인 옵션
uniconv photo.jpg -e faces -- --min-size 50 --confidence 0.9

# ffmpeg 플러그인 옵션
uniconv video.mov -t mp4@ffmpeg -- --crf 23 --preset slow

# 자주 쓰는 옵션은 core에서 프록시
uniconv photo.heic -t jpg -q 85    # -q는 core가 플러그인으로 전달
```

### 4.3 명령어 목록

#### ETL 동작

```bash
# Transform
uniconv photo.heic -t jpg
uniconv photo.heic -t jpg -q 90 -w 1920
uniconv photo.heic -t jpg --preset insta
uniconv photo.jpg -t png --remove-bg           # 플러그인 옵션
uniconv video.mov -t mp4 --target-size 25MB
uniconv video.mov -t gif@ffmpeg -- --fps 15

# Extract
uniconv photo.jpg -e faces -o ./faces/
uniconv photo.jpg -e text -o result.txt
uniconv video.mp4 -e audio -o audio.mp3
uniconv video.mp4 -e scenes -o ./scenes/
uniconv video.mp4 -e highlights -- --duration 2m
uniconv receipt.jpg -e data -o receipt.json
uniconv ./photos -e "beach sunset"
uniconv ./photos -e similar:ref.jpg
uniconv ./photos -e duplicates

# Load
uniconv photo.jpg -l gdrive -- --folder /photos
uniconv video.mp4 -l s3 -- --bucket my-bucket

# 조합
uniconv video.mp4 -e highlights -t gif -o highlight.gif
uniconv photo.heic -t jpg -l gdrive
```

#### Interactive 모드

```bash
# ETL 옵션 없으면 interactive 진입
uniconv photo.heic

# 명시적 interactive
uniconv photo.heic -i

# interactive 끄기
uniconv photo.heic --no-interactive
```

#### 조회 명령어

```bash
uniconv info <file>              # 파일 상세 정보
uniconv formats                  # 지원 포맷 목록
uniconv presets                  # 프리셋 목록
uniconv plugins                  # 플러그인 목록
uniconv plugins --target <t>     # 특정 타겟 지원 플러그인
uniconv plugins --group <g>      # 특정 그룹 플러그인
```

#### 관리 명령어

```bash
# 프리셋 관리
uniconv preset create <name> [options]
uniconv preset delete <name>
uniconv preset show <name>
uniconv preset export <name> <file>
uniconv preset import <file>

# 플러그인 관리
uniconv plugin list [--installed | --available]
uniconv plugin search <keyword>
uniconv plugin install <name|url>
uniconv plugin remove <name>
uniconv plugin update [name | --all]
uniconv plugin info <name>

# 설정
uniconv config get <key>
uniconv config set <key> <value>
uniconv config list
```

### 4.4 Interactive 모드

**단일 파일:**
```
$ uniconv photo.heic

Detected: HEIC image (2.4 MB, 4032x3024)

What to do?
  [T] Transform (convert format)
  [E] Extract (faces, text, ...)
  [I] Info (show details)

> t

Convert to:
  [1] jpg  (recommended)
  [2] png
  [3] webp
  [4] pdf

> 1

Quality [85]: 90
Output: photo.jpg

Converting... done! (2.4 MB → 824 KB)
```

**디렉토리:**
```
$ uniconv ./photos

Detected: 127 files (94 HEIC, 28 PNG, 5 MOV)

What to do?
  [T] Transform all
  [E] Extract (search, find duplicates, ...)
  [I] Info

> t

Convert to:
  [1] jpg (images only)
  [2] mp4 (videos only)
  [3] By type (images→jpg, videos→mp4)

> 3
```

### 4.5 JSON 출력

모든 명령어는 `--json` 플래그로 JSON 출력 지원:

```bash
$ uniconv photo.heic -t jpg --json
```
```json
{
  "success": true,
  "action": "transform",
  "input": "photo.heic",
  "output": "photo.jpg",
  "plugin": "image-core.transform",
  "input_size": 2456789,
  "output_size": 824123,
  "size_ratio": 0.335
}
```

```bash
$ uniconv photo.jpg -e faces --json
```
```json
{
  "success": true,
  "action": "extract",
  "target": "faces",
  "plugin": "ai-vision.extract",
  "input": "photo.jpg",
  "results": [
    {"file": "face_001.jpg", "confidence": 0.98, "bounds": [100, 100, 200, 200]},
    {"file": "face_002.jpg", "confidence": 0.95, "bounds": [300, 150, 180, 180]}
  ]
}
```

```bash
$ uniconv ./photos -e "beach sunset" --json
```
```json
{
  "success": true,
  "action": "extract",
  "target": "semantic_search",
  "query": "beach sunset",
  "results": [
    {"file": "IMG_2847.jpg", "score": 0.92},
    {"file": "vacation/day3.jpg", "score": 0.87}
  ]
}
```

```bash
$ uniconv plugins --json
```
```json
{
  "plugins": [
    {
      "id": "ffmpeg.transform",
      "group": "ffmpeg",
      "etl": "transform",
      "version": "0.1.0",
      "builtin": true,
      "targets": ["mp4", "webm", "mkv", "gif", "mp3"]
    },
    {
      "id": "ffmpeg.extract",
      "group": "ffmpeg",
      "etl": "extract",
      "version": "0.1.0",
      "builtin": true,
      "targets": ["audio", "frames", "thumbnail"]
    }
  ]
}
```

---

## 5. 지원 포맷 및 타겟

### 5.1 내장 (Built-in)

#### Transform 타겟

**이미지:**
| Input | Output |
|-------|--------|
| HEIC, HEIF | JPG, PNG, WebP, PDF |
| JPG, JPEG | PNG, WebP, PDF |
| PNG | JPG, WebP, PDF |
| WebP | JPG, PNG, PDF |
| GIF | JPG, PNG, WebP |
| BMP, TIFF | JPG, PNG, WebP, PDF |

**비디오:**
| Input | Output |
|-------|--------|
| MP4, MOV, MKV, AVI, WebM | MP4, WebM, MKV, GIF, MP3 |

**오디오:**
| Input | Output |
|-------|--------|
| MP3, WAV, M4A, OGG, FLAC | MP3, WAV, OGG, M4A |

#### Extract 타겟

| 타겟 | 설명 |
|------|------|
| audio | 비디오에서 오디오 추출 |
| frames | 비디오에서 프레임 추출 |
| thumbnail | 비디오 썸네일 |

### 5.2 플러그인 예시

#### Transform 플러그인

- **pdf-tools.transform**: PDF 병합, 분할, 압축
- **hwp.transform**: HWP ↔ PDF/DOCX
- **office.transform**: DOCX, XLSX, PPTX 변환
- **cad.transform**: DWG, DXF 변환
- **3d.transform**: STL, OBJ, FBX, GLTF 변환
- **raw.transform**: RAW (CR2, NEF, ARW) 변환
- **ai-image.transform**: 배경 제거, 업스케일, 보정

#### Extract 플러그인

**이미지 분석:**
- **ai-vision.extract**: faces, text, objects, labels
- **ocr.extract**: text, tables, forms
- **document.extract**: receipt, invoice, business-card, resume

**비디오 분석:**
- **video-ai.extract**: scenes, highlights, summary
- **speech.extract**: transcript, chapters
- **face-tracker.extract**: person (특정 인물 추적)

**검색/분류:**
- **semantic.extract**: 의미 검색 ("beach sunset")
- **similarity.extract**: similar (유사 이미지)
- **dedup.extract**: duplicates (중복 찾기)

**오디오 분석:**
- **audio-ai.extract**: stems (보컬/악기 분리)
- **transcribe.extract**: transcript, minutes

#### Load 플러그인

- **gdrive.load**: Google Drive
- **s3.load**: AWS S3
- **dropbox.load**: Dropbox
- **notion.load**: Notion
- **slack.load**: Slack

---

## 6. 플랫폼별 컨텍스트 메뉴

### 6.1 macOS

- Quick Action (Automator workflow) 설치
- `~/Library/Services/` 에 workflow 복사
- "Convert with uniconv" 메뉴 추가

```bash
uniconv --install-context-menu
```

### 6.2 Windows

- 레지스트리 등록
- `HKEY_CLASSES_ROOT\*\shell\uniconv`

```powershell
uniconv --install-context-menu
```

### 6.3 Linux

- Nautilus scripts (GNOME)
- Dolphin service menus (KDE)

```bash
uniconv --install-context-menu
```

### 6.4 동작 방식

컨텍스트 메뉴 클릭 시:
1. uniconv CLI interactive 모드 실행, 또는
2. uniconv GUI (별도 앱) 실행

---

## 7. 프리셋 시스템

### 7.1 프리셋 저장

```bash
uniconv preset create insta -t jpg -w 1080 -q 85
uniconv preset create thumbnail -t jpg -w 200 -h 200
uniconv preset create extract-faces -e faces -- --confidence 0.9
```

### 7.2 프리셋 사용

```bash
uniconv photo.heic --preset insta
uniconv *.heic --preset thumbnail
uniconv photo.jpg --preset extract-faces
```

### 7.3 프리셋 저장 위치

```
~/.uniconv/presets/
├── insta.json
├── thumbnail.json
└── extract-faces.json
```

### 7.4 프리셋 포맷

```json
{
  "name": "insta",
  "description": "Instagram optimized",
  "etl": "transform",
  "target": "jpg",
  "core_options": {
    "width": 1080,
    "quality": 85
  },
  "plugin_options": {}
}
```

---

## 8. Watch 모드

```bash
# 기본
uniconv ./incoming -t jpg --watch

# 프리셋과 함께
uniconv ./incoming --preset insta --watch

# 출력 경로 지정
uniconv ./incoming -t jpg --watch -o ./processed

# ETL 파이프라인
uniconv ./incoming -t jpg -l gdrive --watch
```

---

## 9. 기술 스택

### 9.1 Core

- **언어**: C++20
- **빌드**: CMake
- **CLI 파싱**: CLI11
- **JSON**: nlohmann/json
- **이미지**: libvips (+ libheif for HEIC)
- **비디오/오디오**: FFmpeg (libav*)

### 9.2 플러그인

- **Native**: C ABI (.so, .dylib, .dll)
- **CLI 기반**: 언어 무관 (Python, Go, Rust, Node.js, ...)

### 9.3 크로스 플랫폼 빌드

- **CI**: GitHub Actions
- **macOS**: Clang + Homebrew
- **Windows**: MSVC + vcpkg
- **Linux**: GCC + apt

---

## 10. 코딩 컨벤션

### 10.1 네이밍 규칙

| 종류 | 스타일 | 예시 |
|------|--------|------|
| 파일명 | snake_case | `plugin_manager.cpp` |
| 헤더 (C++) | .hpp | `plugin_manager.hpp` |
| 헤더 (C ABI) | .h | `plugin.h` |
| 클래스/구조체 | PascalCase | `PluginManager` |
| 함수 | snake_case | `load_plugin()` |
| 변수 | snake_case | `file_path` |
| 멤버 변수 | snake_case + `_` | `plugins_`, `config_` |
| 상수 | kPascalCase | `kDefaultQuality` |
| 매크로 | UPPER_SNAKE | `UNICONV_VERSION` |
| 네임스페이스 | snake_case | `uniconv::core` |
| 인터페이스 | I + PascalCase | `IConverter`, `IPlugin` |
| 열거형 | PascalCase | `ETLType::Transform` |
| 타입 별칭 | PascalCase | `using FilePath = ...` |

### 10.2 코드 예시

```cpp
namespace uniconv::core {

// ETL 타입
enum class ETLType {
    Transform,
    Extract,
    Load
};

// 플러그인 정보
struct PluginInfo {
    std::string name;
    ETLType etl;
    std::vector<std::string> targets;
    std::string version;
    bool builtin;
};

// 플러그인 인터페이스
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual Result execute(const Request& req) = 0;
};

// 플러그인 매니저
class PluginManager {
public:
    void load_plugins();
    IPlugin* find_plugin(ETLType etl, const std::string& target) const;
    std::vector<IPlugin*> find_plugins_for_target(const std::string& target) const;

private:
    std::vector<std::unique_ptr<IPlugin>> plugins_;
    std::map<std::string, std::string> default_plugins_;  // target → plugin
};

} // namespace uniconv::core
```

### 10.3 C++20 기능 활용

```cpp
// Concepts
template<typename T>
concept Plugin = requires(T t, const Request& req) {
    { t.info() } -> std::same_as<PluginInfo>;
    { t.execute(req) } -> std::same_as<Result>;
};

// Ranges
auto transform_plugins = plugins_
    | std::views::filter([](const auto& p) {
        return p->info().etl == ETLType::Transform;
      });

// std::format
auto msg = std::format("Using plugin {} for target {}", plugin_id, target);

// Designated initializers
PluginInfo info{
    .name = "ffmpeg",
    .etl = ETLType::Transform,
    .targets = {"mp4", "webm", "gif"},
    .version = "0.1.0"
};
```

---

## 11. 디렉토리 구조

```
uniconv/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── cli/
│   │   ├── parser.cpp
│   │   ├── interactive.cpp
│   │   └── commands/
│   │       ├── etl.cpp           # transform, extract, load 처리
│   │       ├── info.cpp
│   │       ├── preset.cpp
│   │       └── plugin.cpp
│   ├── core/
│   │   ├── engine.cpp
│   │   ├── plugin_manager.cpp
│   │   ├── plugin_loader_native.cpp
│   │   ├── plugin_loader_cli.cpp
│   │   ├── preset_manager.cpp
│   │   └── watcher.cpp
│   ├── plugins/                  # built-in 플러그인
│   │   ├── image_transform.cpp
│   │   ├── ffmpeg_transform.cpp
│   │   └── ffmpeg_extract.cpp
│   └── utils/
│       ├── file_utils.cpp
│       └── json_output.cpp
├── include/
│   └── uniconv/
│       ├── plugin.h              # 플러그인 C ABI
│       ├── types.hpp
│       └── ...
├── plugins/
│   └── examples/                 # 예제 플러그인
│       ├── python/
│       ├── go/
│       └── rust/
├── scripts/
│   ├── macos/
│   ├── windows/
│   └── linux/
└── tests/
```

---

## 12. 향후 확장

### 12.1 GUI 앱

- 컨텍스트 메뉴에서 호출되는 옵션 선택 UI
- 플랫폼별 네이티브 (SwiftUI, WinUI, GTK)
- CLI core 호출

### 12.2 플러그인 레지스트리

- 중앙 플러그인 저장소
- `uniconv plugin search <keyword>`
- 버전 관리, 의존성 해결

### 12.3 Python/Node SDK

- 플러그인 개발용 SDK
- 보일러플레이트 생성기

```bash
uniconv plugin init --lang python my-plugin
```

---

## 13. 마일스톤

### Phase 1: MVP
- [ ] 기본 CLI 구조 (ETL 옵션)
- [ ] 이미지 변환 (HEIC, JPG, PNG, WebP)
- [ ] JSON 출력
- [ ] 프리셋 기본 기능

### Phase 2: 플러그인 시스템
- [ ] Native 플러그인 로더
- [ ] CLI 플러그인 로더
- [ ] 플러그인 관리 명령어
- [ ] 기본 플러그인 설정

### Phase 3: 확장 기능
- [ ] 비디오 변환 (ffmpeg)
- [ ] Interactive 모드
- [ ] Watch 모드

### Phase 4: 플랫폼 통합
- [ ] macOS 컨텍스트 메뉴
- [ ] Windows 컨텍스트 메뉴
- [ ] Linux 컨텍스트 메뉴

### Phase 5: AI 및 생태계
- [ ] Extract 플러그인 (faces, text, ...)
- [ ] Load 플러그인 (gdrive, s3, ...)
- [ ] 플러그인 레지스트리
- [ ] 플러그인 SDK