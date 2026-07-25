# AI_RE Local Baseline Test Guide

이 문서는 Backend와 WebApp 초기 기준선을 로컬 PC와 같은 LAN의 휴대폰에서 확인하는 공용 절차입니다.

## 1. 초기 기준선 정의

현재 상태는 **기능 MVP 완료본이 아니라 이후 기능 구현의 초기 개발 기준선**입니다.

### 포함 범위

- FastAPI 애플리케이션 골격
- `GET /health`, `GET /api/v1/system/capabilities`
- GameClient Bootstrap 등록과 Pairing 기반 영속 Device 인증
- 타입 기반 `AIService` Port와 Mock 구현
- OpenAPI, JSON Schema, Fixture 계약
- Backend 테스트와 Python 의존성 잠금 파일
- Vite와 Strict TypeScript 기반 모바일 WebApp
- QR fragment 및 직접 입력 기반 WebClient Pairing과 브라우저 자격 증명 복구
- Backend 연결 상태, 대화/기억 탭, 로컬 메시지 입력 UI
- 인증된 Mock Offline Chat과 SQLite 대화 저장
- SQLite 기반 Device 조회·폐기와 폐기 Token 차단
- 표준 오류 Envelope, 요청 추적과 민감정보 비기록 로그
- WebApp 의존성 잠금 파일과 프로덕션 빌드 설정

### 미포함 범위

- 실제 Local LLM 답변
- 장기 기억 조회, 저장, 삭제
- 실제 Local LLM Runtime 연동
- C PC의 WebApp 정적 호스팅 구성
- Unreal HTTP 통신과 StateTree 명령 연동

WebApp에서 메시지를 전송하면 인증된 `POST /api/v1/chat` 요청을 보내고 Mock AI 답변을 표시합니다. 동일 `request_id` 재전송은 저장된 응답을 반환하며 다른 내용으로 ID를 재사용하면 거부됩니다.

## 2. 최초 1회 준비

저장소 루트에서 Backend 개발 의존성을 설치합니다.

```powershell
uv --directory Backend sync --dev
```

WebApp 의존성을 설치합니다.

```powershell
cd WebApp
npm.cmd install
cd ..
```

`Backend/.venv/`, `WebApp/node_modules/`, `WebApp/dist/`는 Git에 포함하지 않습니다. `Backend/uv.lock`과 `WebApp/package-lock.json`은 재현 가능한 환경을 위해 Git에 포함합니다.

## 3. 로컬 PC 테스트

### 3.1 Backend 실행

첫 번째 PowerShell에서 저장소 루트를 작업 경로로 사용합니다.

```powershell
uv --directory Backend run alembic upgrade head
uv --directory Backend run uvicorn app.main:app --host 0.0.0.0 --port 8000
```

브라우저에서 다음 주소를 엽니다.

```text
http://127.0.0.1:8000/health
```

정상 응답 예시는 다음과 같습니다.

```json
{
  "service": "aire-backend",
  "status": "ok",
  "schema_version": 1,
  "ai_mode": "mock"
}
```

현재 Health 테스트에는 `.env` 또는 LLM Runtime이 필요하지 않습니다. 이후 보호 API를 테스트할 때는 `Backend/.env.example`을 `Backend/.env`로 복사하고 GameClient와 WebClient에 서로 다른 개발용 토큰을 설정합니다. 실제 토큰이 들어 있는 `.env`는 Git에 커밋하지 않습니다.

`alembic upgrade head`는 최초 실행과 schema 변경 후 다시 적용합니다.

### 3.2 WebApp 실행

두 번째 PowerShell에서 실행합니다.

먼저 Swagger UI에서 GameClient를 등록하고 Pairing Code를 발급합니다. WebApp은
`#/pair/{8자리 Pairing Code}` fragment를 열면 자동 Pairing하며, 화면에서 Code를
직접 입력할 수도 있습니다. Pairing 성공 후 발급된 WebClient 자격 증명은
`aire.web_device_credentials.v1` 브라우저 저장 키에 보관됩니다.

```powershell
cd WebApp
npm.cmd run dev
```

직접 입력 fallback을 사용할 때는 브라우저에서 다음 주소를 엽니다.

```text
http://127.0.0.1:5173
```

WebApp 개발 서버는 `/health`와 `/api` 요청을 로컬 Backend의 `http://127.0.0.1:8000`으로 전달합니다. Pairing 또는 저장 Token 복구가 완료되면 WebApp 헤더에 `C PC 인증됨`이 표시됩니다.

QR 링크와 동일한 자동 Pairing 흐름은 다음 형식으로 확인합니다.

