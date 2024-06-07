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

# 利用ガイド
TBW  

# ~~テスト済み環境(outdated)~~
- ~~NetBSD/luna68k + mlterm~~
- ~~NetBSD/x68k + mlterm~~
- ~~NetBSD/sun3 + mlterm~~
- ~~ArchLinux + xfce terminal~~
- ~~WSL1 + mintty(wsl-terminal)~~
- ~~WSL2 + mintty(wsltty)~~
- ~~WSL1 + Windows Terminal~~

# Thanks
- septag : sjson.h (https://github.com/septag/sjson)
