# CameraGuard 0.3.0

Windows 11 と macOS 13 以降向けのPCカメラ録画アプリ。アプリ本体は C++17 と Qt 6 で実装しています。

## GitHub Actionsでビルド

1. このディレクトリの内容を、隠しディレクトリの `.github` も含めて GitHub リポジトリのルートへ置き、プッシュします。
2. **Actions → Build CameraGuard** を開きます。プッシュで自動実行され、既定ブランチにワークフローがあれば **Run workflow** からも実行できます。
3. 実行が成功したら **Artifacts** から、自分の環境に合う末尾が `-installer` の項目をダウンロードします。

| 環境 | Artifactsの名前 | ZIPを展開して開くファイル |
| --- | --- | --- |
| Windows 11 x64 | CameraGuard-Windows-x64-installer | CameraGuard-Windows-x64-Setup.exe |
| Mac Apple Silicon | CameraGuard-macOS-arm64-installer | CameraGuard-macOS-arm64.dmg |
| Intel Mac | CameraGuard-macOS-x86_64-installer | CameraGuard-macOS-x86_64.dmg |

- **Windows**ではセットアップEXEを実行し、スタートメニューから CameraGuard を起動します。既定のインストール先は現在のユーザーの `%LOCALAPPDATA%\Programs\CameraGuard` です。アンインストールは Windows の「インストールされているアプリ」から行えます。録画と設定はインストール先と別に保存されます。
- **Mac**ではDMGを開き、`CameraGuard.app` を `Applications` にドラッグします。Appleの公証は未実施で、アプリはアドホック署名付きです。
- WindowsのセットアップEXEの発行元署名は未実施です。配布前にコード署名と同梱ライブラリのライセンス条件を確認してください。
- 末尾が `-installer` でない項目からは従来のポータブルZIPを取得できます。

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
