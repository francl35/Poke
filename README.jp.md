# Poke

**[English](README.md) | [日本語](README.jp.md)**


**Poke** は、PSP 用ゲーム **『モンスターハンターポータブル 2nd G』**（MHP2G、海外版タイトル：*Monster Hunter Freedom Unite*）を対象とした、実験的なネイティブ再コンパイルプロジェクトです。

Poke は **[TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo) を基にした派生プロジェクト**です。PSP ランタイム、再コンパイルの仕組み、ホスト側実装の大部分は、Yakumo の原作者および貢献者による成果を基盤としています。**Poke は Yakumo のオリジナルプロジェクトではなく、Yakumo チームが保守するものでも、公式な後継プロジェクトでもありません。** オリジナルプロジェクトのリリース、開発、サポートについては、上流の Yakumo を参照してください。元の成果に対する権利と功績は、それぞれの作者および貢献者に帰属します。

このフォークでは、Yakumo の技術を MHP2G に適用することに重点を置いています。リポジトリには上流から引き継いだ MHP3rd プロファイルも残っていますが、これは Poke の MHP2G ビルドが Yakumo 公式のリリースであることを意味しません。

## 現在の開発状況

**開発中 — 一般向けの正式リリースではありません。**

- MHP2G の実行ファイルを解析し、主要なコードをネイティブ C++ として生成しています。
- Linux の Vulkan ビルドで起動に成功し、村でのプレイを確認しています。
- Apple Silicon macOS の Vulkan/MoltenVK ビルドでも、GPU 頂点デコードと、確認した自宅シーンでの 60 FPS フレーム補間を含むゲームプレイを検証しています。
- クエスト、戦闘、エリア移動、オーバーレイの互換性については、引き続き検証中です。
- MHP2G の CMake ターゲット名と実行ファイル名は、どちらも `Poke` です。
- 一部のゲストコードはインタープリターで実行される場合があり、すべての処理が事前コンパイル（AOT）で実行されるわけではありません。

上記は開発中のテスト結果であり、ゲーム全体の完全な互換性や、各プラットフォームへの対応を保証するものではありません。

## ゲームファイルと法的注意事項

**このリポジトリには、著作権で保護されたゲームコンテンツは含まれていません。** 『モンスターハンターポータブル 2nd G』の適法に入手したご自身のゲームから、必要なファイルを用意してください。ゲームディスクのイメージ、復号済み実行ファイル、抽出したアセット、ゲームから生成した C++ コード、コンパイル済みのゲーム用オーバーレイは配布しておらず、リポジトリにコミットしてはいけません。

Poke は非公式のファン制作による互換性プロジェクトであり、株式会社カプコン、ソニー、またはその関連会社との提携、承認、推奨、後援関係はありません。『モンスターハンター』、『モンスターハンターポータブル 2nd G』、*Monster Hunter Freedom Unite*、PSP、および関連する名称やアセットの権利は、それぞれの権利者に帰属します。ご自身のゲームファイルの準備と使用にあたっては、適用される法令を遵守してください。

## Linux での MHP2G ビルド（開発用）

必要なものには、C++20 対応の開発環境、CMake、Python 3、SDL3、Vulkan の開発用ファイル、シェーダーのコンパイルツールなどがあります。MHP2G のレンダラービルドでは、メディア処理に FFmpeg も使用します。その他の依存関係については、CMake の設定および既存のビルドドキュメントを参照してください。

開発ツールとレンダラーを設定・ビルドします。

```bash
/usr/bin/cmake -S . -B out/mhp2g-renderer -G "Unix Makefiles" \
  -DPSPRECOMP_PROFILE=mhp2g -DMHP2G_RENDERER=ON
/usr/bin/cmake --build out/mhp2g-renderer --target Poke -j"$(nproc)"
```

**ご自身で用意した復号済みの MHP2G `EBOOT.ELF`** から、メインコードをローカルで事前コンパイル（AOT）用に生成します。

```bash
profiles/mhp2g/scripts/generate.sh \
  "$PWD/out/mhp2g-renderer" \
  "/path/to/your/EBOOT.ELF"
```

コード生成後、実行ファイルを再ビルドします。

```bash
/usr/bin/cmake --build out/mhp2g-renderer --target Poke -j"$(nproc)"
```

ローカルのゲームファイルが入ったディレクトリを指定して実行します。

```bash
./out/mhp2g-renderer/bin/Poke "/path/to/your/game-directory"
```

**オーバーレイの生成については、Poke 単体で動作する手順として、まだ検証・文書化が完了していません。** プロファイルにはオーバーレイのビルドスクリプトが含まれていますが、ゲームイメージのパスや出力先の前提条件を現在調整しています。対応が完了するまでは、一部のコードがインタープリター経由で実行される場合があります。生成したゲームコードやオーバーレイライブラリを、Git で追跡されるソースディレクトリにコピーしたり、コミットしたりしないでください。