```text
http://127.0.0.1:5173/#/pair/12345678
```

실제 발급된 8자리 Code를 사용하며, WebApp이 Code를 읽은 직후 주소창에서
fragment가 제거되는지 확인합니다. Device Token을 URL이나 Vite 환경 변수에
넣지 않습니다.

### 3.3 화면 동작 확인

다음 항목을 확인합니다.

- 대화 탭과 기억 탭이 서로 전환됩니다.
- 추천 대화를 누르면 입력란에 문구가 들어갑니다.
- 직접 작성한 메시지를 전송하면 대화 목록에 추가됩니다.
- 전송 중 입력과 전송 버튼이 잠겨 중복 클릭이 차단됩니다.
- Mock AI 답변이 동료 메시지로 추가됩니다.
- 인증 또는 AI 오류가 빈 성공 대사가 아니라 오류 코드와 함께 표시됩니다.
- Backend를 종료하고 연결 새로고침 버튼을 누르면 연결 실패 상태가 표시됩니다.
- Backend를 다시 실행하고 새로고침하면 연결 상태가 복구됩니다.
- Pairing 후 새로고침해도 Code 입력 없이 `/api/v1/devices/me`로 인증이 복구됩니다.
- 연결 해제 성공 후 기존 Token으로 Chat을 호출할 수 없습니다.
- 만료, 사용 완료, 잘못된 Pairing Code가 서로 다른 안내로 표시됩니다.

## 4. 휴대폰 LAN 테스트

PC와 휴대폰을 같은 Wi-Fi 또는 LAN에 연결합니다. PC의 IPv4 주소를 확인합니다.

```powershell
ipconfig
```

PC 주소가 `192.168.0.10`이라면 휴대폰 브라우저에서 다음 주소로 접속합니다.

```text
http://192.168.0.10:5173
```

Windows 방화벽 확인 창이 나타나면 개발 중인 개인 네트워크에 한해 Node.js 접근을 허용합니다. 공용 네트워크에서는 임의로 포트를 공개하지 않습니다.

휴대폰에서는 다음 항목을 추가로 확인합니다.

- 화면이 가로로 넘치지 않습니다.
- 상단 헤더와 하단 입력 영역이 정상적으로 보입니다.
- 소프트 키보드를 열어 메시지를 입력하고 전송할 수 있습니다.
- 화면 잠금과 복귀 후 탭과 입력 UI가 계속 동작합니다.
- Pairing 링크, Offline Chat, 새로고침, Backend 재시작, 재연결과 연결 해제를 순서대로 확인합니다.

## 5. 정적 검증

Backend 테스트를 실행합니다.

```powershell
uv --directory Backend run pytest
```

WebApp 타입 검사와 프로덕션 빌드를 실행합니다.

```powershell
cd WebApp
npm.cmd run typecheck
npm.cmd run build
```

현재 초기 기준선의 기대 결과는 다음과 같습니다.

- Backend: 테스트 전체 통과
- WebApp: TypeScript 오류 없음
- WebApp: Vite 프로덕션 빌드 성공
- 브라우저: 대화/기억 탭과 메시지 입력 동작
- 브라우저: Console Error 없음
- 브라우저: Pairing fragment 제거와 저장 Token 자동 복구

Unreal Engine 프로젝트 빌드는 이 절차에 포함하지 않습니다. Unreal 변경 검증은 Unreal 담당자가 Editor 또는 Rider 환경에서 별도로 수행합니다.

## 6. 종료

Backend와 WebApp을 실행한 각 PowerShell에서 `Ctrl+C`를 눌러 개발 서버를 종료합니다.

## 7. 문제 해결

### WebApp에 Backend 연결 실패가 표시되는 경우

1. `http://127.0.0.1:8000/health`가 열리는지 확인합니다.
2. Backend가 `8000` 포트에서 실행 중인지 확인합니다.
3. WebApp 개발 서버를 다시 실행합니다.

### 휴대폰에서 WebApp이 열리지 않는 경우

1. PC와 휴대폰이 같은 네트워크인지 확인합니다.
2. 휴대폰에서 `127.0.0.1`이 아니라 PC의 IPv4 주소를 사용합니다.
3. Windows 방화벽의 개인 네트워크 허용 여부를 확인합니다.
4. 공유기의 AP Isolation 또는 게스트 네트워크 격리 여부를 확인합니다.

### 포트가 이미 사용 중인 경우

기존 Backend 또는 Vite 프로세스를 먼저 종료합니다. 임의의 다른 포트로 변경하면 Vite Proxy와 접속 주소도 함께 바꿔야 하므로 팀 기본 테스트에서는 `8000`과 `5173`을 유지합니다.
