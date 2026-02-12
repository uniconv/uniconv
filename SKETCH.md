# uniconv - Universal Converter & Content Intelligence Tool

## 프로젝트 요약

컨텍스트 메뉴 통합과 AI 기반 콘텐츠 이해 기능을 갖춘 크로스 플랫폼 파일 변환 도구.
"우클릭 한 방"의 편리함과 파워유저를 위한 CLI 확장성을 동시에 제공.

**단순 포맷 변환을 넘어서:**

- 콘텐츠를 **변환**하고 (Transform)
- 원하는 것을 **추출**하고 (Extract)
- 결과를 **적재**하는 (Load)

**ETL 개념 기반 파이프라인**으로 복잡한 작업도 간단하게.

---

## 1. 핵심 가치

### 1.1 차별화 포인트 (vs ImageMagick, FFmpeg)

| 기능               | ImageMagick/FFmpeg | uniconv                           |
| ------------------ | ------------------ | --------------------------------- |
| 파이프라인 문법    | ❌                 | ✅ `"source \| target \| target"` |
| 프리셋 시스템      | ❌                 | ✅ 저장/불러오기                |
| Watch 모드         | ❌                 | ✅ 폴더 감시 자동 변환          |
| JSON 출력          | ❌                 | ✅ LLM/자동화 친화적            |
| 타겟 용량 지정     | ❌                 | ✅ `--target-size 25MB`         |
| AI 이미지 처리     | ❌                 | ✅ 배경 제거, 자동 보정 등      |
| 컨텍스트 메뉴 통합 | 수동 스크립트      | ✅ 자동 설치                    |
| Interactive 모드   | ❌                 | ✅ 초보자 친화적                |
| 콘텐츠 추출        | ❌                 | ✅ 얼굴, 텍스트, 표, 장면 등    |
| 의미 기반 검색     | ❌                 | ✅ "해변 사진 찾아줘"           |
| 구조화 파싱        | ❌                 | ✅ 영수증, 명함 → JSON          |
| 플러그인 시스템    | ❌                 | ✅ 다중 언어 지원               |

### 1.2 타겟 사용자

- **일반 사용자**: 컨텍스트 메뉴로 간편 변환
- **파워 유저**: CLI 파이프라인 + 프리셋 + 자동화
- **개발자**: JSON 출력, 플러그인 개발 (다중 언어)
- **LLM/Agent**: 구조화된 I/O

---

## 2. 파이프라인 문법

### 2.1 기본 구조

```bash
uniconv [core옵션] <source> "[scope/plugin:]<target>[.ext] [옵션] | ..."
```

**중요: 파이프라인(타겟 체인)은 반드시 따옴표로 감싸야 함** (쉘 `|` 연산자와 충돌 방지)

- `|` = 스테이지 구분
- `,` = 같은 스테이지 내 병렬 요소
- `tee` = 다음 스테이지 요소 개수만큼 복제하는 builtin (fan-out)
- `collect` = 분기된 결과를 하나로 모으는 builtin (fan-in)
- `clipboard` = 결과를 시스템 클립보드에 복사
- `_` (passthrough) = 입력을 그대로 전달

### 2.2 기본 예시

```bash
# 단일 변환
uniconv photo.heic "jpg"
uniconv photo.heic "jpg --quality 90"
uniconv video.mov "mp4"

# 이미지 필터
uniconv photo.jpg "grayscale"
uniconv photo.jpg "ascii --width 80"

# 문서 변환
uniconv document.docx "pdf"
uniconv photo.jpg "pdf | docx"    # 다단계: 이미지 → PDF → DOCX

# 비디오 변환
uniconv video.mp4 "gif --width 320 --fps 15"
uniconv video.mp4 "mp4 --height 720"

# 클립보드로 복사
uniconv photo.jpg "png | clipboard"

# 출력 경로 지정
uniconv -o output.jpg photo.heic "jpg --quality 85"

# stdin/파이프 입력
echo "hello" | uniconv - "translate | txt"
cat data.csv | uniconv - "json"
uniconv - --from-clipboard "png"
```

### 2.3 플러그인 지정

```bash
# scope/plugin:target 형식으로 명시적 지정 (같은 타겟을 여러 플러그인이 지원할 때)
uniconv photo.heic "uniconv/image-convert:jpg --quality 90"
uniconv video.mov "uniconv/video-convert:mp4 --crf 23"

# 출력 확장자가 타겟과 다를 때 .ext로 지정
uniconv data.postgis "geo/postgis:extract.geojson"
uniconv data.postgis "geo/postgis:extract.csv"
```

### 2.4 tee를 이용한 분기

`tee`는 다음 스테이지 요소 개수만큼 현재 결과를 복제.