## macOS（Apple Silicon）でのビルド — 実験的対応

Apple Silicon Mac（macOS 15.7.7）で、Vulkan レンダラーを有効にした Poke のビルドと動作を確認しています。

**ご自身で用意した復号済みの MHP2G `EBOOT.ELF`** から、メインコードをローカルで事前コンパイル（AOT）用に生成します。特別のgenerate_macスクリプトを使用してください。

```bash
profiles/mhp2g/scripts/generate_mac.sh \
  "$PWD/out/mhp2g-renderer" \
  "/path/to/your/EBOOT.ELF"
```

レンダラーは **SDL3 + Vulkan + MoltenVK** を使用します。MoltenVK が Vulkan コマンドを Apple の Metal グラフィックス API に変換します。

### 依存関係

Homebrew を用意し、必要なグラフィックス関連パッケージをインストールします。

```bash
brew install sdl3 molten-vk vulkan-loader vulkan-headers glslang
```

動作確認時の構成は、SDL3 3.4.18、MoltenVK 1.4.2、Vulkan 1.4.363.0、glslang 16.6.0 です。

新規ビルドには、C++ コンパイラー、CMake、Python、PSP 再コンパイル用ツールなど、その他のビルド依存関係も必要です。

### レンダラーの設定

Poke リポジトリのルートで実行します。

```bash
cmake -S . -B out/mhp2g-renderer \
  -DPSPRECOMP_PROFILE=mhp2g \
  -DMHP2G_RENDERER=ON \
  -DSDL3_DIR=/opt/homebrew/opt/sdl3/lib/cmake/SDL3 \
  -DVulkan_INCLUDE_DIR=/opt/homebrew/opt/vulkan-headers/include \
  -DVulkan_LIBRARY=/opt/homebrew/opt/vulkan-loader/lib/libvulkan.dylib \
  -DMHP2G_GLSLANG=/opt/homebrew/bin/glslangValidator
```

CMake の出力に次の表示があることを確認してください。

```text
mhp2g: Vulkan renderer enabled
```

**注意：** `MHP2G_RENDERER=ON` の指定だけでは不十分です。SDL3、Vulkan、glslang が見つからない場合、CMake はレンダラーを無効にし、画面表示のない実行ファイルをビルドすることがあります。

### ビルド

```bash
cmake --build out/mhp2g-renderer --target Poke -j 4
```

### ゲームファイルの準備

日本版『モンスターハンターポータブル 2nd G』のご自身のゲームから用意したファイルが必要です。次の構成で配置します。

```text
mhp2g-test-game/
├── disc.iso
└── EBOOT.ELF
```

`disc.iso` は日本版のゲームディスクイメージです。`EBOOT.ELF` は、復号済みの PSP MIPS 実行ファイルである必要があります。

復号済みの `PSP_GAME/SYSDIR/BOOT.BIN` が有効な MIPS ELF であれば、`EBOOT.ELF` として使用できます。

次のコマンドで形式を確認できます。

```bash
file mhp2g-test-game/EBOOT.ELF
```

動作確認に使用した実行ファイルは、32 ビット・リトルエンディアンの MIPS ELF として認識されました。

ゲームファイルをリポジトリにコミットしたり、再配布したりしないでください。

### メインゲームコードの生成

ゲームファイルの配置だけでは、ネイティブコードは生成されません。新規チェックアウトでは、復号済みの `EBOOT.ELF` からメインの AOT 用 C++ コードを生成し、その後に再ビルドする必要があります。共通の生成コマンドは、上記の Linux 向け開発手順に記載しています。ただし、生成スクリプトを macOS 単独の環境で実行する手順は、ここではまだ検証していません。対応するメインコードをすでにローカルで生成している場合、以下のレンダラー修正のために再生成する必要はありません。

### 起動

Apple Silicon の開発用 Mac では、次の GPU 頂点デコード構成で動作を確認しています。

```bash
MHP2G_GPU_DECODE=1 \
./out/mhp2g-renderer/bin/Poke "$PWD/mhp2g-test-game"
```

レンダラーは Vulkan/MoltenVK 経由で初期化されます。GPU 頂点デコードは明示的に有効化する機能です。既定のデコード経路と比較する場合は、`MHP2G_GPU_DECODE` を設定せずに起動してください。

macOS ではシステムのヒラギノ角ゴシック（Hiragino Sans）を使用できるため、Linux 向けの `MHP2G_FONT=/run/host/...` は不要です。

### フレームレートと確認済みの描画修正

ゲームのシミュレーションは 30 FPS で動作します。Poke で 60 FPS を選択し、フレーム補間が有効になると、中間フレームを描画する経路が使用されます。ゲームのシミュレーション自体が 60 FPS に変わるわけではありません。

GPU 頂点デコードとフレーム補間を試す場合は、次の両方の修正を含むリビジョンを使用してください。

