# CameraGuard 0.3.0

Windows 11 と macOS 13 以降向けのPCカメラ録画アプリ。アプリ本体は C++17 と Qt 6 で実装しています。

## GitHub Actionsでビルド

1. このディレクトリの内容を、隠しディレクトリの `.github` も含めて GitHub リポジトリのルートへ置き、プッシュします。
2. **Actions → Build CameraGuard** を開きます。プッシュで自動実行され、既定ブランチにワークフローがあれば **Run workflow** からも実行できます。
3. 実行が成功したら **Artifacts** から環境に合うZIPをダウンロードします。ArtifactsのZIPを展開した中に、配布用のZIPがあります。

| 環境 | 成果物 | 起動するファイル |
| --- | --- | --- |
| Windows 11 x64 | CameraGuard-Windows-x64 | CameraGuard.exe |
| Mac Apple Silicon | CameraGuard-macOS-arm64 | CameraGuard.app |
| Intel Mac | CameraGuard-macOS-x86_64 | CameraGuard.app |

Windows版はZIPを展開し、同梱のDLLとプラグインを同じ位置に置いて起動します。Mac版の試作ビルドはアドホック署名付きで、Developer IDによる署名やAppleの公証は済んでいません。macOSの設定や配布方法によっては起動が制限されます。配布前に署名・公証と同梱ライブラリのライセンス条件を確認してください。

## 監視

- カメラを選んで「監視を開始」を押します。動きを検知した時点から、最後の検知後約6秒まで録画します。「常時録画」にも切り替えられます。
- 映像は音声なしのMP4に保存します。録画は約1分で分割します。カメラとエンコーダーに 640×480・12 fps を指定します。
- ウィンドウの × を押すと、メニューバーまたは通知領域に移動します。録画は続き、画面用のプレビュー更新は停止します。アイコンのメニューで復帰・監視停止・終了ができます。
- 既定の保存期間は7日、容量上限は2 GiBです。監視中に期限切れと容量超過を確認し、古いファイルから削除します。書き込み中のファイルは確定まで保護され、容量が一時的に上限を超える場合があります。
- 保存先はアプリの「録画フォルダーを開く」で確認できます。Windowsの旧版のAVIは旧版の保存フォルダーに残ります。必要な映像は別フォルダーにコピーしてください。
- macOSで最初の監視開始時にカメラ使用を許可します。拒否した場合はシステム設定の「プライバシーとセキュリティ → カメラ」で確認してください。Windowsも同じカテゴリでデスクトップアプリのカメラアクセスを確認できます。
- スリープやサインアウト中は監視が止まります。強制終了後に残った `.part.mp4` は再生できない場合があります。

## 開発

Qt 6.8（Widgets、Multimedia）と CMake 3.22 以上を使用します。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

録画の自動削除テストだけを実行する場合は、構成時に `-DCAMERAGUARD_STORAGE_ONLY=ON` を追加できます。