```bash
# 하나를 여러 포맷으로
uniconv photo.heic "tee | jpg, png, webp"

#   photo.heic → tee (3개 복제)
#                  → [0] jpg
#                  → [1] png
#                  → [2] webp

# 변환 후 분기
uniconv photo.heic "jpg | tee | grayscale, invert"

#   photo.heic → jpg → tee (2개 복제)
#                        → [0] grayscale
#                        → [1] invert
```

### 2.5 분기 후 각각 다른 처리

분기된 각 요소는 독립적인 파이프라인을 가질 수 있음.

```bash
uniconv photo.heic "tee | jpg --quality 90, png, webp --quality 80"

#   photo.heic → tee (3개 복제)
#                  → [0] jpg --quality 90
#                  → [1] png
#                  → [2] webp --quality 80

# 분기 후 계속 진행
uniconv photo.jpg "tee | grayscale | clipboard, png"

#   photo.jpg → tee (2개 복제)
#                 → [0] grayscale → clipboard
#                 → [1] png (파일 저장)
```

### 2.6 스테이지 요소 개수 규칙

| 현재 스테이지   | 다음 스테이지   | 가능 여부     |
| --------------- | --------------- | ------------- |
| 1개             | 1개             | ✅            |
| 1개 (tee)       | N개             | ✅ (fan-out)  |
| N개             | N개             | ✅ (1:1 매핑) |
| N개             | 1개 (collect)   | ✅ (fan-in)   |
| N개             | M개 (N≠M)       | ❌            |

```bash
# ✅ 1 → 1 → 1
uniconv photo.heic "jpg | grayscale"

# ✅ 1 → tee → 3 (fan-out)
uniconv photo.heic "tee | jpg, png, webp"

# ✅ 1 → 1 → tee → 2
uniconv photo.heic "jpg | tee | grayscale, invert"

# ✅ N → 1 (fan-in via collect)
uniconv photo.heic "tee | jpg, png, webp | collect"

# ❌ 1 → 2 (tee 없이 늘어남)
uniconv photo.heic "jpg, png"

# ❌ 2 → 3 (개수 불일치)
uniconv photo.heic "tee | jpg, png | grayscale, invert, negative"
```

### 2.7 Interactive 모드

```bash
# 파이프라인 없으면 interactive 진입
uniconv photo.heic

# 명시적 interactive
uniconv --interactive photo.heic
```

---

## 3. 타겟과 데이터 타입

### 3.1 타겟 기반 플러그인 해석

플러그인은 ETL 타입으로 분류하지 않고, **타겟 이름**과 **입력 포맷 매칭**으로 해석됨.
플러그인은 지원하는 `targets`와 `accepts`를 매니페스트에 선언.
`PluginResolver`가 이를 기반으로 최적의 플러그인을 자동 선택.

**플러그인 식별자 문법:** `[scope/plugin:]target[.extension]`

| 부분 | 필수 | 설명 |
| ---- | ---- | ---- |
| `scope/plugin` | 선택 | 네임스페이스/플러그인명 (명시적 지정) |
| `target` | 필수 | 작업 이름 |
| `.extension` | 선택 | 출력 포맷 (생략 시 target과 동일) |

**플러그인 해석 우선순위:**
1. 명시적 지정 (`scope/plugin:target`)
2. 기본 플러그인 (`config set default.<target>`)
3. 입력 포맷 + 타겟 매칭
4. 타겟 이름만 매칭

### 3.2 Builtin 타겟

| 타겟              | 설명                                        |
| ----------------- | ------------------------------------------- |
| `tee`             | 다음 스테이지로 분기 (입력을 N개로 복제)    |
| `collect`         | 분기 결과를 하나로 모음 (N → 1 fan-in)      |
| `clipboard`       | 결과를 시스템 클립보드에 복사/읽기          |
| `_` (passthrough) | 입력을 그대로 전달 (분기 시 일부만 처리)    |

**clipboard 동작:**
- 이미지/텍스트 포맷: 콘텐츠를 클립보드에 직접 복사
- 기타 포맷: 파일 경로를 클립보드에 복사
- 기본적으로 파일 미생성 (`--save` 또는 `-o`로 파일 저장)
- 클립보드 입력 지원: `uniconv - --from-clipboard "png"` (클립보드 → 파일)

**collect 동작:**
- 분기된 N개 결과를 단일 임시 디렉토리에 수집
- 스테이지의 유일한 요소여야 함
- 첫 번째 스테이지에는 올 수 없음

**passthrough 별칭:** `_`, `echo`, `bypass`, `pass`, `noop`

### 3.3 현재 사용 가능한 플러그인

| 플러그인         | 타입           | 지원 타겟                              |
| ---------------- | -------------- | -------------------------------------- |
| `image-convert`  | Native (C++)   | jpg, png, webp, gif, heic, bmp, tiff   |
| `video-convert`  | Native (C++)   | mp4, mov, avi, webm, mkv, gif          |
| `doc-convert`    | CLI (Python)   | pdf, docx, odt, xlsx, pptx, md, html   |
| `ascii`          | CLI (Python)   | ascii, ascii-art, text-art             |
| `image-filter`   | CLI (Python)   | grayscale, invert, negative            |

