# nanotodon
CLI Mastodon Client

# WARN
現在かなり大規模に開発中です。安定利用はできない可能性があります

# Depended library
- cURL
- pthread

# Build
## *BSD with pkgsrc
```CFLAGS="-I/usr/pkg/include" LDFLAGS="-L/usr/pkg/lib -Wl,-R/usr/pkg/lib" make```

## Else
```make```

# Options
- ```-mono```  
  - Disable Color (for 1bpp framebuffer)

- ```-unlock```  
  - Show PRIVATE/DIRECT toots.

- ```-noemoji```  
  - Disable emojis in UI.

- ```-profile <name>```  
  - Use profile <name>.

- ```-timeline <public|local|home>```  
  - Select timeline(WIP, streaming on local/public may not work).

# Guide
TBW  

# ~~Tested environments(outdated)~~
- ~~NetBSD/luna68k + mlterm~~
- ~~NetBSD/x68k + mlterm~~
- ~~NetBSD/sun3 + mlterm~~
- ~~ArchLinux + xfce terminal~~
- ~~WSL1 + mintty(wsl-terminal)~~
- ~~WSL2 + mintty(wsltty)~~
- ~~WSL1 + Windows Terminal~~

# Thanks
- septag : author of sjson.h (https://github.com/septag/sjson)
