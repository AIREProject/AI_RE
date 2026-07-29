# TRAIP AI : RE HTML Presentation

`index.html`을 브라우저로 열면 바로 실행되는 정적 발표 페이지입니다.

## 조작

- `←`, `↑`, `PageUp`, `Backspace`: 이전 슬라이드
- `→`, `↓`, `PageDown`, `Space`: 다음 슬라이드
- `Home`, `End`: 첫 장 / 마지막 장
- `M`: 목차 열기 / 닫기
- `Esc`: 목차 닫기
- 화면 좌우 스와이프: 모바일 슬라이드 이동

인터넷 연결이나 별도 빌드 과정이 필요하지 않습니다.

## 10페이지 프론트 웹 임베드

10페이지는 실행 중인 `WebApp`을 `iframe`으로 표시합니다. 인증된 채팅을 임베드에서도 사용하려면 발표 페이지를 파일로 직접 열지 말고, WebApp과 같은 주소에서 여세요.

```powershell
cd C:\workspace\Github\AI_RE\AI_RE\WebApp
npm.cmd run dev
```

발표 페이지:

```text
http://localhost:5173/presentation/index.html
```

같은 출처에서 열면 새 창 WebApp과 임베드 WebApp이 동일한 브라우저 인증 정보를 사용합니다.
로컬 `index.html`을 직접 열면 위 주소로 자동 이동합니다. 새 창에서 Pairing을 완료한 뒤 임베드가 갱신되지 않으면 10페이지의 `새로고침` 버튼을 누르세요.

다른 주소나 포트를 사용할 때는 발표 페이지 URL에 `webapp` 쿼리를 추가합니다.

```text
index.html?webapp=http://localhost:4173/
```