**설치:**
```bash
uniconv plugin install +essentials    # 모든 기본 플러그인
uniconv plugin install image-convert  # 개별 플러그인
```

---

## 4. 플러그인 시스템

### 4.1 설계 원칙

- **하나의 플러그인 → 여러 타겟 지원 가능**
- **여러 플러그인 → 같은 타겟 지원 가능**
- **타겟/포맷 기반 해석**: 플러그인이 선언한 `targets`와 `accepts`로 파이프라인 호환성 검사
- **지연 로딩 (on-demand loading)**: 플러그인은 필요할 때만 로드 (매니페스트만 먼저 스캔)
- **Sink 플러그인**: `"sink": true`로 선언된 플러그인은 출력 파일을 생성하지 않는 터미널 스테이지 (업로드, 저장 등)

### 4.2 플러그인 식별

플러그인은 `scope/name` 형태로 식별. 파이프라인에서 `scope/plugin:target[.ext]` 문법으로 명시적 지정.

```bash
# 파이프라인에서 scope/plugin:target으로 명시적 지정
uniconv photo.heic "uniconv/image-convert:jpg --quality 90"
uniconv video.mov "uniconv/video-convert:mp4"

# 출력 확장자 명시
uniconv data.postgis "geo/postgis:extract.geojson"
```

### 4.3 플러그인 타입

#### Native 플러그인 (C/C++)

성능 크리티컬한 경우. `include/uniconv/plugin_api.h` 헤더 사용 (API v3):

```cpp
// plugin_api.h
extern "C" {
    UniconvPluginInfo* uniconv_plugin_info();
    void uniconv_plugin_init();                    // optional
    UniconvResult* uniconv_plugin_execute(UniconvRequest* req);
    void uniconv_plugin_free_result(UniconvResult* result);
}
```

```cpp
struct UniconvPluginInfo {
    const char* name;               // "image-convert"
    const char* scope;              // "uniconv"
    const char** targets;           // ["jpg", "png", "webp", ...]
    int target_count;
    const char** input_formats;     // ["jpg", "heic", ...] (accepts)
    int input_format_count;
    const char* version;
    const char* description;
};

struct UniconvRequest {
    const char* input_path;
    const char* output_path;
    const char* target;
    const char* input_format;       // 포맷 힌트 (다단계 파이프라인용)
    UniconvOptionGetter get_option; // 콜백 기반 옵션 접근
    void* option_context;
};

struct UniconvResult {
    UniconvStatus status;           // UNICONV_SUCCESS / UNICONV_ERROR
    const char* output_path;
    size_t output_size;
    const char* error;
    const char* extra_json;         // 추가 메타데이터 (JSON)
};

```

#### CLI 플러그인 (언어 무관)

개발 편의성 우선:

```json
{
  "name": "face-extractor",
  "scope": "ai-vision",
  "targets": {"faces": ["json"]},
  "accepts": ["jpg", "png", "webp"],
  "executable": "face-extractor",
  "interface": "cli"
}
```

CLI 플러그인 호출 규약:

```bash
<executable> --input <path> --output <path> --target <target> [--input-format <fmt>] [플러그인 옵션]
```

**참고:** 다단계 파이프라인에서 중간 파일은 `.tmp` 확장자를 사용. 이때 `--input-format`으로 실제 포맷 힌트가 전달됨.

stdout으로 JSON 결과 출력:

```json
{
  "success": true,
  "output_path": "/path/to/output",
  "metadata": { ... }
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
    parser.add_argument('--target', required=True)
    parser.add_argument('--confidence', type=float, default=0.5)
    args = parser.parse_args()

    # 처리 로직
    results = extract_faces(args.input, args.confidence)
    save_results(results, args.output)

    # JSON 결과 stdout 출력
    print(json.dumps({
        "success": True,
        "output_path": args.output,
        "metadata": {"count": len(results)}
    }))

if __name__ == '__main__':
    main()
```

### 4.4 플러그인 매니페스트

```json
{
  "name": "ai-vision",
  "scope": "ai-vision",
  "version": "1.2.0",
  "description": "AI-powered vision analysis",
  "targets": {
    "faces": ["json"],
    "text": ["json", "txt"],
    "objects": ["json"],
    "labels": ["json"]
  },
  "accepts": ["jpg", "png", "webp", "gif", "bmp"],
  "interface": "cli",
  "executable": "ai-vision-extract",
  "options": [
    {
      "name": "--confidence",
      "type": "float",
      "default": 0.5,
      "min": 0.0,
      "max": 1.0,
      "description": "Detection confidence threshold"
    },
    {
      "name": "--language",
      "type": "string",
      "default": "auto",
      "choices": ["auto", "en", "ko", "ja"],
      "targets": ["text"]
    }
  ],
  "dependencies": [
    { "name": "python3", "type": "system", "version": ">=3.8" },
    { "name": "torch", "type": "python", "version": ">=2.0" },
    { "name": "Pillow", "type": "python" }
  ]
}
```

