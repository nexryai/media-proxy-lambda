# media-proxy-lambda
WIP: AWS Lambdaに最適化されたMediaProxyです。

 - C++で書かれています。依存関係を全てHardeningした上でStaticなバイナリにコンパイルしています。
 - AWS Lambda以外での環境での動作には対応していません。
 - SVGには意図的に対応していません。SVGは数々のセキュリティ上の問題を引き起こす上に、対応に必要となる依存関係が膨大です。
 - HEICには法的な問題を避けるために、意図的に対応していません。AVIFには対応しています。
