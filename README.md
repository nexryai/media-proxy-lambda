# media-proxy-lambda
AWS Lambdaに最適化されたMediaProxyです。

 - C++で書かれています。依存関係を全てHardeningした上でStaticなバイナリにコンパイルしています。
 - 必要最低限の依存関係のみを含める方針で開発されています。
 - AWS Lambda以外での環境での動作には対応していません。
 - SVGは専用Rust C ABI shim経由のresvgで静的に描画します。外部リソース、システムフォント、SVGZ、DOCTYPEを無効化し、入力・ノード・埋め込みデータ・キャンバスを制限しています。
 - HEICには法的な問題を避けるために、意図的に対応していません。AVIFには対応しています。

## VS Codeのdevcontainer

初回作成時にx86_64のアプリケーションビルドが自動実行され、完了してから
VS Codeが接続します。このビルドで依存ライブラリと
`out/build/x86_64-release/application/compile_commands.json` が生成されます。
clangdは `.clangd` からこの内側のビルドの設定を読み、muslのsysrootや
依存ライブラリのヘッダーを解決します。外側のCMake superbuildだけを
configureしても、C++ソース用のコンパイル設定は生成されません。

既存のdevcontainerに設定変更を適用するときは、VS Codeで
「Dev Containers: Rebuild Container」を実行してください。ビルド設定を
更新した後に再生成する場合は、コンテナー内で次を実行します。

```console
cmake --preset x86_64-release
cmake --build --preset x86_64-release --target application --parallel 2
```

## arm64 bootstrapのビルド

`.devcontainer`で定義された開発環境から、次のコマンドを実行してビルドします。

```console
cmake --preset arm64-release
cmake --build --preset arm64-release --target bootstrap-artifact --parallel 2
ctest --preset arm64-release
```

成果物は`out/build/arm64-release/artifact/bootstrap`にコンパイルされます。
x86_64にも対応していますが、arm64で動作させることを推奨しています。

LGPL対応ソース、再リンク材料などは
`out/build/arm64-release/artifact/compliance`に生成されます。

ASan/UBSan診断はproduction成果物と分離した動的musl PIEで実行します。

```sh
cmake --preset arm64-sanitizers
cmake --build --preset arm64-sanitizers --parallel 2
ctest --preset arm64-sanitizers
```