**매니페스트 필드 설명:**

| 필드 | 타입 | 기본값 | 설명 |
| ---- | ---- | ------ | ---- |
| `targets` | `map` 또는 `array` | — | 지원하는 타겟 → 출력 확장자 매핑. 배열 단축형: `["jpg", "png"]` → `{"jpg": [], "png": []}` |
| `accepts` | `string[]` (선택) | 생략=전체 수용 | 처리 가능한 입력 포맷. 생략 시 모든 입력 허용, `[]` 시 입력 없음 |
| `sink` | `bool` | `false` | `true`면 출력 파일 미생성 터미널 플러그인 (업로드, 저장 등) |
| `scope` | `string` | — | 플러그인 네임스페이스 (파이프라인에서 `scope/plugin:target`으로 참조) |
| `dependencies` | `object[]` | — | 시스템/Python/Node 의존성 (설치 시 자동 관리) |
| `options.targets` | `string[]` | — | 특정 타겟에서만 유효한 옵션 (비어있으면 모든 타겟) |

**targets 형식:**
- **Map**: `{"extract": ["geojson", "csv"], "fgb": []}` — 타겟 → 출력 확장자
- **Array 단축형**: `["jpg", "png", "gif"]` → `{"jpg":[], "png":[], "gif":[]}` (타겟=확장자일 때)
- Map의 첫 번째 확장자가 기본 출력 확장자로 사용됨

### 4.5 플러그인 검색 경로

```
~/.uniconv/plugins/                    # 유저 설치
/usr/local/share/uniconv/plugins/      # 시스템 설치
<executable_dir>/plugins/              # 포터블 (실행파일 옆)
```

**플러그인 지연 로딩 (On-demand Loading):**

플러그인은 시작 시 모두 로드하지 않고, 필요할 때만 로드:

1. **매니페스트 스캔** (lazy): 첫 접근 시 디렉토리 스캔, `plugin.json` 파싱
2. **매칭 로드** (on-demand): `find_plugin()` 호출 시 타겟에 맞는 플러그인만 로드
3. **목록 조회**: 매니페스트만으로 `PluginInfo` 반환 (로드 불필요)

이를 통해 `--help`, `--version` 등은 플러그인 스캔 없이 즉시 실행.

### 4.6 플러그인 설치 및 관리

```bash
# 개별 설치 (레지스트리에서)
uniconv plugin install image-convert
uniconv plugin install image-convert@1.0.16    # 버전 지정

# 컬렉션 설치
uniconv plugin install +essentials             # +이름 또는 collection:이름

# 로컬 설치 (경로 또는 plugin.json)
uniconv plugin install /path/to/plugin-dir

# 온라인에서 검색
uniconv plugin search face

# 플러그인 제거
uniconv plugin remove ai-vision

# 플러그인 업데이트
uniconv plugin update ai-vision
uniconv plugin update                          # 모든 레지스트리 플러그인 업데이트
uniconv plugin update --check                  # 업데이트 확인만 (설치 안 함)
uniconv plugin update +essentials              # 컬렉션 내 플러그인 업데이트

# 목록 조회
uniconv plugin list                            # 설치된 플러그인
uniconv plugin list --registry                 # 레지스트리에서 사용 가능한 플러그인

# 상세 정보
uniconv plugin info image-convert

# 기본 플러그인 설정
uniconv config set default.jpg vips
uniconv config set default.faces mediapipe
```

**의존성 자동 관리:**
플러그인 설치 시 `dependencies`에 명시된 의존성을 자동 처리:
- `system`: 존재 확인, 미설치 시 `install_hint` 제공
- `python`: 플러그인별 격리된 virtualenv에 설치
- `node`: 플러그인별 격리된 node_modules에 설치

### 4.7 플러그인 조회 예시

```bash
$ uniconv plugin list

NAME                     TARGETS                       VERSION   SOURCE
-----------------------------------------------------------------------------
ascii                    ascii,ascii-art,text-art      1.0.7     registry
image-filter             grayscale,gray,bw,invert...   1.0.5     registry
image-convert            jpg,jpeg,png,webp,gif,...     1.0.16    registry
doc-convert              pdf,docx,doc,odt,rtf,...      1.0.6     registry
video-convert            mp4,mov,avi,webm,mkv,...      1.1.11    registry

$ uniconv plugin info image-convert

Name:        image-convert
Scope:       uniconv
Version:     1.0.16
Description: Image format conversion via libvips
Type:        native
Source:      registry
Targets:     jpg, jpeg, png, webp, gif, bmp, tiff, heic, avif
Inputs:      jpg, jpeg, png, webp, gif, bmp, tiff, heic, avif, pdf
```

