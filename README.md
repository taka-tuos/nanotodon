# nanotodon
CLI Mastodon Client

# 注意
現在かなり大規模に開発中です。安定利用はできない可能性があります

# 依存ライブラリ
- cURL
- pthread

# ビルド
## pkgsrc環境
```CFLAGS="-I/usr/pkg/include" LDFLAGS="-L/usr/pkg/lib -Wl,-R/usr/pkg/lib" make```

## OpenBSD
```CFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib" make```

## その他
```make```

# オプション
- ```-mono```  
  - 色付け無効化(太字のみ有効)(for 1bpp framebuffer)

- ```-unlock```  
  - 公開範囲が PRIVATE/DIRECT の投稿を表示する

- ```-noemoji```  
  - ReblogやFavourite、公開範囲表示などのUI要素に絵文字を利用しない

- ```-profile <name>```  
  - プロファイル ``<name>`` を利用する

- ```-timeline <public|local|home>```  
  - (WIP)流すタイムラインの選択

- ```-tllimit <num>```  
  - (WIP)起動時にrest APIで取得するトゥート数の指定(デフォルトは 20)

# 投稿方法
1. TLが流れているときにEnterを押す
2. ``> ``とプロンプトが出るのでToot内容を入力(``\n``と``\\``が利用できます)
3. Enterを押すと投稿
- Toot入力中はTLの更新がブロックされます
- 以前のVerにあった``/private``等も利用できます

# 詳しいガイド
TBW  

# 動いてるっぽい環境(24/12/07)
- WSL2 + VSCode Terminal(w/ Sixel)
- WSL2 + Windows Terminal
- OpenBSD/amd64
- NetBSD/i386 10.0 (w/ Sixel)
- NetBSD/evbppc 10.0 (on Nintendo Wii)
- NetBSD/amd64 10.0 (w/ Sixel)
- NetBSD/evbarm 10.0 (w/ Sixel)
- NetBSD/luna68k 10.0 (w/ Sixel)
- NetBSD/vax 10.0 (w/ Sixel)
- NetBSD/mac68k 10.0 (w/ Sixel)
- NetBSD/hp300 10.0 (w/ Sixel)
- NetBSD/sun3 10.0 (w/ Sixel)

# ~~テスト済み環境(0.1.x-0.3.x)~~
- ~~NetBSD/luna68k + mlterm~~
- ~~NetBSD/x68k + mlterm~~
- ~~NetBSD/sun3 + mlterm~~
- ~~ArchLinux + xfce terminal~~
- ~~WSL1 + mintty(wsl-terminal)~~
- ~~WSL2 + mintty(wsltty)~~
- ~~WSL1 + Windows Terminal~~

# Thanks
- septag : sjson.h (https://github.com/septag/sjson)
