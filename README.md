# media-proxy-lambda
WIP: AWS Lambdaに最適化されたMediaProxyです。

 - C++で書かれています。依存関係を全てHardeningした上でStaticなバイナリにコンパイルしています。
 - 必要最低限の依存関係のみを含める方針で開発されています。
 - AWS Lambda以外での環境での動作には対応していません。
 - SVGには意図的に対応していません。SVGは数々のセキュリティ上の問題を引き起こす上に、対応に必要となる依存関係が膨大です。
 - HEICには法的な問題を避けるために、意図的に対応していません。AVIFには対応しています。

## arm64 bootstrapのビルド

`.devcontainer`で定義された開発環境から、次のCMakeインターフェースで
strip・静的ELF検証済みの成果物を生成します。

```console
cmake --preset arm64-release
cmake --build --preset arm64-release --target bootstrap-artifact --parallel 2
ctest --preset arm64-release
```

成果物は`out/build/arm64-release/artifact/bootstrap`です。AWSリソースの
デプロイ、IaC、コスト管理はこのリポジトリの対象外です。

ASan/UBSan診断はproduction成果物と分離した動的musl PIEで実行します。

```sh
cmake --preset arm64-sanitizers
cmake --build --preset arm64-sanitizers --parallel 2
ctest --preset arm64-sanitizers
```