### 4.8 플러그인 충돌 해결

같은 타겟을 여러 플러그인이 지원하는 경우:

```bash
# 기본값 미설정 + interactive
uniconv "photo.jpg | faces"
# → "어떤 플러그인을 사용할까요?"
# → [1] ai-vision (recommended)
# → [2] face-yolo
# → [3] face-mediapipe

# 기본값 미설정 + non-interactive
uniconv --no-interactive "photo.jpg | faces"
# → 경고 출력 후 첫 번째 사용
# ⚠ Multiple plugins support 'faces'. Using 'ai-vision'.
#   Use mediapipe:faces to specify.

# 명시적 지정
uniconv "photo.jpg | mediapipe:faces"

# 기본값 설정
uniconv config set default.faces mediapipe
```

---

## 5. 플러그인 예시 (향후 확장 포함)

### 5.1 변환 플러그인

- **pdf-tools**: PDF 병합, 분할, 압축
- **hwp**: HWP ↔ PDF/DOCX
- **office**: DOCX, XLSX, PPTX 변환
- **cad**: DWG, DXF 변환
- **3d**: STL, OBJ, FBX, GLTF 변환
- **raw**: RAW (CR2, NEF, ARW) 변환
- **ai-image**: 배경 제거, 업스케일, 보정

### 5.2 추출 플러그인

**이미지 분석:**

- **ai-vision**: faces, text, objects, labels
- **ocr**: text, tables, forms
- **document**: receipt, invoice, business-card, resume

**비디오 분석:**

- **video-ai**: scenes, highlights, summary
- **speech**: transcript, chapters
- **face-tracker**: person (특정 인물 추적)

**검색/분류:**

- **semantic**: 의미 검색 ("beach sunset")
- **similarity**: similar (유사 이미지)
- **dedup**: duplicates (중복 찾기)

**오디오 분석:**

- **audio-ai**: stems (보컬/악기 분리)
- **transcribe**: transcript, minutes

### 5.3 적재 플러그인 (Sink)

적재 플러그인은 `"sink": true`로 선언. 출력 파일을 생성하지 않고 외부로 전송하는 터미널 스테이지.

- **gdrive**: Google Drive 업로드
- **s3**: AWS S3 업로드
- **dropbox**: Dropbox 업로드
- **notion**: Notion 페이지 생성
- **slack**: Slack 메시지 전송

```bash
# 변환 후 업로드 (sink 플러그인)
uniconv photo.heic "jpg | gdrive:upload"
uniconv data.csv "json | s3:upload"
```

---

## 6. CLI 상세

### 6.1 Core 옵션

Core 옵션은 소스 파일 앞에 위치:

```bash
uniconv [core옵션] <source> "<pipeline>"
```

| 옵션                         | 설명                                  |
| ---------------------------- | ------------------------------------- |
| `-o, --output <path>`        | 출력 경로                             |
| `-f, --force`                | 덮어쓰기                              |
| `--json`                     | JSON 출력                             |
| `--quiet`                    | 조용히                                |
| `--verbose`                  | 상세 로그                             |
| `--dry-run`                  | 실제 실행 안 함                       |
| `-r, --recursive`            | 재귀적 디렉토리 처리                  |
| `-p, --preset <n>`           | 프리셋 사용                           |
| `--input-format <fmt>`       | 입력 포맷 수동 지정 (stdin 등)        |
| `--from-clipboard`           | 클립보드에서 입력 읽기 (`-` 소스 필요) |
| `--timeout <seconds>`        | 플러그인 타임아웃 (0 = 무제한)        |

**플러그인별 옵션** (파이프라인 내 타겟 뒤에 위치):
- `--quality`, `--width`, `--height` 등은 각 타겟의 플러그인별 옵션
- 예: `uniconv "photo.heic | jpg --quality 90 --width 1920"`

**출력 경로 결정 우선순위**:
1. 파이프라인 요소 내 `--output` 옵션 (예: `jpg --output out.jpg`)
2. CLI `-o` 옵션 (마지막 스테이지에만 적용):
   - 확장자 없음: 타겟 확장자 추가 (`-o ./temp/output` → `./temp/output.png`)
   - 확장자 있음: 그대로 사용 (`-o ./temp/output.png` → `./temp/output.png`)
   - 참고: tee로 복수 출력 시 확장자가 지정되면 같은 파일에 덮어쓰기됨
3. 기본값: 현재 디렉토리에 입력 파일명 + 타겟 확장자

### 6.2 조회 명령어

