# nanotodon
CLI Mastodon Client

# WARN
現在かなり大規模に開発中です。安定利用はできない可能性があります

# Depended library
- cURL
- pthread

# How to build
## If you're using *BSD with pkgsrc
```make -f Makefile.bsd```

## Else
```make```


# Options

- ```-mono```  
 - Draw as monochrome mode.

- ```-unlock```  
- Show UNLISTED/PRIVATE/DIRECT toots.

- ```-noemoji```  
- Remove emojis from UI.

- ```-profile <name>```  
- Use profile <name>.

- ```-timeline <public|local|home>```  
- Select timeline(WIP, streaming on local/public may not work).

# Tips
## How to UNLISTED toot
```/unlisted <your funny toot here>```

## How to PRIVATE toot
```/private <your funny toot here>```

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