- **HUD 頂点のアラインメント：** GPU デコード用の生頂点データに続く展開済み頂点のアラインメントを修正し、GPU 頂点デコード有効時に HUD が断続的に消える問題を解消しました。
- **補間用カメラ移動の推定：** スキニング対象のキャラクター描画をカメラ移動の推定サンプルから除外しました。キャラクターの対応付けとアニメーション補間は引き続き有効です。これにより、確認した自宅シーンで、60 FPS 時に移動すると背景が揺れる問題が解消しました。

問題を切り分ける際は、まず 30 FPS を選択し、同じ移動を 60 FPS と比較してください。カメラ推定の修正後、自宅シーンは 60 FPS で正常に動作することを確認しました。その他のシーンやカメラ動作については、引き続き検証が必要です。

### トラブルシューティング

**`Renderer: not built` と表示される**

CMake が `mhp2g: Vulkan renderer enabled` と表示したことを確認し、`Poke` ターゲットを再ビルドしてください。

**SDL3 のアーキテクチャやリンクに関する問題**

CMake が、古い `/usr/local` 側のインストールではなく、`/opt/homebrew/opt/sdl3/lib/cmake/SDL3` にある ARM64 用 Homebrew SDL3 を使用していることを確認してください。

**`disc.iso not found` または `Missing EBOOT.ELF` と表示される**

シンボリックリンクの参照先を確認してください。

```bash
ls -lhL mhp2g-test-game/
```

相対パスのリンク先は、リンクが置かれたディレクトリを基準に解決されます。絶対パスでリンクを作成すると、参照先の混同を避けられます。

**GPU 頂点デコード有効時に HUD がちらつく、または消える**

`profiles/mhp2g/host/gpu/vulkan_renderer.cpp` に、展開済み頂点のアラインメント修正が含まれていることを確認してください。GPU 頂点デコードを無効にすると以前の不具合が表面化しない場合がありますが、この修正により、確認済みの GPU デコード経路を有効なまま使用できます。

**60 FPS で移動すると背景が揺れる**

`profiles/mhp2g/host/gpu/frame_interpolation.cpp` のカメラ推定用サンプリングループに、スキニング対象の描画を除外する修正が含まれていることを確認してください。30 FPS は比較や一時的な回避策として利用できます。確認した自宅シーンの揺れは、カメラ推定の修正で解消しました。

**診断を有効にすると動作が遅くなる、または不具合の出方が変わる**

`MHP2G_TRACE_SPRITES` を設定すると、`MHP2G_GPU_DECODE=1` を指定していても GPU 頂点デコードが無効になります。このトレースを使って、通常の GPU デコード時の挙動を判断しないでください。通常プレイでは診断用の環境変数を解除してください。一部のスイッチは変数が存在するかどうかだけを判定するため、値を `0` にしても有効になる場合があります。調査中に使用した一時的な HUD トレースやバッファ記録用のパッチは、どちらの描画修正にも不要です。

**オーバーレイライブラリが見つからない**

ゲーム用オーバーレイは別途ビルドします。レンダラーが動作していても、すべてのゲーム内容やクエストに対応しているとは限りません。

### 現在の状況

Apple Silicon でネイティブ実行ファイルと Vulkan レンダラーをビルドし、実際のゲームプレイで動作を確認しています。GPU 頂点デコード時の HUD のちらつきと、60 FPS で自宅内を移動した際の背景の揺れは、上記の修正で解消しました。ただし、ゲーム全体の互換性、新規チェックアウトからの macOS 用オーバーレイ生成手順、Intel Mac 対応、その他の Mac 構成での互換性を保証するものではありません。

## プロジェクト構成

- `profiles/mhp2g/` — MHP2G 用のホストコード、ツール、シェーダー、スクリプト、テスト。
- `profiles/mhp3rd/` — 上流プロジェクトから引き継いだ MHP3rd プロファイル。
- `include/psprecomp/`、`src/`、`tools/` — 共通の PSP ランタイムと再コンパイル基盤。
- `docs/` — プロジェクトのドキュメント。一部には上流の MHP3rd に関する記述が残っているため、検証済みの MHP2G 向け手順として扱わないでください。

## クレジットとライセンス

Poke は **[TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo)** を基にしています。Yakumo は独立したプロジェクトであり、このフォークの保守者は Yakumo の開発者や貢献者を代表する立場にはありません。オリジナルのランタイム、ツール、実装に取り組んだ Yakumo チームおよび上流のすべての貢献者に感謝します。

上流プロジェクトは **MIT ライセンス**で配布されています。`LICENSE` および該当するソースファイルに記載された著作権表示と許諾表示を保持してください。サードパーティ製コンポーネントには個別のライセンスや注意事項が適用される場合があります。付属するライセンスファイルを確認してください。**ソフトウェアのライセンスは、カプコンやソニーのゲームコンテンツを配布する権利を付与するものではありません。**

上流 Yakumo のドキュメントやリリースについては [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo) を、このフォークについては [francl35/Poke](https://github.com/francl35/Poke) を参照してください。