```bash
uniconv info <file>                  # 파일 상세 정보
uniconv detect <file>                # 파일 타입 감지 (libmagic 기반)
uniconv formats                      # 지원 포맷 목록
uniconv preset list                  # 프리셋 목록
uniconv plugin list                  # 설치된 플러그인 목록
uniconv plugin list --registry       # 레지스트리 플러그인 목록
uniconv update                       # uniconv 자체 업데이트
uniconv update --check               # 업데이트 확인만
```

### 6.3 관리 명령어

```bash
# 프리셋 관리
uniconv preset create <n> "<pipeline>"
uniconv preset delete <n>
uniconv preset show <n>
uniconv preset list
uniconv preset export <n>              # 프리셋 내보내기
uniconv preset import <file>           # 프리셋 가져오기

# 플러그인 관리
uniconv plugin install <n[@version]>   # 레지스트리 설치 (버전 지정 가능)
uniconv plugin install <path>          # 로컬 설치
uniconv plugin install +<collection>   # 컬렉션 설치
uniconv plugin remove <n>
uniconv plugin update [name]           # 개별 또는 전체 업데이트
uniconv plugin update --check          # 업데이트 확인만
uniconv plugin update +<collection>    # 컬렉션 업데이트
uniconv plugin search <keyword>
uniconv plugin info <n>

# 설정
uniconv config get <key>
uniconv config set <key> <value>
uniconv config list
```

### 6.4 JSON 출력

```bash
$ uniconv --json photo.heic "jpg | grayscale"
```

```json
{
  "success": true,
  "stage_results": [
    {
      "stage_index": 0,
      "target": "jpg",
      "plugin_used": "image-convert",
      "input": "photo.heic",
      "output": "/tmp/uniconv/stage0_elem0_jpg_photo.tmp",
      "status": "success"
    },
    {
      "stage_index": 1,
      "target": "grayscale",
      "plugin_used": "image-filter",
      "input": "/tmp/uniconv/stage0_elem0_jpg_photo.tmp",
      "output": "/path/to/photo_grayscale.jpg",
      "status": "success"
    }
  ],
  "final_outputs": ["/path/to/photo_grayscale.jpg"]
}
```

---

## 7. 프리셋 시스템

### 7.1 프리셋 생성

프리셋 = 저장된 파이프라인

```bash
# 파이프라인을 프리셋으로 저장
uniconv preset create insta "jpg --quality 85 --width 1080"
uniconv preset create multi-format "tee | jpg, png, webp"
uniconv preset create video-gif "gif --width 320 --fps 15"
```

### 7.2 프리셋 사용

```bash
uniconv --preset insta photo.heic
# = uniconv photo.heic "jpg --quality 85 --width 1080"

uniconv --preset multi-format photo.jpg
# = uniconv photo.jpg "tee | jpg, png, webp"

uniconv --preset video-gif video.mp4
# = uniconv video.mp4 "gif --width 320 --fps 15"
```

### 7.3 프리셋 저장 위치

```
~/.uniconv/presets/
├── insta.json
├── backup.json
└── video-gif.json
```

### 7.4 프리셋 포맷

```json
{
  "name": "insta",
  "description": "Instagram optimized",
  "pipeline": "jpg --quality 85 --width 1080"
}
```

---

## 8. Watch 모드

```bash
# 기본: 폴더 감시하며 자동 변환
uniconv watch ./incoming "jpg"

# 프리셋과 함께
uniconv watch --preset insta ./incoming

# 출력 경로 지정
uniconv watch -o ./processed ./incoming "jpg"

# 파이프라인
uniconv watch ./incoming "jpg | grayscale"

# 재귀적 감시
uniconv watch -r ./incoming "jpg"
```

---

## 9. 플랫폼별 컨텍스트 메뉴

### 9.1 macOS

- Quick Action (Automator workflow) 설치
- `~/Library/Services/` 에 workflow 복사

```bash
uniconv --install-context-menu
```

### 9.2 Windows

- 레지스트리 등록
- `HKEY_CLASSES_ROOT\*\shell\uniconv`

```powershell
uniconv --install-context-menu
```

### 9.3 Linux

- Nautilus scripts (GNOME)
- Dolphin service menus (KDE)

```bash
uniconv --install-context-menu
```

### 9.4 동작 방식

컨텍스트 메뉴 클릭 시:

1. uniconv CLI interactive 모드 실행, 또는
2. uniconv GUI (별도 앱) 실행

---

## 10. 기술 스택

### 10.1 Core

- **언어**: C++20
- **빌드**: CMake
- **CLI 파싱**: CLI11 v2.4.2
- **JSON**: nlohmann/json v3.11.3
- **테스트**: Google Test v1.14.0
- **파일 감지**: libmagic (파일 타입 자동 감지)

코어에는 변환 라이브러리(FFmpeg, libvips 등)를 직접 포함하지 않음.
모든 변환 기능은 외부 플러그인으로 제공 (image-convert, video-convert 등).

### 10.2 플러그인

- **Native**: C ABI (.so, .dylib, .dll)
- **CLI 기반**: 언어 무관 (Python, Go, Rust, Node.js, ...)

### 10.3 크로스 플랫폼 빌드

- **CI**: GitHub Actions
- **macOS**: Clang + Homebrew
- **Windows**: MSVC + vcpkg
- **Linux**: GCC + apt

---

## 11. 코딩 컨벤션

### 11.1 네이밍 규칙

| 종류          | 스타일           | 예시                     |
| ------------- | ---------------- | ------------------------ |
| 파일명        | snake_case       | `plugin_manager.cpp`     |
| 헤더          | .h               | `plugin_manager.h`       |
| 클래스/구조체 | PascalCase       | `PluginManager`          |
| 함수          | snake_case       | `load_plugin()`          |
| 변수          | snake_case       | `file_path`              |
| 멤버 변수     | snake_case + `_` | `plugins_`, `config_`    |
| 상수          | kPascalCase      | `kDefaultQuality`        |
| 매크로        | UPPER_SNAKE      | `UNICONV_VERSION`        |
| 네임스페이스  | snake_case       | `uniconv::core`          |
| 인터페이스    | I + PascalCase   | `IPlugin`                |
| 열거형        | PascalCase       | `DataType::Image`        |

### 11.2 코드 예시

```cpp
namespace uniconv::core {

// 데이터 타입 (입출력 분류)
enum class DataType {
    File, Image, Video, Audio, Text, Json, Binary, Stream
};

// 플러그인 정보
struct PluginInfo {
    std::string name;                       // "image-convert"
    std::string id;                         // scope와 동일
    std::string scope;                      // "uniconv"
    std::map<std::string, std::vector<std::string>> targets; // 타겟 → 출력 확장자
    std::optional<std::vector<std::string>> accepts; // nullopt=전체, []=없음, [값]=목록
    std::string version;
    std::string description;
    bool builtin = false;
    bool sink = false;                      // sink 플러그인 (출력 파일 미생성)
};

// 파이프라인 스테이지 요소
struct StageElement {
    std::string target;
    std::optional<std::string> plugin;          // scope/plugin:target 지정 시
    std::optional<std::string> extension;       // .ext 지정 시
    std::map<std::string, std::string> options;
    std::vector<std::string> raw_options;
    // is_tee(), is_collect(), is_clipboard(), is_passthrough() 헬퍼
};

// 파이프라인 스테이지
struct PipelineStage {
    std::vector<StageElement> elements;         // 병렬 요소들
};

// 파이프라인
struct Pipeline {
    std::filesystem::path source;
    std::vector<PipelineStage> stages;
    CoreOptions core_options;
    std::optional<std::string> input_format;    // stdin/generator용 포맷 힌트
};

// 플러그인 해석 컨텍스트
struct ResolutionContext {
    std::string input_format;
    std::string target;
    std::optional<std::string> explicit_plugin;
};

// 플러그인 인터페이스
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual bool supports_target(const std::string& target) const = 0;
    virtual bool supports_input(const std::string& format) const = 0;
    virtual Result execute(const Request& req) = 0;
};

} // namespace uniconv::core
```

---

## 12. 디렉토리 구조

```
uniconv/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── cli/
│   │   ├── parser.cpp/.h              # CLI11 기반 인자 파싱
│   │   ├── pipeline_parser.cpp/.h     # 파이프라인 문법 파싱
│   │   └── commands/
│   │       ├── info_command.cpp/.h
│   │       ├── formats_command.cpp/.h
│   │       ├── preset_command.cpp/.h
│   │       ├── plugin_command.cpp/.h
│   │       ├── config_command.cpp/.h
│   │       ├── update_command.cpp/.h  # uniconv 자체 업데이트
│   │       └── detect_command.cpp/.h  # 파일 타입 감지
│   ├── core/
│   │   ├── types.h                    # 핵심 타입 (DataType, PluginInfo, Request, Result 등)
│   │   ├── pipeline.h                 # Pipeline, PipelineStage, StageElement
│   │   ├── engine.cpp/.h             # ETL 요청 오케스트레이터
│   │   ├── pipeline_executor.cpp/.h  # 파이프라인 실행 엔진
│   │   ├── execution_graph.cpp/.h    # DAG 기반 실행 그래프
│   │   ├── plugin_manager.cpp/.h     # 플러그인 레지스트리 (지연 로딩)
│   │   ├── plugin_manifest.h         # PluginManifest 구조체
│   │   ├── plugin_discovery.cpp/.h   # 파일시스템 플러그인 탐색
│   │   ├── plugin_resolver.cpp/.h    # 타입 기반 플러그인 해석
│   │   ├── plugin_loader_cli.cpp/.h  # CLI 플러그인 로더
│   │   ├── plugin_loader_native.cpp/.h # Native 플러그인 로더
│   │   ├── dependency_installer.cpp/.h # 플러그인 의존성 설치
│   │   ├── dependency_checker.cpp/.h   # 의존성 확인
│   │   ├── installed_plugins.cpp/.h    # 설치 기록 추적
│   │   ├── registry_client.cpp/.h      # 플러그인 레지스트리 클라이언트
│   │   ├── registry_types.h            # 레지스트리 타입 정의
│   │   ├── config_manager.cpp/.h       # 설정 관리
│   │   ├── preset_manager.cpp/.h       # 프리셋 관리
│   │   ├── watcher.cpp/.h             # Watch 모드
│   │   └── output/                    # 출력 추상화
│   │       ├── output.h               # IOutput 인터페이스
│   │       ├── console_output.cpp/.h  # 콘솔 출력 (스피너 포함)
│   │       └── json_output.cpp/.h     # JSON 출력
│   ├── builtins/
│   │   ├── tee.cpp/.h                # fan-out builtin
│   │   ├── collect.cpp/.h            # fan-in builtin
│   │   ├── clipboard.cpp/.h          # 클립보드 입출력
│   │   └── passthrough.cpp/.h        # 패스스루 builtin
│   ├── plugins/
│   │   └── plugin_interface.h        # IPlugin 인터페이스 (순수 가상)
│   └── utils/
│       ├── file_utils.cpp/.h         # 파일 유틸리티
│       ├── string_utils.cpp/.h       # 문자열 유틸리티
│       ├── mime_detector.cpp/.h      # libmagic 기반 타입 감지
│       ├── http_utils.cpp/.h         # HTTP 클라이언트
│       └── version_utils.cpp/.h      # 버전 비교
├── include/
│   └── uniconv/
│       ├── plugin_api.h              # 플러그인 C ABI (API v3)
│       ├── export.h                  # 공유 라이브러리 export 매크로
│       └── version.h.in              # 버전 정보 (CMake 생성)
└── tests/
    └── unit/                         # Google Test 기반 유닛 테스트
```

---

## 13. 향후 확장

### 13.1 GUI 앱

- 컨텍스트 메뉴에서 호출되는 옵션 선택 UI
- 플랫폼별 네이티브 (SwiftUI, WinUI, GTK)
- CLI core 호출
- 파이프라인 시각적 편집기

### 13.2 Python/Node SDK

- 플러그인 개발용 SDK
- 보일러플레이트 생성기

```bash
uniconv plugin init --lang python my-plugin
```

### 13.3 Interactive 모드

- 파이프라인 없이 실행 시 대화형 UI
- 초보자 친화적 가이드

---

## 14. 마일스톤

### Phase 1: MVP ✅

- [x] 파이프라인 파서
- [x] 기본 파이프라인 실행 (단일 스테이지)
- [x] 이미지 변환 (HEIC, JPG, PNG, WebP)
- [x] JSON 출력

### Phase 2: 파이프라인 확장 ✅

- [x] 다중 스테이지 파이프라인
- [x] tee builtin (fan-out 분기)
- [x] collect builtin (fan-in 수집)
- [x] 프리셋 시스템 (export/import 포함)
- [x] clipboard builtin (입출력 양방향)
- [x] passthrough builtin (_)
- [x] `--input-format` 힌트 (다단계 파이프라인용)
- [x] stdin/파이프 입력 지원 (`-` 소스)
- [x] DAG 기반 실행 그래프 (ExecutionGraph)

### Phase 3: 플러그인 시스템 ✅

- [x] Native 플러그인 로더 (C ABI v3)
- [x] CLI 플러그인 로더
- [x] 플러그인 관리 명령어
- [x] 플러그인 레지스트리 (검색, 설치, 업데이트)
- [x] 플러그인 컬렉션 (+essentials)
- [x] Python/Node 의존성 자동 설치 (격리 환경)
- [x] DataType 기반 플러그인 해석 (PluginResolver)
- [x] 플러그인 지연 로딩 (on-demand loading)

### Phase 4: 확장 기능 (진행 중)

- [x] 비디오 변환 (video-convert 플러그인)
- [x] 문서 변환 (doc-convert 플러그인)
- [x] Watch 모드
- [x] 파일 타입 감지 (detect 명령어)
- [x] uniconv 자체 업데이트 (update 명령어)
- [x] 출력 추상화 (IOutput: Console/JSON)
- [ ] Interactive 모드

### Phase 5: 플랫폼 통합

- [ ] macOS 컨텍스트 메뉴
- [ ] Windows 컨텍스트 메뉴
- [ ] Linux 컨텍스트 메뉴

### Phase 6: AI 및 생태계

- [ ] Extract 플러그인 (faces, text, ...)
- [ ] Load 플러그인 (gdrive, s3, ...)
- [ ] 플러그인 SDK
